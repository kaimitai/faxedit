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

// borabohra control: tunes borabohra. after it rises, borabohra glides
// sideways at you forever, its speed swelling from nothing to about one pixel
// a frame and back, and it turns toward you on every frame. this hack sets
// the top speed, how long each swell lasts and how often it turns toward you,
// so it can glide past you; body=32 makes its box as wide as its wings. the
// defaults are the stock numbers, so the hack alone changes nothing until a
// value is set.
//
// one bank 14 instruction is retargeted, where the glide speed is worked out,
// and with turn above 1 also the turn test; the new code goes in bank 15,
// which is always mapped. body=32 rewrites two bytes of its box. a clear flag,
// or mode=vanilla, is the stock routine. every site is checked against its
// vanilla bytes first, and they are identical in the us, us rev a, eu and jp
// roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used (a zero shift and a turn above 1
// still need their hooks); with a flag only the ones whose values differ from
// stock are retargeted. at the stock values nothing is written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec }, SPEED_LO{ 0x0374 }, SPEED_HI{ 0x0375 };
	constexpr word SITE_GLIDE{ 0x9cb5 }, SITE_TURN{ 0x9cc8 }, BOX{ 0xb32f };
	constexpr word WAVE{ 0x83e1 }, SCALE{ 0x83c1 };
	constexpr word V_MOVE{ 0x9cc2 }, V_WAVE{ 0x9cb8 }, V_FACE{ 0x9ccc }, V_DONE{ 0x9ccf };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 13> GLIDE_ORIG{ 0xbd, 0xec, 0x02, 0xa0, 0x02, 0x20, 0xe1, 0x83, 0xa0, 0x02, 0x20, 0xc1, 0x83 };
	constexpr std::array<byte, 8> TURN_ORIG{ 0x29, 0x7f, 0xd0, 0x03, 0x20, 0x7b, 0x86, 0x60 };
	constexpr std::array<byte, 4> BOX_ORIG{ 0x04, 0x00, 0x18, 0x30 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	int wave_index(int p_loop) {
		switch (p_loop) {
		case 256: return 1;
		case 128: return 2;
		case 64: return 3;
		case 32: return 4;
		default: return -1;
		}
	}

	int speed_log(int p_speed) {
		switch (p_speed) {
		case 2: return -2;
		case 4: return -1;
		case 8: return 0;
		case 16: return 1;
		case 32: return 2;
		default: return -9;
		}
	}
}

word fh::HackManager::install_AtlasDevBorabohraControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevBorabohraControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int speed{ get("speed", 8) }, loop{ get("loop", 128) }, turn{ get("turn", 1) }, body{ get("body", 24) };
	const int k{ wave_index(loop) }, j{ speed_log(speed) };
	if (j < -2)
		throw std::runtime_error(name + ": speed must be 2, 4, 8, 16 or 32");
	if (k < 0)
		throw std::runtime_error(name + ": loop must be 32, 64, 128 or 256");
	if (k + j < 0)
		throw std::runtime_error(name + ": speed=2 needs a loop of 128 or less");
	if (turn < 1 || turn > 128 || (turn & (turn - 1)) != 0)
		throw std::runtime_error(name + ": turn must be 1, 2, 4, 8, 16, 32, 64 or 128");
	if (body != 24 && body != 32)
		throw std::runtime_error(name + ": body must be 24 or 32");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;
	const int s{ k + j };

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_GLIDE, GLIDE_ORIG, name);
	if (turn > 1)
		require_site(p_rom, SITE_TURN, TURN_ORIG, name);
	if (body == 32)
		require_site(p_rom, BOX, BOX_ORIG, name);

	// a value left at stock needs nothing
	const bool glide_hook{ (k != 2 || s != 2) && (flag >= 0 || s == 0) }, turn_hook{ turn > 1 };
	klib::Asm6502 code;
	// the flag test, inline in each hook: with at most two of them that is smaller than a shared routine
	auto test = [&](const std::string& p_label) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.beq(p_label);
	};
	word glide_addr{ 0 }, turn_addr{ 0 };
	if (glide_hook) {
		glide_addr = static_cast<word>(cpu_addr + code.size());
		if (flag >= 0) test("@vg");
		code.lda_abs_x(TIMER); code.ldy_imm(static_cast<byte>(k)); code.jsr(WAVE);
		if (s > 0) {
			code.ldy_imm(static_cast<byte>(s)); code.jsr(SCALE);
		}
		else {
			// the stock scaler would loop 256 times on a zero shift; y must still end at zero, as the scaler leaves it
			code.sta_abs(SPEED_LO); code.lda_imm(0x00); code.sta_abs(SPEED_HI); code.tay();
		}
		code.jmp(V_MOVE);
		if (flag >= 0) { code.label("@vg"); code.lda_abs_x(TIMER); code.jmp(V_WAVE); }
	}
	if (turn_hook) {
		turn_addr = static_cast<word>(cpu_addr + code.size());
		if (flag >= 0) { code.pha(); test("@vt"); code.pla(); }
		code.lda_abs_x(TIMER); code.and_imm(static_cast<byte>(turn - 1)); code.beq(3); code.jmp(V_DONE); code.jmp(V_FACE);
		if (flag >= 0) { code.label("@vt"); code.pla(); code.and_imm(0x7f); code.bne(3); code.jmp(V_FACE); code.jmp(V_DONE); }
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		if (s > 0) {
			at(0x9cb9) = static_cast<byte>(k); at(0x9cbe) = static_cast<byte>(s);
		}
	}
	if (body == 32) {
		// the box record: x offset 4 -> 0, width 24 -> 32
		const auto box{ klib::Asm6502::get_file_offset(BANK14, BOX) };
		p_rom[box] = 0x00;
		p_rom[box + 2] = 0x20;
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (glide_hook) { code.jmp(glide_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_GLIDE); }
	if (turn_hook) { code.jmp(turn_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_TURN); }
	return static_cast<word>(cpu_addr + size);
}
