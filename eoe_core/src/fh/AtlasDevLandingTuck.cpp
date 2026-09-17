#include "HackManager.h"
#include "fe/Config.h"
#include "fe/sprite/fe_sprite_constants.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// landing tuck: a short pose when the hero comes down from a jump. vanilla
// snaps him upright on the first grounded frame; this holds a crouched frame
// for three, four or six frames and then lets go. it is only how he is drawn.
// nothing about movement, physics, damage or collision changes, and pressing
// jump or attack drops the pose on the same frame.
//
// the pose is not new art. every gear group's jump frame is already a 16x32
// record, so the hack crops each one to its top 24 pixels and draws it eight
// pixels lower. that is a hero with his knees up, built from tiles the
// project already exports, so edited graphics follow automatically and no
// chr, no xml frame and no tile budget is touched.
//
// seven instructions are replaced. the per frame tick at $e0d3 gains a call
// to the state machine; the pose picker at $ec43 and the weapon and shield
// drawers at bank 14 $b875 and $b9d1 get sent through a chooser that hands
// the stock drawer a different record while the pose is up; and the three
// places that end a life or a screen, $d127, $e0aa and $d8f4, clear the state
// first. all seven are checked against their vanilla bytes before anything is
// written, and they are identical in the us, us rev a, eu and jp roms.
//
// one ram byte at $04fe holds it: bit 7 says the hero was in the air last
// frame, bit 6 says the pose is up, and the low three bits count it down.
// $04fe and $04ff are the last two bytes of the free tail and the only pair
// nothing else has taken; the game never touches either in any region.
//
// profile=off installs nothing, so a hack listed that way leaves the rom byte
// identical. with a flag the pose appears only while the flag is set; clear,
// the hero lands the way he always did.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };

	// the state byte and its bits
	constexpr word STATE{ 0x04fe };
	constexpr byte PREVIOUS_AIR{ 0x80 }, ACTIVE{ 0x40 }, REMAINING{ 0x07 };

	// the seven sites, and the stock routines the hooks hand control back to
	constexpr word TICK{ 0xe0d3 }, POSE{ 0xec43 }, CLEAR_TRANSITION{ 0xd127 },
		CLEAR_INIT{ 0xe0aa }, CLEAR_DEATH{ 0xd8f4 };
	constexpr word SHIELD_DRAW{ 0xb9d1 }, WEAPON_DRAW{ 0xb875 };
	constexpr word STOCK_TICK{ 0xe0e8 }, STOCK_DRAW{ 0xf039 }, DRAW_FROM_POINTER{ 0xf07d };

	// jsr $e0e8
	constexpr std::array<byte, 3> TICK_ORIG{ 0x20, 0xe8, 0xe0 };
	// jsr $f039
	constexpr std::array<byte, 3> POSE_ORIG{ 0x20, 0x39, 0xf0 };
	// jmp $f039, the tail of both drawers
	constexpr std::array<byte, 3> DRAW_ORIG{ 0x4c, 0x39, 0xf0 };
	// stx $54 / lda $0d
	constexpr std::array<byte, 4> CLEAR_TRANSITION_ORIG{ 0x86, 0x54, 0xa5, 0x0d };
	// lda #$00 / sta $9f
	constexpr std::array<byte, 4> CLEAR_INIT_ORIG{ 0xa9, 0x00, 0x85, 0x9f };
	// lda $a4 / and #$40
	constexpr std::array<byte, 4> CLEAR_DEATH_ORIG{ 0xa5, 0xa4, 0x29, 0x40 };

	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the jump frame of each gear group, and the crop taken from it
	constexpr std::size_t JUMP_FRAME{ 3 };
	constexpr std::size_t CROP_CELLS{ 6 };
	// 16x32, no offset, pivot 8
	constexpr std::array<byte, 4> SOURCE_HEADER{ 0x31, 0x00, 0x00, 0x08 };
	// 16x24, drawn eight pixels lower
	constexpr std::array<byte, 4> CROP_HEADER{ 0x21, 0x00, 0x08, 0x08 };
	constexpr byte NO_TILE{ 0xff };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, byte p_bank, word p_addr,
		const std::array<byte, N>& p_orig, const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(p_bank, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	// off holds no pose at all, so it installs nothing
	byte frames_of(const fh::GeneralHack& p_hack, const std::string& p_name) {
		std::string v{ p_hack.string_or("profile", "medium") };
		std::transform(v.begin(), v.end(), v.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (v == "off") return 0;
		if (v == "light") return 3;
		if (v == "medium") return 4;
		if (v == "heavy") return 6;
		throw std::runtime_error(std::format("{}: profile must be off, light, medium or heavy", p_name));
	}

	// the crop, one record per gear group. the frame directory is the one the
	// project exported, so a group whose art was edited crops its own art.
	std::vector<std::vector<byte>> crop_jump_frames(const fe::Config& p_config,
		const std::vector<byte>& p_rom, const std::string& p_name) {
		const auto [ptr, zero] { p_config.pointer(fe::c::ID_GFX_PLAYER_ANIM_FRAME_PTR) };
		const auto tile_counts{ p_config.constant(fe::c::ID_GFX_PLAYER_TILE_COUNT_OFFSET) };
		if (ptr + 1 >= p_rom.size())
			throw std::runtime_error(p_name + ": the player frame directory is outside the rom");
		const std::size_t directory{ zero + (p_rom[ptr] | (p_rom[ptr + 1] << 8)) };

		std::vector<std::vector<byte>> out;
		for (std::size_t group{ 0 }; group < fe::c::PLAYER_TYPE_COUNT; ++group) {
			const std::size_t entry{ directory + 2 * (group * fe::c::PLAYER_FRAME_COUNT + JUMP_FRAME) };
			if (entry + 1 >= p_rom.size())
				throw std::runtime_error(p_name + ": the player frame directory is outside the rom");
			const std::size_t frame{ zero + (p_rom[entry] | (p_rom[entry + 1] << 8)) };
			if (frame + SOURCE_HEADER.size() > p_rom.size())
				throw std::runtime_error(p_name + ": a jump frame points outside the rom");
			for (std::size_t i{ 0 }; i < SOURCE_HEADER.size(); ++i)
				if (p_rom[frame + i] != SOURCE_HEADER[i])
					throw std::runtime_error(std::format(
						"{}: the jump frame of gear group {} is not the 16x24 crop's source shape", p_name, group));

			std::vector<byte> record(CROP_HEADER.begin(), CROP_HEADER.end());
			std::size_t at{ frame + SOURCE_HEADER.size() };
			bool drawn{ false };
			for (std::size_t cell{ 0 }; cell < CROP_CELLS; ++cell) {
				if (at >= p_rom.size())
					throw std::runtime_error(p_name + ": a jump frame ends before its sixth cell");
				const byte tile{ p_rom[at] };
				record.push_back(tile);
				++at;
				if (tile == NO_TILE)
					continue;
				if (at >= p_rom.size())
					throw std::runtime_error(p_name + ": a jump frame ends before a cell's attribute");
				// the group loads a fixed number of tiles; a crop may not reach past them
				if (tile_counts + group >= p_rom.size() || tile > p_rom[tile_counts + group])
					throw std::runtime_error(std::format(
						"{}: the jump frame of gear group {} uses a tile that group does not load", p_name, group));
				record.push_back(p_rom[at]);
				++at;
				drawn = true;
			}
			if (!drawn)
				throw std::runtime_error(std::format(
					"{}: the crop of gear group {} would draw nothing", p_name, group));
			out.push_back(std::move(record));
		}
		return out;
	}
}

word fh::HackManager::install_AtlasDevLandingTuck(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevLandingTuck" };
	// every parameter is judged in every mode, vanilla included, so a typo is
	// caught even when the hack installs nothing
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	const byte frames{ frames_of(p_hack, name) };
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = static_cast<int>(p_hack.word_or("flag", 0));
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla" || frames == 0)
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, BANK15, TICK, TICK_ORIG, name);
	require_site(p_rom, BANK15, POSE, POSE_ORIG, name);
	require_site(p_rom, BANK14, SHIELD_DRAW, DRAW_ORIG, name);
	require_site(p_rom, BANK14, WEAPON_DRAW, DRAW_ORIG, name);
	require_site(p_rom, BANK15, CLEAR_TRANSITION, CLEAR_TRANSITION_ORIG, name);
	require_site(p_rom, BANK15, CLEAR_INIT, CLEAR_INIT_ORIG, name);
	require_site(p_rom, BANK15, CLEAR_DEATH, CLEAR_DEATH_ORIG, name);
	const auto records{ crop_jump_frames(p_config, p_rom, name) };

	klib::Asm6502 code;
	const auto raw = [&code](std::initializer_list<byte> p_bytes) {
		for (byte b : p_bytes) code.db(b);
	};

	// the stock call this replaces runs first, then the state machine, and the
	// stock routine's own flags and accumulator are handed back untouched
	code.label("tick");
	code.jsr(STOCK_TICK);
	raw({ 0x08, 0x48 });                       // php / pha
	code.jsr("evaluate");
	raw({ 0x68, 0x28, 0x60 });                 // pla / plp / rts

	code.label("evaluate");
	if (flag >= 0) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3)));
		code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.beq("cancel");
	}
	code.lda_zp(0x54);                         // a screen change is under way
	code.cmp_imm(0xff);
	code.bne("cancel");
	code.lda_abs(0x0438);                      // a cutscene owns the hero
	code.bne("cancel");
	code.lda_zp(0xa4);                         // climbing, or already crouched
	code.and_imm(0x98);
	code.bne("cancel");
	code.lda_zp(0xa5);                         // wing boots, attacking, hurt
	code.and_imm(0x83);
	code.bne("cancel");
	code.lda_zp(0x19);                         // jump or attack pressed this frame
	code.and_imm(0xf0);
	code.bne("cancel");
	code.lda_zp(0xa4);
	code.and_imm(0x05);                        // in the air
	code.beq("grounded");
	code.lda_imm(PREVIOUS_AIR);
	code.sta_abs(STATE);
	code.rts();

	code.label("grounded");
	code.lda_abs(STATE);
	code.bmi("touchdown");                     // he was in the air last frame
	code.and_imm(REMAINING);
	code.jmp("count");
	code.label("touchdown");
	code.lda_imm(frames);
	code.label("count");
	code.beq("cancel");
	code.sec();
	code.sbc_imm(1);
	code.ora_imm(ACTIVE);
	code.sta_abs(STATE);
	code.rts();
	code.label("cancel");
	code.lda_imm(0);
	code.sta_abs(STATE);
	code.rts();

	// the chooser. while the pose is up the stock drawer is entered past its
	// own record lookup, with the crop's address in $3a
	code.label("pose");
	raw({ 0x08, 0x48 });
	code.lda_abs(STATE);
	code.and_imm(ACTIVE);
	code.bne("pose_tuck");
	raw({ 0x68, 0x28 });
	code.jmp(STOCK_DRAW);
	code.label("pose_tuck");
	raw({ 0x68, 0x28, 0x4a, 0x4a, 0x4a, 0x0a, 0xaa });   // pla / plp / gear group * 2 into x
	code.lda_abs_x("pose_table");
	code.sta_zp(0x3a);
	code.lda_abs_x("pose_table_hi");
	code.sta_zp(0x3b);
	code.lda_abs(0x0100);                      // the bank the drawer returns to
	code.pha();
	code.jmp(DRAW_FROM_POINTER);

	// the shield hangs off the body, so it comes down with it
	code.label("shield_draw");
	raw({ 0x08, 0x48 });
	code.lda_abs(STATE);
	code.and_imm(ACTIVE);
	code.beq("shield_stock");
	code.lda_zp(0x28);
	code.clc();
	code.adc_imm(8);
	code.bcs("shield_stock");
	code.sta_zp(0x28);
	code.label("shield_stock");
	raw({ 0x68, 0x28 });
	code.jmp(STOCK_DRAW);

	// the weapon is not drawn at all while he is tucked
	code.label("weapon_draw");
	raw({ 0x08, 0x48 });
	code.lda_abs(STATE);
	code.and_imm(ACTIVE);
	code.beq("weapon_stock");
	raw({ 0x68, 0x28, 0x60 });
	code.label("weapon_stock");
	raw({ 0x68, 0x28 });
	code.jmp(STOCK_DRAW);

	// the three lifecycle stubs each clear the byte and then run the vanilla
	// instructions they displaced. clear_all leaves a zero, which is the value
	// the init site was storing anyway
	code.label("clear_all");
	code.lda_imm(0);
	code.sta_abs(STATE);
	code.rts();
	code.label("clear_transition");
	code.jsr("clear_all");
	raw({ 0x86, 0x54, 0xa5, 0x0d, 0x60 });
	code.label("clear_init");
	code.jsr("clear_all");
	raw({ 0x85, 0x9f, 0x60 });
	code.label("clear_death");
	code.jsr("clear_all");
	raw({ 0xa5, 0xa4, 0x29, 0x40, 0x60 });

	// gear groups that crop to the same bytes share one record
	std::vector<std::vector<byte>> unique;
	std::array<std::size_t, fe::c::PLAYER_TYPE_COUNT> alias{};
	for (std::size_t i{ 0 }; i < records.size(); ++i) {
		const auto found{ std::find(unique.begin(), unique.end(), records[i]) };
		alias[i] = static_cast<std::size_t>(found - unique.begin());
		if (found == unique.end())
			unique.push_back(records[i]);
	}
	const word table{ static_cast<word>(cpu_addr + code.size()) };
	std::vector<word> at(unique.size());
	word next{ static_cast<word>(table + 2 * fe::c::PLAYER_TYPE_COUNT) };
	for (std::size_t i{ 0 }; i < unique.size(); ++i) {
		at[i] = next;
		next = static_cast<word>(next + unique[i].size());
	}
	code.label("pose_table");
	code.db(static_cast<byte>(at[alias[0]]));
	code.label("pose_table_hi");
	code.db(static_cast<byte>(at[alias[0]] >> 8));
	for (std::size_t i{ 1 }; i < alias.size(); ++i)
		code.dw(at[alias[i]]);
	for (const auto& record : unique)
		for (byte b : record)
			code.db(b);

	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));

	const word tick{ static_cast<word>(cpu_addr + code.label_position("tick")) };
	const word pose{ static_cast<word>(cpu_addr + code.label_position("pose")) };
	const word shield{ static_cast<word>(cpu_addr + code.label_position("shield_draw")) };
	const word weapon{ static_cast<word>(cpu_addr + code.label_position("weapon_draw")) };
	const word transition{ static_cast<word>(cpu_addr + code.label_position("clear_transition")) };
	const word init{ static_cast<word>(cpu_addr + code.label_position("clear_init")) };
	const word death{ static_cast<word>(cpu_addr + code.label_position("clear_death")) };
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);

	code.jsr(tick);
	code.apply_hack_and_clear(p_rom, BANK15, TICK);
	code.jsr(pose);
	code.apply_hack_and_clear(p_rom, BANK15, POSE);
	code.jmp(shield);
	code.apply_hack_and_clear(p_rom, BANK14, SHIELD_DRAW);
	code.jmp(weapon);
	code.apply_hack_and_clear(p_rom, BANK14, WEAPON_DRAW);
	code.jsr(transition);
	code.nop();
	code.apply_hack_and_clear(p_rom, BANK15, CLEAR_TRANSITION);
	code.jsr(init);
	code.nop();
	code.apply_hack_and_clear(p_rom, BANK15, CLEAR_INIT);
	code.jsr(death);
	code.nop();
	code.apply_hack_and_clear(p_rom, BANK15, CLEAR_DEATH);

	return static_cast<word>(cpu_addr + size);
}
