#include "AtlasDevFaxOptions.h"
#include "HackManager.h"
#include "fh_constants.h"
#include "common/klib/Asm6502.h"
#include "common/klib/Kstring.h"

#include <array>
#include <cctype>
#include <format>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

// fax options: a pause screen menu with one on/off row per listed AtlasDev
// hack that has a flag. this part is the resident one, in bank 15 or in sram
// with sram=true: the state, the routine that copies it onto the flags, and
// the hooks: three with a save, two without.
//
// the state is one bit per row. with a save (the SRAM hack is in the project)
// it lives at the top of sram: "FXO" and a version, an identity of the row
// list, and eight bytes of row bits. a block that does not match, from a
// fresh cart or another build, turns every row on. the choices are applied
// when a new game starts ($db26) and when the hero spawns after continue or a
// death ($db0a).
//
// without a save the flags themselves hold the choices. every row is turned
// on when the title screen comes up ($da71: power-on and after a reset),
// and kept through deaths, which never pass there. the reset leaves page 1,
// and so the flags, as they were, which is why the title screen does it.
//
// select on the pause screen ($e039) opens the menu. each site runs the
// instructions it displaced first, and each is checked against its vanilla
// bytes before anything is written.
//
// the game code the hack calls is the same in the US, US rev A and EU ROMs,
// but for the three zero page bytes the far call keeps a, x and y in: $de-$e0
// on the US ROM, $e2-$e4 on US rev A and EU. the menu keeps its cursor, row
// and top in the other three. the code is checked before anything is
// written; the JP ROM, where it differs, is refused, and so is a ROM that
// keeps the flags in SRAM.
//
// the menu itself (AtlasDevFaxOptionsMenu.cpp) goes where bank=, addr= and
// sram= put it, bank 9 by default. the pause hook reaches it at the end of
// the resident part: there sits its trampoline, or in bank 15 and in sram
// the menu itself.

namespace {
	constexpr std::size_t MAX_ROWS{ 64 }, MAX_LABEL{ 19 };
	constexpr word MAX_FLAG{ 247 };
	const std::string PREFIX{ "AtlasDev" }, CONTROL{ " CONTROL" };

	constexpr byte BANK15{ 15 }, MENU_BANK{ 9 }, WINDOW_BANK{ 12 };
	constexpr byte SELECT{ 0x20 }, SPAWN_HP{ 0x50 }, VERSION{ 1 };
	constexpr std::size_t BLOCK_SIZE{ 16 }, SIGNATURE_SIZE{ 6 }, STATE_SIZE{ 8 }, ROW_SIZE{ 4 };
	// a row in the rows table
	constexpr int FLAG_OFFSET{ 0 }, FLAG_MASK{ 1 }, STATE_OFFSET{ 2 }, STATE_MASK{ 3 };

	// jsr Sprites_FlipRanges
	constexpr std::array<byte, 3> PAUSE_ORIG{ 0x20, 0xa8, 0xcb };
	// lda #$50 / sta PlayerHP
	constexpr std::array<byte, 5> SPAWN_ORIG{ 0xa9, 0x50, 0x8d, 0x31, 0x04 };
	// jsr Game_ClearTimedItems, which clears the timed items
	constexpr std::array<byte, 3> START_ORIG{ 0x20, 0xaf, 0xc6 };
	// sta CurrentStage, with a = 0
	constexpr std::array<byte, 3> TITLE_ORIG{ 0x8d, 0x35, 0x04 };

	// the far call reloads the saved a, x and y this far in, then jumps to its target
	constexpr word FAR_CALL_RELOAD{ 0x3e };
	constexpr byte JMP{ 0x4c };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK15, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	bool matches(const std::vector<byte>& p_rom, byte p_bank, word p_addr, std::initializer_list<byte> p_bytes) {
		auto off{ klib::Asm6502::get_file_offset(p_bank, p_addr) };
		for (const byte b : p_bytes)
			if (off >= p_rom.size() || p_rom[off++] != b)
				return false;
		return true;
	}

	std::runtime_error unsupported(const std::string& p_name, byte p_bank, word p_addr) {
		return std::runtime_error(std::format(
			"{}: the game code at bank {} ${:04x} differs from the US, US rev A and EU ROMs, the only ones supported",
			p_name, p_bank, p_addr));
	}

	void require_code(const std::vector<byte>& p_rom, byte p_bank, word p_addr, std::initializer_list<byte> p_bytes,
		const std::string& p_name) {
		if (!matches(p_rom, p_bank, p_addr, p_bytes))
			throw unsupported(p_name, p_bank, p_addr);
	}

