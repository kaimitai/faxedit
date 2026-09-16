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

// fast blink: a screen change that blanks and redraws the screen (going up
// or down, an area without smooth scrolling, or left and right with
// AtlasDevScreenTransition) spends about 16 frames before play resumes. about
// half of that is waiting: two frame waits inside the sprite setup, the
// enemy graphics trickling through the ppu queue a few tiles a frame, and a
// two frame wait for the display to go dark. this hack turns the display
// and the frame handlers off first and runs the same steps back to back,
// with the queue drained on the spot, so a change takes about 8 frames.
// the screen that comes up is the same, byte for byte.
//
// the blank path at $db91 becomes a jump to the new path in bank 15. two
// queue waits get a short stub each: the capacity wait at $cfca runs a
// drain call itself while the handlers are off, and the clear wait at $cff4
// (which the graphics loader jumps to) drains the whole queue. the stubs
// act only where the stock game would hang: no room, or a queue still
// full, with the handlers off and so no nmi to drain it. everywhere the
// stock game goes they behave as the stock waits. every site is checked
// against its vanilla bytes first, and they are identical in the us, us
// rev a, eu and jp roms. no ram is claimed.
//
// with a flag the new path runs only while the flag is set; clear, a copy
// of the stock path runs. the waits need no flag, for the reason above.
//
// AtlasDevScreenTransition checks the stock blank path before it installs, so
// it must be listed before this hack.
namespace {
	constexpr byte BANK15{ 15 };
	constexpr word PATH{ 0xdb91 }, MAIN_LOOP{ 0xdb45 };
	constexpr word FLUSH{ 0xcaf7 }, RESET_GAMEPLAY{ 0xcb17 }, DRAW{ 0xcf3c }, DRAW_ALL{ 0xcffb };
	constexpr word WFC{ 0xcfca }, WUC{ 0xcff4 }, HAS_CAPACITY{ 0xcfd0 };
	constexpr word RESET_SPRITES{ 0xcb47 }, CLEAR_SPRITES{ 0xc130 }, SPRITE_INFO{ 0xc1b4 }, SPRITE_IMAGES{ 0xc28d };
	constexpr word WAIT_FRAME{ 0xca25 }, REDRAW{ 0xdd0f };
	constexpr byte HANDLERS{ 0x13 }, PAUSE_COUNT{ 0x14 }, FORCE_LOWER{ 0x5b };
	constexpr word PPUMASK{ 0x2001 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the blank path: sprite reset, wait, clear, info, wait, graphics, flush, redraw, reset, back to the loop
	constexpr std::array<byte, 30> PATH_ORIG{ 0x20, 0x47, 0xcb, 0x20, 0x25, 0xca, 0x20, 0x30, 0xc1, 0x20, 0xb4, 0xc1,
		0x20, 0x25, 0xca, 0x20, 0x8d, 0xc2, 0x20, 0xf7, 0xca, 0x20, 0x0f, 0xdd, 0x20, 0x17, 0xcb, 0x4c, 0x45, 0xdb };
	// PPU_WaitUntilFlushed: the wait, then the three stores the new path repeats
	constexpr std::array<byte, 21> FLUSH_ORIG{ 0xa5, 0x20, 0xc5, 0x1f, 0xd0, 0xfa, 0xa9, 0x00, 0x85, 0x14, 0x85, 0x13,
		0x85, 0x5b, 0xa5, 0x14, 0xc9, 0x02, 0x90, 0xfa, 0x60 };
	constexpr std::array<byte, 8> RESET_ORIG{ 0x20, 0x47, 0xcb, 0xa9, 0x01, 0x85, 0x13, 0x60 };
	constexpr std::array<byte, 6> DRAW_ORIG{ 0xa5, 0x1f, 0xc5, 0x20, 0xf0, 0xf9 };
	constexpr std::array<byte, 10> DRAW_ALL_ORIG{ 0x20, 0x3c, 0xcf, 0xa5, 0x20, 0xc5, 0x1f, 0xd0, 0xf7, 0x60 };
	constexpr std::array<byte, 6> WFC_ORIG{ 0x20, 0xd0, 0xcf, 0x90, 0xfb, 0x60 };
	constexpr std::array<byte, 7> WUC_ORIG{ 0xa5, 0x20, 0xc5, 0x1f, 0xd0, 0xfa, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK15, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	void hook(std::vector<byte>& p_rom, word p_site, word p_target, std::size_t p_length) {
		klib::Asm6502 code;
		code.jmp(p_target);
		code.nop(p_length - 3);
		code.apply_hack_and_clear(p_rom, BANK15, p_site);
	}
}

word fh::HackManager::install_AtlasDevFastBlink(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevFastBlink" };
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
	require_site(p_rom, PATH, PATH_ORIG, name);
	require_site(p_rom, FLUSH, FLUSH_ORIG, name);
	require_site(p_rom, RESET_GAMEPLAY, RESET_ORIG, name);
	require_site(p_rom, DRAW, DRAW_ORIG, name);
	require_site(p_rom, DRAW_ALL, DRAW_ALL_ORIG, name);
	require_site(p_rom, WFC, WFC_ORIG, name);
	require_site(p_rom, WUC, WUC_ORIG, name);

	klib::Asm6502 code;
	if (flag >= 0) {
		// clear: the stock path, byte for byte
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.bne("@go");
		for (const word target : { RESET_SPRITES, WAIT_FRAME, CLEAR_SPRITES, SPRITE_INFO, WAIT_FRAME, SPRITE_IMAGES,
			FLUSH, REDRAW, RESET_GAMEPLAY })
			code.jsr(target);
		code.jmp(MAIN_LOOP);
		code.label("@go");
	}
	// the stores of PPU_WaitUntilFlushed, then the display off, then the stock steps without their waits
	code.lda_imm(0); code.sta_zp(PAUSE_COUNT); code.sta_zp(HANDLERS); code.sta_zp(FORCE_LOWER);
	code.sta_abs(PPUMASK);
	for (const word target : { DRAW_ALL, RESET_SPRITES, CLEAR_SPRITES, SPRITE_INFO, SPRITE_IMAGES, DRAW_ALL, REDRAW,
		RESET_GAMEPLAY })
		code.jsr(target);
	code.jmp(MAIN_LOOP);
	// the capacity wait: no room and handlers on waits for the nmi as stock; handlers off runs a drain call
	const word wfc{ static_cast<word>(cpu_addr + code.size()) };
	code.label("@wfc");
	code.jsr(HAS_CAPACITY); code.bcs("@ok");
	code.lda_zp(HANDLERS); code.bne("@wfc");
	code.jsr(DRAW); code.jmp("@wfc");
	code.label("@ok"); code.rts();
	// the clear wait: handlers off drains the whole queue; on, the stock wait
	const word wuc{ static_cast<word>(cpu_addr + code.size()) };
	code.lda_zp(HANDLERS); code.beq("@drain");
	code.label("@wait");
	code.lda_zp(0x20); code.cmp_zp(0x1f); code.bne("@wait");
	code.rts();
	code.label("@drain"); code.jmp(DRAW_ALL);

	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	hook(p_rom, PATH, cpu_addr, PATH_ORIG.size());
	hook(p_rom, WFC, wfc, WFC_ORIG.size());
	hook(p_rom, WUC, wuc, WUC_ORIG.size());
	return static_cast<word>(cpu_addr + size);
}
