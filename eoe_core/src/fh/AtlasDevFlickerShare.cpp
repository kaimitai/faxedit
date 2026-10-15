#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// flicker share: the entity loop at $800f updates and draws slots 7 to 0, in
// that order every pass, and the sprite allocator hands out its 64 entries
// in draw order. when two big entities do not fit, the one drawn first is
// whole every pass and the other is starved every pass, so it never shows.
// after a pass that overflowed, the next one runs the other way, so each gets
// the budget on alternate passes and both show, flickering. a pass that fits
// keeps the stock order, and only the drawing order changes: every entity is
// still updated every pass.
//
// two bank 14 sites are retargeted: the loop head at $800f (ldx #7, stx
// $0378) and the loop tail at $8043 (ldx $0378, dex, bpl $8011). nothing
// branches into either, and the loop is the same in every region. $39 counts
// the entries dropped this frame and is cleared with the sprite count every
// frame, so the tail latches it for the next pass.
//
// one ram byte at $04ff: bit 7 says the last pass overflowed, bit 0 is the
// direction. the game never touches it in any region.
namespace {
	constexpr byte BANK{ 14 };
	constexpr int MAX_FLAG{ 247 };
	constexpr byte ZP_OVERFLOW{ 0x39 };
	constexpr word STATE{ 0x04ff };
	constexpr word SITE_HEAD{ 0x800f }, SITE_TAIL{ 0x8043 }, V_BODY{ 0x8014 }, V_EXIT{ 0x8049 };

	constexpr std::array<byte, 5> HEAD_ORIG{ 0xa2, 0x07, 0x8e, 0x78, 0x03 };
	constexpr std::array<byte, 6> TAIL_ORIG{ 0xae, 0x78, 0x03, 0xca, 0x10, 0xc8 };
	// where the hooks go on: the slot's first load and the loop's exit
	constexpr std::array<byte, 3> BODY_ORIG{ 0xbd, 0xcc, 0x02 };
	constexpr std::array<byte, 5> EXIT_ORIG{ 0xa9, 0x00, 0x85, 0x33, 0x60 };

	template<std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("AtlasDevFlickerShare: the vanilla site at ${:04x} is not intact", p_addr));
	}

	void emit_code(klib::Asm6502& c, int p_flag) {
		// in place of ldx #7 / stx $0378
		c.label("@head");
		c.ldx_imm(7);
		if (p_flag >= 0) {
			c.lda_abs(static_cast<word>(fh::RAM::Flags + (p_flag >> 3)));
			c.and_imm(static_cast<byte>(1 << (p_flag & 7)));
			c.beq("@fits");
		}
		c.lda_abs(STATE); c.bmi("@turn");
		c.label("@fits"); c.lda_imm(0); c.beq("@set");                // stock order
		c.label("@turn"); c.eor_imm(0x81); c.beq("@set");             // latch off, direction turned
		c.ldx_imm(0);
		c.label("@set"); c.sta_abs(STATE); c.stx_abs(fh::RAM::CurrentEntitySlot);
		c.rts();

		// in place of ldx $0378 / dex / bpl $8011
		c.label("@tail");
		c.ldx_abs(fh::RAM::CurrentEntitySlot);
		c.lda_abs(STATE); c.lsr_a(); c.bcc("@down");
		c.inx(); c.cpx_imm(8); c.bcc("@next"); c.bcs("@done");
		c.label("@down"); c.dex(); c.bpl("@next");
		c.label("@done");
		c.lda_zp(ZP_OVERFLOW); c.beq("@exit");                       // the pass overflowed: the next one turns
		c.lda_abs(STATE); c.ora_imm(0x80); c.sta_abs(STATE);
		c.label("@exit"); c.jmp(V_EXIT);
		c.label("@next"); c.stx_abs(fh::RAM::CurrentEntitySlot); c.jmp(V_BODY);
	}
}

word fh::HackManager::install_AtlasDevFlickerShare(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) {
	const std::string name{ "AtlasDevFlickerShare" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		const word f{ p_hack.word_or("flag", 0) };
		if (f > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
		flag = f;
	}
	if (mode == "vanilla")
		return cpu_addr;
	const bool sram{ p_hack.bool_or("sram", false) };
	if (sram && !sram_state)
		throw std::runtime_error(name + ": sram=true requires a rom with sram support, or the SRAM hack");

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_HEAD, HEAD_ORIG);
	require_site(p_rom, SITE_TAIL, TAIL_ORIG);
	require_site(p_rom, V_BODY, BODY_ORIG);
	require_site(p_rom, V_EXIT, EXIT_ORIG);

	klib::Asm6502 code;
	emit_code(code, flag);
	const std::size_t size{ code.size() };
	if (!sram) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
		for (std::size_t i{ 0 }; i < size; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
				throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));
	}

	// with sram the code runs from the shared sram code area, and bank 14
	// keeps only the two hooks; label addresses are read before the install
	// clears them
	const word body{ sram ? sram_hack_addr() : cpu_addr };
	const word head{ static_cast<word>(body + code.label_position("@head")) };
	const word tail{ static_cast<word>(body + code.label_position("@tail")) };
	if (sram) install_sram_hack(p_rom, code);
	else code.apply_hack_and_clear(p_rom, BANK, cpu_addr);

	code.jsr(head); code.nop(2); code.apply_hack_and_clear(p_rom, BANK, SITE_HEAD);
	code.jmp(tail); code.nop(3); code.apply_hack_and_clear(p_rom, BANK, SITE_TAIL);
	return sram ? cpu_addr : static_cast<word>(cpu_addr + size);
}
