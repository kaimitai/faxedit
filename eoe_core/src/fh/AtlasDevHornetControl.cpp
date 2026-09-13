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

// hornet control: tunes how the hornet flies. it moves sideways at speed
// eighths of a pixel per frame and bobs up and down in the stock wave shape,
// peaking at bob eighths, one wave step every period frames. the defaults are
// the stock numbers, so the hack alone changes nothing until a value is set.
// walls and blocks still turn it, through the game's own movers.
//
// one bank 14 site is retargeted: the five bytes at $8e87 in the hornet's
// flying routine that set its sideways speed, reached after the routine's
// setup. the hook sets both speeds, calls the two stock movers and returns
// for the routine. a clear flag, or mode=vanilla, is the stock routine. every
// site is checked against its vanilla bytes first, and they are identical in
// the us, us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; a period of 16 is the one shift that
// does not fit, and keeps a seven byte hook. with a flag the hook covers only
// the knobs whose values differ from stock, and rejoins the stock routine where
// its change ends. at the stock values nothing is written.
namespace {
	constexpr byte BANK{ 14 };
	constexpr word DX_FRAC{ 0x0374 }, DX_FULL{ 0x0375 }, DY_FRAC{ 0x0376 }, DY_FULL{ 0x0377 };
	constexpr word TIMER{ 0x02ec }, FLAG_BASE{ 0x0101 }, MOVE_X{ 0x8419 }, MOVE_Y{ 0x8564 };
	constexpr word SITE_INIT{ 0x8e77 }, SITE{ 0x8e87 }, V_CONT{ 0x8e8c };
	// the stock routine after the site: its sideways mover call, its three lsr and the and that follow,
	// its table load, and the store of the whole up/down speed. a hook rejoins wherever its change ends
	constexpr word V_JSR{ 0x8e91 }, SHIFT_AT{ 0x8e97 }, V_AND{ 0x8e9a }, V_LOAD{ 0x8e9d }, V_STA_FULL{ 0x8ea6 };
	constexpr int MAX_SPEED{ 64 }, MAX_BOB{ 64 }, MAX_FLAG{ 247 };

	// the setup, the site, the rest of the routine with its two speed tables,
	// and the two movers the hook calls
	constexpr std::array<byte, 16> INIT_ORIG{ 0x20, 0x85, 0xa8, 0xd0, 0x0b, 0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x9d, 0xec, 0x02, 0x20, 0x94, 0xa8 };
	constexpr std::array<byte, 5> SITE_ORIG{ 0xa9, 0x00, 0x8d, 0x74, 0x03 };
	constexpr std::array<byte, 52> BODY_ORIG{
		0xa9, 0x02, 0x8d, 0x75, 0x03, 0x20, 0x19, 0x84, 0xbd, 0xec, 0x02, 0x4a, 0x4a, 0x4a, 0x29, 0x07,
		0xa8, 0xb9, 0xb0, 0x8e, 0x8d, 0x76, 0x03, 0xb9, 0xb8, 0x8e, 0x8d, 0x77, 0x03, 0x20, 0x64, 0x85,
		0xfe, 0xec, 0x02, 0x60, 0x00, 0x00, 0x80, 0x40, 0x00, 0x40, 0x80, 0x00, 0x02, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01 };
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

	// the stock wave: peak, a half, a quarter, an eighth, still, and back
	constexpr std::array<int, 8> WAVE_SHIFT{ 0, 1, 2, 3, -1, 3, 2, 1 };

	// the flag build's hook. it redoes the speed stores and then rejoins the stock routine where its
	// change ends: at the mover call when only the speed differs, after the shift when the bob is stock,
	// else at the store of the whole up/down speed, carrying its own tables
	void emit_hook(klib::Asm6502& c, int p_speed, int p_bob, int p_shift, int p_flag, word p_table,
		bool p_bob_changed, bool p_period_changed) {
		c.lda_abs(static_cast<word>(FLAG_BASE + (p_flag >> 3))); c.and_imm(static_cast<byte>(1 << (p_flag & 7)));
		c.bne("@on");
		c.sta_abs(DX_FRAC); c.jmp(V_CONT);        // flag clear: the test leaves a zero, the stock lda #$00
		c.label("@on");
		const int v{ p_speed * 32 };
		c.lda_imm(static_cast<byte>(v & 0xff)); c.sta_abs(DX_FRAC);
		c.lda_imm(static_cast<byte>(v >> 8)); c.sta_abs(DX_FULL);
		if (!p_bob_changed && !p_period_changed) {
			c.jmp(V_JSR);
			return;
		}
		c.jsr(MOVE_X);                                                                 // sideways, turns at walls
		c.lda_abs_x(TIMER);
		if (p_shift > 0)
			c.lsr_a(static_cast<std::size_t>(p_shift));
		if (p_shift != 5)
			c.and_imm(0x07);                          // five shifts already leave a wave step of 0 to 7
		c.tay();
		if (!p_bob_changed) {
			c.jmp(V_LOAD);
			return;
		}
		c.lda_abs_y(p_table); c.sta_abs(DY_FRAC);
		c.lda_abs_y(static_cast<word>(p_table + 8)); c.jmp(V_STA_FULL);
		for (int i{ 0 }; i < 8; ++i)
			c.db(static_cast<byte>(WAVE_SHIFT[i] < 0 ? 0 : ((p_bob * 32) >> WAVE_SHIFT[i]) & 0xff));
		for (int i{ 0 }; i < 8; ++i)
			c.db(static_cast<byte>(WAVE_SHIFT[i] < 0 ? 0 : ((p_bob * 32) >> WAVE_SHIFT[i]) >> 8));
	}
}

