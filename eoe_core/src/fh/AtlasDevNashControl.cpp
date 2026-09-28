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

// nash control: tunes nash. nash hides, appears near you, faces you for a
// while, then attacks and throws at you before it hides again. this hack sets
// how long it stays hidden, how long it faces you before the attack, how long
// the attack lasts and when in the attack it throws. the defaults are the
// stock numbers, so the hack alone changes nothing until a value is set. what
// it throws stays stock, since other monsters throw it too.
//
// five bank 14 instructions are retargeted, where the first time hidden is
// set, where the throw moment is tested, where each later time hidden is set,
// where the windup is set and where the attack length is set. the new code
// goes in bank 15, which is always mapped. a clear flag, or mode=vanilla, is
// the stock routine. every site is checked against its vanilla bytes first,
// and they are identical in the us, us rev a, eu and jp roms. no ram is
// claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec };
	constexpr word SITE_INIT{ 0x9065 }, SITE_THROW{ 0x907d }, SITE_HIDE{ 0x9089 }, SITE_WINDUP{ 0x909d }, SITE_ATTACK{ 0x90d6 };
	constexpr word V_INIT{ 0x906a }, V_THROW{ 0x9081 }, V_NO_THROW{ 0x908e }, V_HIDE{ 0x908e }, V_WINDUP{ 0x90a2 }, V_ATTACK{ 0x90db };
	constexpr byte STOCK_HIDE{ 0x78 }, STOCK_WINDUP{ 0x3c }, STOCK_ATTACK{ 0x3c }, STOCK_THROW{ 0x0a };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 8> INIT_ORIG{ 0xa9, 0x78, 0x9d, 0xec, 0x02, 0x20, 0x94, 0xa8 };
	constexpr std::array<byte, 7> THROW_ORIG{ 0xc9, 0x0a, 0xd0, 0x0d, 0x4c, 0xa0, 0xa0 };
	constexpr std::array<byte, 6> HIDE_ORIG{ 0xa9, 0x78, 0x9d, 0xec, 0x02, 0x60 };
	constexpr std::array<byte, 7> WINDUP_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0xa5, 0xa4 };
	constexpr std::array<byte, 8> ATTACK_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0xfe, 0xe4, 0x02 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevNashControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevNashControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int hide{ get("hide", 120) }, windup{ get("windup", 60) }, attack{ get("attack", 60) }, throw_at{ get("throw", 10) };
	if (hide < 16 || hide > 255)
		throw std::runtime_error(name + ": hide must be 16 to 255");
	if (windup < 16 || windup > 255)
		throw std::runtime_error(name + ": windup must be 16 to 255");
	if (attack < 16 || attack > 255)
		throw std::runtime_error(name + ": attack must be 16 to 255");
	// the attack counter falls from attack to zero, so a throw at attack or above would never come
	if (throw_at < 1 || throw_at >= attack)
		throw std::runtime_error(name + ": throw must be 1 to attack - 1");
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
	require_site(p_rom, SITE_THROW, THROW_ORIG, name);
	require_site(p_rom, SITE_HIDE, HIDE_ORIG, name);
	require_site(p_rom, SITE_WINDUP, WINDUP_ORIG, name);
	require_site(p_rom, SITE_ATTACK, ATTACK_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool init_hook{ flag >= 0 && hide != 120 }, throw_hook{ flag >= 0 && throw_at != 10 }, hide_hook{ flag >= 0 && hide != 120 }, windup_hook{ flag >= 0 && windup != 60 }, attack_hook{ flag >= 0 && attack != 60 };
	const int hooks{ (init_hook ? 1 : 0) + (throw_hook ? 1 : 0) + (hide_hook ? 1 : 0) + (windup_hook ? 1 : 0) + (attack_hook ? 1 : 0) };
	klib::Asm6502 code;
	// the flag test: a shared routine once three hooks pay for the calls, else inline in each hook
	const bool shared{ flag >= 0 && hooks >= 3 };
	if (shared) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7))); code.rts();
	}
	auto flag_test = [&]() {
		if (shared)
			code.jsr(cpu_addr);
		else {
			code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		}
	};
	auto test = [&](const std::string& p_label) { flag_test(); code.beq(p_label); };
	// the value the store takes: the tuned one with the flag set, the stock one with it clear. the load
	// sets Z, so the branch after it is always taken, and one store serves both paths
	auto select = [&](const std::string& p_name, byte p_value, byte p_stock) {
		test("@v" + p_name);
		code.lda_imm(p_value);
		if (p_value != 0) code.bne("@s" + p_name); else code.beq("@s" + p_name);
		code.label("@v" + p_name); code.lda_imm(p_stock);
		code.label("@s" + p_name);
	};
	word init_addr{ 0 }, throw_addr{ 0 }, hide_addr{ 0 }, windup_addr{ 0 }, attack_addr{ 0 };
	if (init_hook) {
		init_addr = static_cast<word>(cpu_addr + code.size());
		select("init", static_cast<byte>(hide), STOCK_HIDE); code.sta_abs_x(TIMER); code.jmp(V_INIT);
	}
	if (throw_hook) {
		throw_addr = static_cast<word>(cpu_addr + code.size());
		// the hook runs with the attack counter in A: it keeps it across the test, and both paths share the exit
		code.pha(); flag_test(); code.bne("@tt");
		code.pla(); code.cmp_imm(STOCK_THROW); code.jmp("@ct");
		code.label("@tt"); code.pla();
		code.cmp_imm(static_cast<byte>(throw_at));
		code.label("@ct"); code.bne(3); code.jmp(V_THROW); code.jmp(V_NO_THROW);
	}
	if (hide_hook) {
		hide_addr = static_cast<word>(cpu_addr + code.size());
		select("hide", static_cast<byte>(hide), STOCK_HIDE); code.sta_abs_x(TIMER); code.jmp(V_HIDE);
	}
	if (windup_hook) {
		windup_addr = static_cast<word>(cpu_addr + code.size());
		select("windup", static_cast<byte>(windup), STOCK_WINDUP); code.sta_abs_x(TIMER); code.jmp(V_WINDUP);
	}
	if (attack_hook) {
		attack_addr = static_cast<word>(cpu_addr + code.size());
		select("attack", static_cast<byte>(attack), STOCK_ATTACK); code.sta_abs_x(TIMER); code.jmp(V_ATTACK);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9066) = static_cast<byte>(hide); at(0x908a) = static_cast<byte>(hide); at(0x907e) = static_cast<byte>(throw_at);
		at(0x909e) = static_cast<byte>(windup); at(0x90d7) = static_cast<byte>(attack);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (init_hook) { code.jmp(init_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_INIT); }
	if (throw_hook) { code.jmp(throw_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_THROW); }
	if (hide_hook) { code.jmp(hide_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_HIDE); }
	if (windup_hook) { code.jmp(windup_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WINDUP); }
	if (attack_hook) { code.jmp(attack_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_ATTACK); }
	return static_cast<word>(cpu_addr + size);
}
