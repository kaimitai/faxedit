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

// bihoruda control: tunes bihoruda's two swoops. sideways and up and down, its
// speed rises and falls in the stock triangle wave; each axis gets a peak
// speed in eighths of a pixel per frame and a loop length in frames, both
// powers of two. the defaults are the stock numbers, so the hack alone changes
// nothing until a value is set. walls still turn it, through the game's own
// movers.
//
// one bank 14 instruction is retargeted: the first load of its movement, at
// $8ee1, reached after its setup. the hook runs the same movement with the
// chosen numbers, through the game's own wave and shift routines, which are
// only called, never changed. a clear flag, or mode=vanilla, is the stock
// routine. every site is checked against its vanilla bytes first, and they are
// identical in the us, us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; a half-pixel peak on a 256-frame loop
// cannot call the stock scaler, and keeps a nine byte routine per axis. with a
// flag only the axis whose values differ from stock is hooked, and the hook
// rejoins the stock movement where its change ends. at the stock values nothing
// is written.
namespace {
	constexpr byte BANK{ 14 };
	constexpr word T1{ 0x02ec }, T2{ 0x02f4 }, DX_FRAC{ 0x0374 }, DX_FULL{ 0x0375 }, DY_FRAC{ 0x0376 }, DY_FULL{ 0x0377 };
	constexpr word TRI{ 0x83e1 }, SHIFT_X{ 0x83c1 }, SHIFT_Y{ 0x83d1 }, MOVE_X{ 0x8419 }, MOVE_Y{ 0x8564 };
	constexpr word FLAG_BASE{ 0x0101 }, SITE_INIT{ 0x8ecf }, SITE{ 0x8ee1 }, V_CONT{ 0x8ee4 }, HELPERS{ 0x83c1 };
	constexpr int MAX_FLAG{ 247 };
	// the second site (the y axis's timer load), each axis's scaler call and its operand, and the mover
	// call its swoop ends at: a hook rejoins the stock routine wherever its change ends
	constexpr word SITE_Y{ 0x8efc }, X_SCALE{ 0x8eeb }, X_SCALE_OP{ 0x8eec }, X_MOVER{ 0x8ef6 };
	constexpr word Y_SCALE{ 0x8f06 }, Y_SCALE_OP{ 0x8f07 }, Y_MOVER{ 0x8f11 };
	constexpr int STOCK_CAP_X{ 1 }, STOCK_CAP_Y{ 1 };

