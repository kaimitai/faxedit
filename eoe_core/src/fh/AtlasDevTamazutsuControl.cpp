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

// tamazutsu control: tunes tamazutsu. tamazutsu never moves; it waits
// underground, blinks a warning, pops up, stays up for a while and sinks
// again; this hack sets how long it stays underground, how long it stays up
// and how long the warning blinks before it rises. the defaults are the stock
// numbers, so the hack alone changes nothing until a value is set. the rise
// and the sink keep their stock length.
//
// four bank 14 instructions are retargeted, where the first time underground
// is set, where each later time underground is set, where the time up is set
// and where the warning blink is tested. the new code goes in bank 15, which
// is always mapped. a clear flag, or mode=vanilla, is the stock routine. every
// site is checked against its vanilla bytes first, and they are identical in
// the us, us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec };
	constexpr word SITE_INIT{ 0x986a }, SITE_UP{ 0x9892 }, SITE_HIDE{ 0x98a2 }, SITE_WARN{ 0x98ce };
	constexpr word V_INIT{ 0x986f }, V_COUNT{ 0x98a7 }, V_BLINK{ 0x98d2 }, V_NO_DRAW{ 0x98b2 };
	constexpr byte STOCK_LENGTH{ 0x3c }, STOCK_WARN{ 0x1e };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 7> INIT_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0xa9, 0x00 };
	constexpr std::array<byte, 5> UP_ORIG{ 0xa9, 0x3c, 0x4c, 0xa7, 0x98 };
	constexpr std::array<byte, 5> HIDE_ORIG{ 0xa9, 0x3c, 0x4c, 0xa7, 0x98 };
	constexpr std::array<byte, 6> WARN_ORIG{ 0xc9, 0x1e, 0xb0, 0xe0, 0xa0, 0x00 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevTamazutsuControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevTamazutsuControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int hide{ get("hide", 60) }, up{ get("up", 60) }, warn{ get("warn", 30) };
	if (hide < 16 || hide > 255)
		throw std::runtime_error(name + ": hide must be 16 to 255");
	if (up < 16 || up > 255)
		throw std::runtime_error(name + ": up must be 16 to 255");
	if (warn < 1 || warn > 255)
		throw std::runtime_error(name + ": warn must be 1 to 255");
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
	require_site(p_rom, SITE_UP, UP_ORIG, name);
	require_site(p_rom, SITE_HIDE, HIDE_ORIG, name);
	require_site(p_rom, SITE_WARN, WARN_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool init_hook{ flag >= 0 && hide != 60 }, hide_hook{ flag >= 0 && hide != 60 }, up_hook{ flag >= 0 && up != 60 }, warn_hook{ flag >= 0 && warn != 30 };
	const int hooks{ (init_hook ? 1 : 0) + (hide_hook ? 1 : 0) + (up_hook ? 1 : 0) + (warn_hook ? 1 : 0) };
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
	// the value the load takes: the tuned one with the flag set, the stock one with it clear. the load
	// sets Z, so the branch after it is always taken, and one tail serves both paths
	auto select = [&](const std::string& p_name, byte p_value, byte p_stock) {
		test("@v" + p_name);
		code.lda_imm(p_value);
		if (p_value != 0) code.bne("@s" + p_name); else code.beq("@s" + p_name);
		code.label("@v" + p_name); code.lda_imm(p_stock);
		code.label("@s" + p_name);
	};
	word init_addr{ 0 }, hide_addr{ 0 }, up_addr{ 0 }, warn_addr{ 0 };
	if (init_hook) {
		init_addr = static_cast<word>(cpu_addr + code.size());
		select("init", static_cast<byte>(hide), STOCK_LENGTH); code.sta_abs_x(TIMER); code.jmp(V_INIT);
	}
	if (hide_hook) {
		hide_addr = static_cast<word>(cpu_addr + code.size());
		select("hide", static_cast<byte>(hide), STOCK_LENGTH); code.jmp(V_COUNT);
	}
	if (up_hook) {
		up_addr = static_cast<word>(cpu_addr + code.size());
		select("up", static_cast<byte>(up), STOCK_LENGTH); code.jmp(V_COUNT);
	}
	if (warn_hook) {
		warn_addr = static_cast<word>(cpu_addr + code.size());
		// the hook runs with the counter in A: it keeps it across the test, and both paths share the exit
		code.pha(); flag_test(); code.bne("@tw");
		code.pla(); code.cmp_imm(STOCK_WARN); code.jmp("@cw");
		code.label("@tw"); code.pla();
		code.cmp_imm(static_cast<byte>(warn));
		code.label("@cw"); code.bcs(3); code.jmp(V_BLINK); code.jmp(V_NO_DRAW);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x986b) = static_cast<byte>(hide); at(0x98a3) = static_cast<byte>(hide);
		at(0x9893) = static_cast<byte>(up); at(0x98cf) = static_cast<byte>(warn);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (init_hook) { code.jmp(init_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_INIT); }
	if (hide_hook) { code.jmp(hide_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_HIDE); }
	if (up_hook) { code.jmp(up_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_UP); }
	if (warn_hook) { code.jmp(warn_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WARN); }
	return static_cast<word>(cpu_addr + size);
}