word fh::HackManager::install_AtlasDevHornetControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevHornetControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default, int p_max) {
		if (!p_hack.has_param(p_id))
			return p_default;
		const int v{ static_cast<int>(p_hack.word_or(p_id, 0)) };
		if (v > p_max)
			throw std::runtime_error(std::format("{}: {} must be 0 to {}", name, p_id, p_max));
		return v;
	};
	const int speed{ get("speed", 16, MAX_SPEED) };
	const int bob{ get("bob", 16, MAX_BOB) };
	const int period{ get("period", 8, 32) };
	int shift{ -1 };
	for (int s{ 0 }; s <= 5; ++s)
		if (period == (1 << s))
			shift = s;
	if (shift < 0)
		throw std::runtime_error(name + ": period must be 1, 2, 4, 8, 16 or 32");
	const int flag{ get("flag", -1, MAX_FLAG) };
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_INIT, INIT_ORIG, name);
	require_site(p_rom, SITE, SITE_ORIG, name);
	require_site(p_rom, V_CONT, BODY_ORIG, name);
	require_site(p_rom, MOVE_X, MOVE_X_ORIG, name);
	require_site(p_rom, MOVE_Y, MOVE_Y_ORIG, name);

	const bool bob_changed{ bob != 16 }, period_changed{ shift != 3 };
	// every value at stock: nothing to install
	if (speed == 16 && !bob_changed && !period_changed)
		return cpu_addr;

	// the code this build needs. without a flag the numbers go into the stock routine itself, and only
	// a period of 16 keeps a hook: its four shifts do not fit the three lsr, so seven bytes shift and
	// jump back to the stock and #$07. with a flag the hook covers every knob that differs from stock
	klib::Asm6502 probe;
	if (flag < 0) {
		if (shift == 4) { probe.lsr_a(4); probe.jmp(V_AND); }
	}
	else
		emit_hook(probe, speed, bob, shift, flag, 0, bob_changed, period_changed);
	const std::size_t size{ probe.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));

	klib::Asm6502 code;
	if (flag < 0) {
		// the speed's two lda operands and the two eight-byte speed tables always; the timer's shifts
		// over the three lsr when they fit (nops fill the ones a shorter period drops), and a period of
		// 32 shifts five times over the lsr and the and too, since the timer over 32 is already 0 to 7
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK, p_addr)]; };
		const int v{ speed * 32 };
		at(0x8e88) = static_cast<byte>(v & 0xff);
		at(0x8e8d) = static_cast<byte>(v >> 8);
		for (int i{ 0 }; i < 8; ++i) {
			const int w{ WAVE_SHIFT[i] < 0 ? 0 : (bob * 32) >> WAVE_SHIFT[i] };
			at(static_cast<word>(0x8eb0 + i)) = static_cast<byte>(w & 0xff);
			at(static_cast<word>(0x8eb8 + i)) = static_cast<byte>(w >> 8);
		}
		if (shift <= 3)
			for (int i{ 0 }; i < 3; ++i)
				at(static_cast<word>(SHIFT_AT + i)) = i < shift ? 0x4a : 0xea;
		else if (shift == 5)
			for (int i{ 0 }; i < 5; ++i)
				at(static_cast<word>(SHIFT_AT + i)) = 0x4a;
		if (size == 0)
			return cpu_addr;
		code.lsr_a(4); code.jmp(V_AND);
		code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
		code.jmp(cpu_addr); code.apply_hack_and_clear(p_rom, BANK, SHIFT_AT);
		return static_cast<word>(cpu_addr + size);
	}
	emit_hook(code, speed, bob, shift, flag, static_cast<word>(cpu_addr + size - 16), bob_changed, period_changed);
	code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
	code.jmp(cpu_addr); code.apply_hack_and_clear(p_rom, BANK, SITE);
	return static_cast<word>(cpu_addr + size);
}
