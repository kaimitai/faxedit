#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include "fh_constants.h"

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
// the blank path in Game_MainLoop becomes a jump to the new path. two queue
// waits get a short stub each: PPUBuffer_WaitForCapacity runs a drain call
// itself while the handlers are off, and PPUBuffer_WaitEmpty (which the
// graphics loader jumps to) drains the whole queue. the stubs
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
	namespace ROM = fh::ROM;
	namespace RAM = fh::RAM;

	constexpr byte BANK15{ 15 };
	// the blank path in Game_MainLoop
	constexpr word Game_MainLoop_blankTransition{ 0xdb91 };
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

word fh::HackManager::install_AtlasDevFastBlink(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) {
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
	require_site(p_rom, Game_MainLoop_blankTransition, PATH_ORIG, name);
	require_site(p_rom, ROM::PPU_WaitUntilFlushed, FLUSH_ORIG, name);
	require_site(p_rom, ROM::Screen_ResetForGamePlay, RESET_ORIG, name);
	require_site(p_rom, ROM::PPUBuffer_Draw, DRAW_ORIG, name);
	require_site(p_rom, ROM::PPUBuffer_DrawAll, DRAW_ALL_ORIG, name);
	require_site(p_rom, ROM::PPUBuffer_WaitForCapacity, WFC_ORIG, name);
	require_site(p_rom, ROM::PPUBuffer_WaitEmpty, WUC_ORIG, name);

	klib::Asm6502 code;
	code.label("@path");
	if (flag >= 0) {
		// clear: the stock path, byte for byte
		code.lda_abs(static_cast<word>(RAM::Flags + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.bne("@go");
		for (const word target : { ROM::Screen_ResetSpritesForGamePlay, ROM::WaitForNextFrame, ROM::Screen_ClearSprites,
			ROM::Screen_LoadSpriteInfo, ROM::WaitForNextFrame, ROM::EntityChrPass, ROM::PPU_WaitUntilFlushed,
			ROM::Game_UpdateForScroll, ROM::Screen_ResetForGamePlay })
			code.jsr(target);
		code.jmp(ROM::Game_MainLoop);
		code.label("@go");
	}
	// the stores of PPU_WaitUntilFlushed, then the display off, then the stock steps without their waits
	code.lda_imm(0);
	code.sta_zp(RAM::ZP_PauseInterruptCounter);
	code.sta_zp(RAM::ZP_Game_InterruptHandlersEnabled);
	code.sta_zp(RAM::ZP_PPU_ForceLowerPatternTables);
	code.sta_abs(fh::PPU::PPU_MASK);
	for (const word target : { ROM::PPUBuffer_DrawAll, ROM::Screen_ResetSpritesForGamePlay, ROM::Screen_ClearSprites,
		ROM::Screen_LoadSpriteInfo, ROM::EntityChrPass, ROM::PPUBuffer_DrawAll, ROM::Game_UpdateForScroll,
		ROM::Screen_ResetForGamePlay })
		code.jsr(target);
	code.jmp(ROM::Game_MainLoop);
	// the capacity wait: no room and handlers on waits for the nmi as stock; handlers off runs a drain call
	code.label("@wfc");
	code.jsr(ROM::PPUBuffer_HasCapacity); code.bcs("@ok");
	code.lda_zp(RAM::ZP_Game_InterruptHandlersEnabled); code.bne("@wfc");
	code.jsr(ROM::PPUBuffer_Draw); code.jmp("@wfc");
	code.label("@ok"); code.rts();
	// the clear wait: handlers off drains the whole queue; on, the stock wait
	code.label("@wuc");
	code.lda_zp(RAM::ZP_Game_InterruptHandlersEnabled); code.beq("@drain");
	code.label("@wait");
	code.lda_zp(RAM::ZP_PPUBufferWriteCursor); code.cmp_zp(RAM::ZP_PPUBufferReadCursor); code.bne("@wait");
	code.rts();
	code.label("@drain"); code.jmp(ROM::PPUBuffer_DrawAll);

	// in bank 15, or with sram=true in the sram code area. the queue waits
	// are reached from text, hud, password and graphics code in banks 12
	// and 15 with any bank mapped, so the code must always be mapped
	const auto place{ place_hack(p_config, p_rom, p_hack, name, cpu_addr, code.size(), 0) };
	const word path{ code.label_addr("@path", place.addr) };
	const word wfc{ code.label_addr("@wfc", place.addr) };
	const word wuc{ code.label_addr("@wuc", place.addr) };
	const word cursor{ install_placed(p_rom, code, place, cpu_addr, name) };
	hook(p_rom, Game_MainLoop_blankTransition, path, PATH_ORIG.size());
	hook(p_rom, ROM::PPUBuffer_WaitForCapacity, wfc, WFC_ORIG.size());
	hook(p_rom, ROM::PPUBuffer_WaitEmpty, wuc, WUC_ORIG.size());
	return cursor;
}
