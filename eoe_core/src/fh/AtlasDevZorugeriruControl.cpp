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

// zorugeriru control: tunes zorugeriru's falling rocks. zorugeriru never
// moves; it rests, winds up, then drops a rock above you, with at most four
// rocks out; this hack sets how long it rests and winds up, how many rocks
// may be out, how fast they speed up as they fall, and whether its whole body
// can touch you and be hit (its stock box covers only the left half of what
// is drawn). the defaults are the stock numbers, so the hack alone changes
// nothing until a value is set.
//
// four bank 14 instructions are retargeted, where the windup is tested, where
// the rest is set, where the rock count is tested and where a rock's fall
// speed is loaded; the new code goes in bank 15, which is always mapped. the
// box width is one data byte, set only for body=32, and it does not follow the
// flag. a clear flag, or mode=vanilla, is the stock routine. every site is
// checked against its vanilla bytes first, and they are identical in the us,
// us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec }, COUNTER{ 0x02f4 };
	constexpr byte COUNT_ZP{ 0x00 };
	constexpr word SITE_WINDUP{ 0x9de4 }, SITE_REST{ 0x9dee }, SITE_CAP{ 0x9e06 }, SITE_FALL{ 0x9e93 };
	constexpr word V_WINDUP{ 0x9de9 }, V_WINDUP_CMP{ 0x9de7 }, V_REST{ 0x9df3 }, V_CAP{ 0x9e0a };
	constexpr word V_FALL{ 0x9e98 }, V_FALL_LDY{ 0x9e96 };
	constexpr word BOX{ 0xb337 }, BOX_WIDTH{ 0xb339 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into, and zorugeriru's box
	constexpr std::array<byte, 7> WINDUP_ORIG{ 0xbd, 0xec, 0x02, 0xc9, 0x20, 0x90, 0x0b };
	constexpr std::array<byte, 8> REST_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0x4c, 0x13, 0x9e };
	constexpr std::array<byte, 6> CAP_ORIG{ 0xa5, 0x00, 0xc9, 0x04, 0x90, 0x02 };
	constexpr std::array<byte, 8> FALL_ORIG{ 0xbd, 0xf4, 0x02, 0xa0, 0x05, 0x20, 0xd1, 0x83 };
	constexpr std::array<byte, 4> BOX_ORIG{ 0x00, 0x00, 0x10, 0x20 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevZorugeriruControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevZorugeriruControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int rest{ get("rest", 12) }, windup{ get("windup", 32) }, cap{ get("cap", 4) };
	const int fall{ get("fall", 2) }, body{ get("body", 16) };
	if (rest < 1 || rest > 16)
		throw std::runtime_error(name + ": rest must be 1 to 16");
	if (windup < 1 || windup > 255)
		throw std::runtime_error(name + ": windup must be 1 to 255");
	if (cap < 1 || cap > 7)
		throw std::runtime_error(name + ": cap must be 1 to 7");
	if (fall != 1 && fall != 2 && fall != 4)
		throw std::runtime_error(name + ": fall must be 1, 2 or 4");
	if (body != 16 && body != 32)
		throw std::runtime_error(name + ": body must be 16 or 32");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_WINDUP, WINDUP_ORIG, name);
	require_site(p_rom, SITE_REST, REST_ORIG, name);
	require_site(p_rom, SITE_CAP, CAP_ORIG, name);
	require_site(p_rom, SITE_FALL, FALL_ORIG, name);
	require_site(p_rom, BOX, BOX_ORIG, name);

	// the rest check fires once the counter's low nibble reaches 0, so a rest of n frames reloads $30 + n % 16;
	// the rock's fall speed is its counter shifted left by 4, 5 or 6
	const byte reload{ static_cast<byte>(0x30 + rest % 16) };
	const byte shift{ static_cast<byte>(fall == 1 ? 4 : (fall == 2 ? 5 : 6)) };

	// with a flag, only the values that differ from stock need a hook
	const bool windup_hook{ flag >= 0 && windup != 32 }, rest_hook{ flag >= 0 && reload != 0x3c }, cap_hook{ flag >= 0 && cap != 4 }, fall_hook{ flag >= 0 && shift != 5 };
	const int hooks{ (windup_hook ? 1 : 0) + (rest_hook ? 1 : 0) + (cap_hook ? 1 : 0) + (fall_hook ? 1 : 0) };
	klib::Asm6502 code;
	// the flag test: a shared routine once three hooks pay for the calls, else inline in each hook
	const bool shared{ flag >= 0 && hooks >= 3 };
	if (shared) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7))); code.rts();
	}
	auto test = [&](const std::string& p_label) {
		if (shared)
			code.jsr(cpu_addr);
		else {
			code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		}
		code.beq(p_label);
	};
	// the value the store takes: the tuned one with the flag set, the stock one with it clear. the load
	// sets Z, so the branch after it is always taken, and one store serves both paths
	auto select = [&](const std::string& p_name, byte p_value, byte p_stock) {
		test("@v" + p_name);
		code.lda_imm(p_value);
		if (p_value != 0) code.bne("@s" + p_name); else code.beq("@s" + p_name);
		code.label("@v" + p_name); code.lda_imm(p_stock);
		code.label("@s" + p_name);
	};
	word windup_addr{ 0 }, rest_addr{ 0 }, cap_addr{ 0 }, fall_addr{ 0 };
	if (windup_hook) {
		windup_addr = static_cast<word>(cpu_addr + code.size());
		test("@vw");
		code.lda_abs_x(TIMER); code.cmp_imm(static_cast<byte>(windup)); code.jmp(V_WINDUP);
		code.label("@vw"); code.lda_abs_x(TIMER); code.jmp(V_WINDUP_CMP);
	}
	if (rest_hook) {
		rest_addr = static_cast<word>(cpu_addr + code.size());
		select("rest", reload, 0x3c); code.sta_abs_x(TIMER); code.jmp(V_REST);
	}
	if (cap_hook) {
		cap_addr = static_cast<word>(cpu_addr + code.size());
		test("@vc");
		code.lda_zp(COUNT_ZP); code.cmp_imm(static_cast<byte>(cap)); code.jmp(V_CAP);
		code.label("@vc"); code.lda_zp(COUNT_ZP); code.cmp_imm(0x04); code.jmp(V_CAP);
	}
	if (fall_hook) {
		fall_addr = static_cast<word>(cpu_addr + code.size());
		test("@vf");
		code.lda_abs_x(COUNTER); code.ldy_imm(shift); code.jmp(V_FALL);
		code.label("@vf"); code.lda_abs_x(COUNTER); code.jmp(V_FALL_LDY);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9de8) = static_cast<byte>(windup); at(0x9def) = reload; at(0x9e09) = static_cast<byte>(cap); at(0x9e97) = shift;
	}
	// the box width covers the whole drawn body
	if (body == 32)
		p_rom[klib::Asm6502::get_file_offset(BANK14, BOX_WIDTH)] = 0x20;
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (windup_hook) { code.jmp(windup_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WINDUP); }
	if (rest_hook) { code.jmp(rest_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_REST); }
	if (cap_hook) { code.jmp(cap_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_CAP); }
	if (fall_hook) { code.jmp(fall_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_FALL); }
	return static_cast<word>(cpu_addr + size);
}