	constexpr std::array<byte, 18> INIT_ORIG{ 0x20, 0x85, 0xa8, 0xd0, 0x0d, 0xa9, 0x00, 0x9d, 0xec, 0x02, 0xa9, 0x40, 0x9d, 0xf4, 0x02, 0x20, 0x94, 0xa8 };
	constexpr std::array<byte, 3> SITE_ORIG{ 0xbd, 0xec, 0x02 };
	constexpr std::array<byte, 52> BODY_ORIG{
		0xa0, 0x02, 0x20, 0xe1, 0x83, 0xa0, 0x03, 0x20, 0xc1, 0x83, 0xad, 0x75, 0x03, 0x29, 0x01, 0x8d,
		0x75, 0x03, 0x20, 0x19, 0x84, 0xfe, 0xec, 0x02, 0xbd, 0xf4, 0x02, 0xa0, 0x02, 0x20, 0xe1, 0x83,
		0xa0, 0x03, 0x20, 0xd1, 0x83, 0xad, 0x77, 0x03, 0x29, 0x01, 0x8d, 0x77, 0x03, 0x20, 0x64, 0x85,
		0xfe, 0xf4, 0x02, 0x60 };
	constexpr std::array<byte, 70> HELPERS_ORIG{
		0x8d, 0x74, 0x03, 0xa9, 0x00, 0x0e, 0x74, 0x03, 0x2a, 0x88, 0xd0, 0xf9, 0x8d, 0x75, 0x03, 0x60,
		0x8d, 0x76, 0x03, 0xa9, 0x00, 0x0e, 0x76, 0x03, 0x2a, 0x88, 0xd0, 0xf9, 0x8d, 0x77, 0x03, 0x60,
		0x48, 0x39, 0xf7, 0x83, 0x85, 0x00, 0x68, 0x39, 0xff, 0x83, 0xf0, 0x07, 0xb9, 0xf7, 0x83, 0x38,
		0xe5, 0x00, 0x60, 0xa5, 0x00, 0x60, 0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00, 0x80,
		0x40, 0x20, 0x10, 0x08, 0x04, 0x02 };
	constexpr std::array<byte, 14> MOVE_X_ORIG{ 0x20, 0x94, 0x84, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x01, 0x9d, 0xdc, 0x02, 0x60 };
	constexpr std::array<byte, 17> MOVE_Y_ORIG{ 0x20, 0xca, 0x85, 0x20, 0x75, 0x85, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x80, 0x9d, 0xdc, 0x02, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	// klib::Asm6502 has no inc abs,x
	void inc_abs_x(klib::Asm6502& c, word p_addr) {
		c.db(0xfe); c.db(static_cast<byte>(p_addr & 0xff)); c.db(static_cast<byte>(p_addr >> 8));
	}

	int loop_index(int p_loop) {   // frames per swoop -> the wave routine's index
		for (int k{ 1 }; k <= 6; ++k)
			if (p_loop == (1 << (9 - k)))
				return k;
		return -1;
	}

	int peak_log(int p_peak) {     // eighths of a pixel -> log2 of pixels
		for (int j{ -1 }; j <= 3; ++j)
			if (p_peak == (1 << (j + 3)))
				return j;
		return -9;
	}

	// one axis: the stock triangle of its timer, scaled into its speed cells and capped, then either the
	// stock mover call and the timer step, or a jump into the stock routine where the change ends
	void swoop(klib::Asm6502& c, word p_timer, int p_k, int p_s, int p_cap, word p_lo, word p_hi, word p_shifter,
		word p_mover, word p_tail) {
		c.lda_abs_x(p_timer); c.ldy_imm(static_cast<byte>(p_k)); c.jsr(TRI);   // the stock triangle wave
		if (p_s > 0) {
			c.ldy_imm(static_cast<byte>(p_s)); c.jsr(p_shifter);                 // shifted into the speed
		}
		else {
			c.sta_abs(p_lo); c.lda_imm(0x00); c.sta_abs(p_hi);                    // no shift: stored as is
		}
		c.lda_abs(p_hi); c.and_imm(static_cast<byte>(p_cap)); c.sta_abs(p_hi);
		if (p_tail != 0) {
			c.jmp(p_tail);
			return;
		}
		c.jsr(p_mover);
		inc_abs_x(c, p_timer);
	}
}

word fh::HackManager::install_AtlasDevBihorudaControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevBihorudaControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int xspeed{ get("xspeed", 16) }, xloop{ get("xloop", 128) }, yspeed{ get("yspeed", 16) }, yloop{ get("yloop", 128) };
	const int kx{ loop_index(xloop) }, ky{ loop_index(yloop) }, jx{ peak_log(xspeed) }, jy{ peak_log(yspeed) };
	if (jx < -1 || jy < -1)
		throw std::runtime_error(name + ": xspeed and yspeed must be 4, 8, 16, 32 or 64");
	if (kx < 0 || ky < 0)
		throw std::runtime_error(name + ": xloop and yloop must be 8, 16, 32, 64, 128 or 256");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_INIT, INIT_ORIG, name);
	require_site(p_rom, SITE, SITE_ORIG, name);
	require_site(p_rom, V_CONT, BODY_ORIG, name);
	require_site(p_rom, HELPERS, HELPERS_ORIG, name);
	require_site(p_rom, MOVE_X, MOVE_X_ORIG, name);
	require_site(p_rom, MOVE_Y, MOVE_Y_ORIG, name);

	const int sx{ jx + kx }, sy{ jy + ky };
	const int cap_x{ std::max(0, xspeed / 8 - 1) }, cap_y{ std::max(0, yspeed / 8 - 1) };
	const bool x_changed{ xspeed != 16 || xloop != 128 }, y_changed{ yspeed != 16 || yloop != 128 };
	// every value at stock: nothing to install
	if (!x_changed && !y_changed)
		return cpu_addr;

	klib::Asm6502 code;
	word x_scale_to{ 0 }, y_scale_to{ 0 };
	const bool y_only{ y_changed && !x_changed };
	if (flag < 0) {
		// an axis at shift zero cannot call the stock scaler, which would shift 256 times: its call is
		// retargeted to a nine-byte routine that stores the wave and clears the whole pixels, leaving y
		// at zero as the scaler leaves it
		if (sx == 0) {
			x_scale_to = static_cast<word>(cpu_addr + code.size());
			code.sta_abs(DX_FRAC); code.lda_imm(0x00); code.sta_abs(DX_FULL); code.rts();
		}
		if (sy == 0) {
			y_scale_to = static_cast<word>(cpu_addr + code.size());
			code.sta_abs(DY_FRAC); code.lda_imm(0x00); code.sta_abs(DY_FULL); code.rts();
		}
	}
	else {
		// the hook covers the axes that differ from stock and rejoins the stock routine where the change
		// ends: at the scaler call when only the loop differs, else at that axis's mover call
		const word site{ y_only ? SITE_Y : SITE };
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.bne("@on");
		code.lda_abs_x(y_only ? T2 : T1); code.jmp(static_cast<word>(site + 3));   // flag clear: stock
		code.label("@on");
		if (x_changed && y_changed) {
			swoop(code, T1, kx, sx, cap_x, DX_FRAC, DX_FULL, SHIFT_X, MOVE_X, 0);
			swoop(code, T2, ky, sy, cap_y, DY_FRAC, DY_FULL, SHIFT_Y, MOVE_Y, Y_MOVER);
		}
		else if (x_changed) {
			if (cap_x == STOCK_CAP_X && sx > 0) {
				code.lda_abs_x(T1); code.ldy_imm(static_cast<byte>(kx)); code.jsr(TRI);
				code.ldy_imm(static_cast<byte>(sx)); code.jmp(X_SCALE);
			}
			else
				swoop(code, T1, kx, sx, cap_x, DX_FRAC, DX_FULL, SHIFT_X, MOVE_X, X_MOVER);
		}
		else {
			if (cap_y == STOCK_CAP_Y && sy > 0) {
				code.lda_abs_x(T2); code.ldy_imm(static_cast<byte>(ky)); code.jsr(TRI);
				code.ldy_imm(static_cast<byte>(sy)); code.jmp(Y_SCALE);
			}
			else
				swoop(code, T2, ky, sy, cap_y, DY_FRAC, DY_FULL, SHIFT_Y, MOVE_Y, Y_MOVER);
		}
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));

	// without a flag each axis's wave index, shift and cap go straight into the stock movement, and no
	// free space is used unless an axis sits at shift zero
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK, p_addr)]; };
		at(0x8ee5) = static_cast<byte>(kx); if (sx > 0) at(0x8eea) = static_cast<byte>(sx);
		at(0x8ef2) = static_cast<byte>(cap_x);
		at(0x8f00) = static_cast<byte>(ky); if (sy > 0) at(0x8f05) = static_cast<byte>(sy);
		at(0x8f0d) = static_cast<byte>(cap_y);
		if (size == 0)
			return cpu_addr;
		code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
		if (sx == 0) klib::Asm6502::apply_word(p_rom, x_scale_to, BANK, X_SCALE_OP);
		if (sy == 0) klib::Asm6502::apply_word(p_rom, y_scale_to, BANK, Y_SCALE_OP);
		return static_cast<word>(cpu_addr + size);
	}
	code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
	code.jmp(cpu_addr); code.apply_hack_and_clear(p_rom, BANK, y_only ? SITE_Y : SITE);
	return static_cast<word>(cpu_addr + size);
}
