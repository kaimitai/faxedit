#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// screen blink: walking off the left or right side of a screen makes the game
// slide the next screen in, 64 frames with everything frozen. going up or
// down it blanks the screen and redraws it in one burst instead. this hack
// makes left and right do the same, so a horizontal screen change takes about
// 16 frames instead of about 72.
//
// the main loop picks between the two at $db86: an area whose smooth flag
// ($042f) is clear always redraws, and otherwise only left and right slide,
// through the bcc at $db8f. without a flag that bcc becomes two nops. with a
// flag, the lda/cmp before it becomes a jsr to 15 bytes of bank 15 code that
// keeps the slide while the flag is clear. the gate, the redraw path after it
// and the redraw loop at $d2ce are checked against their vanilla bytes first,
// and they are identical in the us, us rev a, eu and jp roms. no ram is
// claimed.
namespace {
	constexpr byte BANK15{ 15 };
	constexpr word GATE{ 0xdb86 }, CALL{ 0xdb8b }, BRANCH{ 0xdb8f }, REDRAW_PATH{ 0xdb91 }, SLIDE{ 0xdbaf },
		REDRAW{ 0xd2ce };
	constexpr byte DIRECTION{ 0x54 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };
	constexpr std::size_t STUB_SIZE{ 15 };

	// lda $042f / beq $db91 / lda $54 / cmp #$02 / bcc $dbaf
	constexpr std::array<byte, 11> GATE_ORIG{ 0xad, 0x2f, 0x04, 0xf0, 0x06, 0xa5, 0x54, 0xc9, 0x02, 0x90, 0x1e };
	// the redraw path: the sprites, their graphics, the queue flush, the redraw, then back to the main loop
	constexpr std::array<byte, 30> PATH_ORIG{ 0x20, 0x47, 0xcb, 0x20, 0x25, 0xca, 0x20, 0x30, 0xc1, 0x20, 0xb4, 0xc1,
		0x20, 0x25, 0xca, 0x20, 0x8d, 0xc2, 0x20, 0xf7, 0xca, 0x20, 0x0f, 0xdd, 0x20, 0x17, 0xcb, 0x4c, 0x45, 0xdb };
	// the redraw loop: one step and one queue write at a time until the scroll wraps, left and right included
	constexpr std::array<byte, 25> REDRAW_ORIG{ 0x20, 0x48, 0xe0, 0x20, 0xe7, 0xd2, 0x20, 0x1d, 0xd6, 0xa5, 0x54, 0xc9,
		0x02, 0xb0, 0x05, 0xa5, 0x0c, 0xd0, 0xed, 0x60, 0xa5, 0x57, 0xd0, 0xe8, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK15, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevScreenBlink(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevScreenBlink" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = static_cast<int>(p_hack.word_or("flag", 0));
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, GATE, GATE_ORIG, name);
	require_site(p_rom, REDRAW_PATH, PATH_ORIG, name);
	require_site(p_rom, REDRAW, REDRAW_ORIG, name);

	// without a flag the slide's branch becomes two nops, and no free space is used
	if (flag < 0) {
		const auto off{ klib::Asm6502::get_file_offset(BANK15, BRANCH) };
		p_rom[off] = 0xea;
		p_rom[off + 1] = 0xea;
		return cpu_addr;
	}

	// carry out: set redraws, clear slides. up and down keep their set carry
	klib::Asm6502 code;
	code.lda_zp(DIRECTION); code.cmp_imm(0x02); code.bcs(8);
	code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
	code.beq(1); code.sec();
	code.rts();
	const std::size_t size{ code.size() };
	if (size != STUB_SIZE)
		throw std::runtime_error(name + ": unexpected code size");
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	// the bcc moves up two bytes and still lands on the slide
	code.jsr(cpu_addr); code.bcc(static_cast<byte>(SLIDE - (CALL + 5))); code.nop();
	code.apply_hack_and_clear(p_rom, BANK15, CALL);
	return static_cast<word>(cpu_addr + size);
}