	// the zero page bytes the far call keeps a, x and y in
	byte far_call_zp(const std::vector<byte>& p_rom, word p_far_call, const std::string& p_name) {
		for (const byte zp : { fh::RAM::ZP_BankedCallSavedA, fh::RAM::ZP_e2 }) {
			const byte x{ static_cast<byte>(zp + 1) }, y{ static_cast<byte>(zp + 2) };
			// sta / stx / sty, then the return address off the stack; and the reload before the jump
			if (matches(p_rom, BANK15, p_far_call, { 0x85, zp, 0x86, x, 0x84, y, 0x68, 0x85, 0xec })
				&& matches(p_rom, BANK15, static_cast<word>(p_far_call + FAR_CALL_RELOAD),
					{ 0xa5, zp, 0xa6, x, 0xa4, y, 0x60 }))
				return zp;
		}
		throw unsupported(p_name, BANK15, p_far_call);
	}
}

std::string fh::faxoptions_label(GeneralHackLib p_type) {
	const std::string name{ klib::str::enum_to_string(p_type).substr(PREFIX.size()) };
	std::string label;
	for (std::size_t i{ 0 }; i < name.size(); ++i) {
		if (i > 0 && std::isupper(static_cast<unsigned char>(name[i])))
			label += ' ';
		label += static_cast<char>(std::toupper(static_cast<unsigned char>(name[i])));
	}
	// a long label drops its trailing " CONTROL"
	if (label.size() > MAX_LABEL && label.ends_with(CONTROL))
		label.resize(label.size() - CONTROL.size());
	return label;
}

std::vector<fh::FaxOptionsRow> fh::faxoptions_rows(const std::vector<GeneralHack>& p_hacks) {
	const std::string name{ "AtlasDevFaxOptions" };
	std::vector<FaxOptionsRow> rows;
	std::set<byte> flags;
	for (const auto& hack : p_hacks) {
		if (hack.get_type() == GeneralHackLib::KillSwitch)
			throw std::runtime_error(name + ": cannot be combined with KillSwitch, they share the pause screen");
		if (hack.get_type() == GeneralHackLib::AtlasDevFaxOptions)
			continue;
		if (!klib::str::enum_to_string(hack.get_type()).starts_with(PREFIX) || !hack.has_param("flag"))
			continue;
		const word value{ hack.get_word("flag") };
		if (value > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
		const byte flag{ static_cast<byte>(value) };
		if (!flags.insert(flag).second)
			throw std::runtime_error(std::format("{}: two listed hacks share flag {}", name, flag));
		const auto label{ faxoptions_label(hack.get_type()) };
		if (label.size() > MAX_LABEL)
			throw std::runtime_error(std::format("{}: the label {} does not fit {} characters", name, label, MAX_LABEL));
		rows.push_back({ label, flag });
	}
	if (rows.empty())
		throw std::runtime_error(name + ": no listed AtlasDev hack has a flag");
	if (rows.size() > MAX_ROWS)
		throw std::runtime_error(std::format("{}: {} rows, at most {}", name, rows.size(), MAX_ROWS));
	return rows;
}

word fh::HackManager::install_AtlasDevFaxOptions(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) {
	const std::string name{ "AtlasDevFaxOptions" };
	// every check comes first, so a refused install leaves the rom byte identical
	const auto rows{ faxoptions_rows(m_project_hacks) };
	const bool has_save{ sram_state.has_value() };

	if (p_config.boolean_or(c::ID_FLAGS_WRAM_TO_SRAM, false))
		throw std::runtime_error(name + ": this ROM keeps the flags in SRAM; only the US, US rev A and EU ROMs "
			"are supported");
	const byte far_zp{ far_call_zp(p_rom, cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL), name) };
	// the menu's cursor, row and top take the three the far call leaves alone
	const byte menu_zp{ far_zp == RAM::ZP_BankedCallSavedA ? RAM::ZP_e2 : RAM::ZP_BankedCallSavedA };
	require_code(p_rom, BANK15, ROM::Sprites_FlipRanges, { 0xa2, 0x04, 0xa0, 0x84 }, name);
	require_code(p_rom, BANK15, cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK), { 0x8e, 0x00, 0x01, 0xa9, 0x01 }, name);
	require_code(p_rom, BANK15, ROM::WaitForNextFrame, { 0xa9, 0x00, 0x85, 0x10, 0xa5, 0x10, 0xf0, 0xfc, 0x60 }, name);
	// AtlasDevFastBlink puts a jmp here; its stub is the stock wait while the frame handlers run ($13 non-zero),
	// and they always do when the menu runs
	if (p_rom[klib::Asm6502::get_file_offset(BANK15, ROM::PPUBuffer_WaitEmpty)] != JMP)
		require_code(p_rom, BANK15, ROM::PPUBuffer_WaitEmpty, { 0xa5, 0x20, 0xc5, 0x1f, 0xd0, 0xfa, 0x60 }, name);
	require_code(p_rom, BANK15, ROM::PPUQueueAppendHeader, { 0x48, 0x20, 0xca, 0xcf, 0x68, 0xa6, 0x20 }, name);
	require_code(p_rom, BANK15, ROM::PPUAddressFromPos, { 0xa5, 0x0d, 0x29, 0x01, 0x09, 0x08, 0x85, 0xe9 }, name);
	require_code(p_rom, BANK15, ROM::PPUQueuePayload, { 0x9d, 0x00, 0x05, 0xe8, 0x60 }, name);
	require_code(p_rom, WINDOW_BANK, ROM::OpenWindowDraw, { 0xad, 0x08, 0x02, 0x85, 0xea, 0xad, 0x09, 0x02, 0x85, 0xeb },
		name);
	require_code(p_rom, WINDOW_BANK, ROM::WindowClose, { 0xa9, 0x01, 0x8d, 0x0c, 0x02 }, name);
	if (has_save)
		require_code(p_rom, BANK15, ROM::Game_ClearTimedItems, { 0xa9, 0xff, 0x8d, 0x28, 0x04 }, name);

	require_site(p_rom, ROM::GameLoop_CheckPauseGame_JSR_Sprites_FlipRanges, PAUSE_ORIG, name);
	if (has_save) {
		require_site(p_rom, ROM::Player_Spawn, SPAWN_ORIG, name);
		require_site(p_rom, ROM::Game_Start, START_ORIG, name);
	}
	else
		require_site(p_rom, ROM::Game_InitStateForStartScreen_STA_CurrentStage, TITLE_ORIG, name);

	// bank= and addr= place the menu; the resident part is never in a switchable bank
	std::map<std::string, std::string> resident_params;
	if (p_hack.has_param("sram"))
		resident_params.insert({ "sram", p_hack.get_string("sram") });
	const GeneralHack resident{ name, resident_params };

	const word block{ has_save ? reserve_sram_top(BLOCK_SIZE) : word{ 0 } };
	const word state{ has_save ? static_cast<word>(block + SIGNATURE_SIZE) : RAM::Flags };
	// without a save the row bits are the flag bits
	const int bit_offset{ has_save ? STATE_OFFSET : FLAG_OFFSET };
	const int bit_mask{ has_save ? STATE_MASK : FLAG_MASK };
	// 64 rows make 256 table bytes, and x wraps to 0 there too
	const byte table_end{ static_cast<byte>(ROW_SIZE * rows.size()) };

	klib::Asm6502 code;

	// the play-entry hooks with a save, the title screen hook without: the
	// displaced instructions, then the setup with every register and flag
	// kept as they left them
	if (has_save) {
		code.label("@start");
		code.jsr(ROM::Game_ClearTimedItems);
		code.jmp("@enter");
		code.label("@spawn");
		code.lda_imm(SPAWN_HP);
		code.sta_abs(RAM::PlayerHP);
	}
	else {
		code.label("@title");
		code.sta_abs(RAM::CurrentStage);
	}
	code.label("@enter");
	code.php();
	code.pha();
	code.txa();
	code.pha();
	code.tya();
	code.pha();
	// with a save the block is checked first; without one every row goes on
	code.jsr(has_save ? "@setup" : "@defaults");
	code.pla();
	code.tay();
	code.pla();
	code.tax();
	code.pla();
	code.plp();
	code.rts();

	if (has_save) {
		code.label("@setup");
		code.ldx_imm(SIGNATURE_SIZE - 1);
		code.label("@check");
		code.lda_abs_x("@signature");
		code.cmp_abs_x(block);
		code.bne("@fresh");
		code.dex();
		code.bpl("@check");
		code.jmp("@apply");
		// a fresh cart or another build: the signature, the identity, every row on
		code.label("@fresh");
		code.ldx_imm(SIGNATURE_SIZE - 1);
		code.label("@sign");
		code.lda_abs_x("@signature");
		code.sta_abs_x(block);
		code.dex();
		code.bpl("@sign");
		code.lda_imm(0);
		code.ldx_imm(STATE_SIZE - 1);
		code.label("@zero");
		code.sta_abs_x(state);
		code.dex();
		code.bpl("@zero");
		code.jmp("@defaults");
	}

	// the pause hook: the stock call, then select opens the menu
	code.label("@pause");
	code.jsr(ROM::Sprites_FlipRanges);
	code.lda_zp(RAM::ZP_Joy1_ChangedButtonMask);
	code.and_imm(SELECT);
	code.beq("@pause_done");
	code.jmp("@menu");
	code.label("@pause_done");
	code.rts();

	// copy each row's state bit onto its flag bit; other bits stay as they are
	code.label("@apply");
	if (has_save) {
		code.ldx_imm(0);
		code.label("@apply_row");
		code.lda_abs_x("@rows", STATE_OFFSET);
		code.tay();
		code.lda_abs_y(state);
		code.and_abs_x("@rows", STATE_MASK);
		code.php();                                // z set: the row is off
		code.lda_abs_x("@rows", FLAG_OFFSET);
		code.tay();
		code.plp();
		code.beq("@apply_clear");
		code.lda_abs_y(RAM::Flags);
		code.ora_abs_x("@rows", FLAG_MASK);
		code.sta_abs_y(RAM::Flags);
		code.jmp("@apply_next");
		code.label("@apply_clear");
		code.lda_abs_x("@rows", FLAG_MASK);
		code.eor_imm(0xff);
		code.and_abs_y(RAM::Flags);
		code.sta_abs_y(RAM::Flags);
		code.label("@apply_next");
		code.inx();
		code.inx();
		code.inx();
		code.inx();
		code.cpx_imm(table_end);
		code.bne("@apply_row");
	}
	// without a save the flags are the state, so there is nothing to copy
	code.rts();

	// x = row: flip its bit, then apply
	code.label("@toggle");
	code.txa();
	code.asl_a();
	code.asl_a();
	code.tax();
	code.lda_abs_x("@rows", bit_offset);
	code.tay();
	code.lda_abs_y(state);
	code.eor_abs_x("@rows", bit_mask);
	code.sta_abs_y(state);
	code.jmp("@apply");

	// every row on, then apply
	code.label("@defaults");
	code.ldx_imm(0);
	code.label("@defaults_row");
	code.lda_abs_x("@rows", bit_offset);
	code.tay();
	code.lda_abs_y(state);
	code.ora_abs_x("@rows", bit_mask);
	code.sta_abs_y(state);
	code.inx();
	code.inx();
	code.inx();
	code.inx();
	code.cpx_imm(table_end);
	code.bne("@defaults_row");
	code.jmp("@apply");

	// the signature, the version and the identity of the row list
	if (has_save) {
		word identity{ 0 };
		for (std::size_t i{ 0 }; i < rows.size(); ++i)
			identity = static_cast<word>(identity + rows[i].flag * (i + 1));
		code.label("@signature");
		code.db('F');
		code.db('X');
		code.db('O');
		code.db(VERSION);
		code.dw(identity);
	}

	// per row: flag offset, flag mask, state offset, state mask
	code.label("@rows");
	for (std::size_t i{ 0 }; i < rows.size(); ++i) {
		code.db(static_cast<byte>(rows[i].flag >> 3));
		code.db(static_cast<byte>(1 << (rows[i].flag & 7)));
		code.db(static_cast<byte>(i >> 3));
		code.db(static_cast<byte>(1 << (i & 7)));
	}

	// the menu's entry follows the resident part
	code.label("@menu");

	auto place{ place_hack(p_config, p_rom, resident, name, cpu_addr, code.size(), 1) };
	const auto at = [&](const std::string& p_label) { return code.label_addr(p_label, place.addr); };
	const word pause{ at("@pause") };
	const word spawn{ has_save ? at("@spawn") : word{ 0 } }, start{ has_save ? at("@start") : word{ 0 } },
		title{ has_save ? word{ 0 } : at("@title") };
	const FaxOptionsResident addrs{
		.apply = at("@apply"),
		.toggle = at("@toggle"),
		.defaults = at("@defaults"),
		.state = state,
		.rows_table = at("@rows"),
		.row_count = static_cast<word>(rows.size()),
		.bit_column = static_cast<byte>(bit_offset)
	};
	const word menu_entry{ at("@menu") };
	klib::Asm6502 menu;
	emit_faxoptions_menu(menu, rows, addrs, menu_zp, p_config);

	// written to a copy until everything is in, so a refusal leaves the rom as it was
	std::vector<byte> patched{ p_rom };
	word next{ install_placed(patched, code, place, cpu_addr, name) };
	auto menu_place{ place_hack(p_config, patched, p_hack, name, next, menu.size(), 1, MENU_BANK) };
	const word menu_addr{ menu_place.addr };
	next = install_placed(patched, menu, menu_place, next, name);
	if (far_entry(p_config, patched, menu_place, menu_addr, next, name) != menu_entry)
		throw std::logic_error(name + ": the menu's entry is not at the end of the resident part");

	code.jsr(pause);
	code.apply_hack_and_clear(patched, BANK15, ROM::GameLoop_CheckPauseGame_JSR_Sprites_FlipRanges);
	if (has_save) {
		code.jsr(spawn);
		code.nop(SPAWN_ORIG.size() - 3);
		code.apply_hack_and_clear(patched, BANK15, ROM::Player_Spawn);
		code.jsr(start);
		code.apply_hack_and_clear(patched, BANK15, ROM::Game_Start);
	}
	else {
		code.jsr(title);
		code.apply_hack_and_clear(patched, BANK15, ROM::Game_InitStateForStartScreen_STA_CurrentStage);
	}
	p_rom = std::move(patched);
	return next;
}
