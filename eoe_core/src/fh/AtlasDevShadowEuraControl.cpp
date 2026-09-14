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

// shadow eura control: tunes shadow eura. it lurches toward you a step at a
// time, throws at you on two of its steps, and stands still between walks.
// this hack sets how many steps it walks, how long it stands, which two steps
// it throws on, and how far each moving step goes; body=1 makes its box cover
// its whole body, which the stock box misses a strip of. the defaults are the
// stock numbers, so the hack alone changes nothing until a value is set. what
// it throws stays stock.
//
// four bank 14 instructions are retargeted, where the first walk is set, where
// the throw steps are tested, where the pause is set and where each later
// walk is set. the new code goes in bank 15, which is always mapped. step
// rewrites six bytes of its step table and body=1 rewrites its box; both are
// fixed when the rom is built. a clear flag, or mode=vanilla, is the stock
// routine. every site is checked against its vanilla bytes first, and they
// are identical in the us, us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec };
	constexpr word SITE_INIT{ 0x9ff8 }, SITE_FIRE{ 0xa029 }, SITE_PAUSE{ 0xa04d }, SITE_WALK{ 0xa05e };
	constexpr word V_INIT{ 0x9ffd }, V_THROW{ 0xa032 }, V_NO_THROW{ 0xa035 }, V_PAUSE{ 0xa052 }, V_WALK{ 0xa063 };
	constexpr word TABLE{ 0xa064 }, BOX{ 0xb33f };
	constexpr byte STOCK_WALK{ 0x14 }, STOCK_PAUSE{ 0x1e }, STOCK_FIRE1{ 3 }, STOCK_FIRE2{ 8 }, STOCK_STEP{ 8 };
	// a throw turned off compares with $ff, which the step (0 to 9) never is
	constexpr byte FIRE_OFF{ 0xff };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 8> INIT_ORIG{ 0xa9, 0x14, 0x9d, 0xec, 0x02, 0x20, 0x7b, 0x86 };
	constexpr std::array<byte, 13> FIRE_ORIG{ 0x48, 0xc9, 0x03, 0xf0, 0x04, 0xc9, 0x08, 0xd0, 0x03, 0x20, 0x77, 0xa0, 0x68 };
	constexpr std::array<byte, 6> PAUSE_ORIG{ 0xa9, 0x1e, 0x9d, 0xec, 0x02, 0x60 };
	constexpr std::array<byte, 6> WALK_ORIG{ 0xa9, 0x14, 0x9d, 0xec, 0x02, 0x60 };
	// the step table: pixels moved on each of the ten steps; step rewrites the six 08s
	constexpr std::array<byte, 10> TABLE_ORIG{ 0x00, 0x00, 0x08, 0x08, 0x08, 0x00, 0x00, 0x08, 0x08, 0x08 };
	// the box: x+8, y+0, 40x80 in the stock game; body=1 is x+0, y-8, 48x88, which covers the whole drawing. it
	// never moves up or down, so the y-8 cannot wrap
	constexpr std::array<byte, 4> BOX_ORIG{ 0x08, 0x00, 0x28, 0x50 };
	constexpr std::array<byte, 4> BOX_FULL{ 0x00, 0xf8, 0x30, 0x58 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevShadowEuraControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevShadowEuraControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int walk{ get("walk", 20) }, pause{ get("pause", 30) }, fire1{ get("fire1", 3) }, fire2{ get("fire2", 8) },
		step{ get("step", 8) }, body{ get("body", 0) };
	if (walk < 1 || walk > 255)
		throw std::runtime_error(name + ": walk must be 1 to 255");
	if (pause < 16 || pause > 255)
		throw std::runtime_error(name + ": pause must be 16 to 255");
	// it counts its steps 0 to 9, so a throw step is one of those; 0 turns that throw off
	if (fire1 < 0 || fire1 > 9)
		throw std::runtime_error(name + ": fire1 must be 0 to 9");
	if (fire2 < 0 || fire2 > 9)
		throw std::runtime_error(name + ": fire2 must be 0 to 9");
	if (step < 1 || step > 16)
		throw std::runtime_error(name + ": step must be 1 to 16");
	if (body != 0 && body != 1)
		throw std::runtime_error(name + ": body must be 0 or 1");
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
	require_site(p_rom, SITE_FIRE, FIRE_ORIG, name);
	require_site(p_rom, SITE_PAUSE, PAUSE_ORIG, name);
	require_site(p_rom, SITE_WALK, WALK_ORIG, name);
	if (step != STOCK_STEP)
		require_site(p_rom, TABLE, TABLE_ORIG, name);
	if (body == 1)
		require_site(p_rom, BOX, BOX_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool init_hook{ flag >= 0 && walk != 20 }, fire_hook{ flag >= 0 && (fire1 != 3 || fire2 != 8) }, pause_hook{ flag >= 0 && pause != 30 }, walk_hook{ flag >= 0 && walk != 20 };
	const int hooks{ (init_hook ? 1 : 0) + (fire_hook ? 1 : 0) + (pause_hook ? 1 : 0) + (walk_hook ? 1 : 0) };
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
	// the throw test: the step is pushed, as the routine pulls it back after the throw, and both paths
	// share the exit
	auto fire_test = [&](byte p_first, byte p_second) {
		code.pha(); code.cmp_imm(p_first); code.beq("@hf"); code.cmp_imm(p_second);
	};
	word init_addr{ 0 }, fire_addr{ 0 }, pause_addr{ 0 }, walk_addr{ 0 };
	if (init_hook) {
		init_addr = static_cast<word>(cpu_addr + code.size());
		select("init", static_cast<byte>(walk), STOCK_WALK); code.sta_abs_x(TIMER); code.jmp(V_INIT);
	}
	if (fire_hook) {
		fire_addr = static_cast<word>(cpu_addr + code.size());
		code.pha(); flag_test(); code.bne("@tf");
		code.pla(); fire_test(STOCK_FIRE1, STOCK_FIRE2); code.jmp("@cf");
		code.label("@tf"); code.pla();
		fire_test(fire1 ? static_cast<byte>(fire1) : FIRE_OFF, fire2 ? static_cast<byte>(fire2) : FIRE_OFF);
		code.label("@cf"); code.bne(3);
		code.label("@hf"); code.jmp(V_THROW); code.jmp(V_NO_THROW);
	}
	if (pause_hook) {
		pause_addr = static_cast<word>(cpu_addr + code.size());
		select("pause", static_cast<byte>(pause), STOCK_PAUSE); code.sta_abs_x(TIMER); code.jmp(V_PAUSE);
	}
	if (walk_hook) {
		walk_addr = static_cast<word>(cpu_addr + code.size());
		select("walk", static_cast<byte>(walk), STOCK_WALK); code.sta_abs_x(TIMER); code.jmp(V_WALK);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9ff9) = static_cast<byte>(walk); at(0xa05f) = static_cast<byte>(walk); at(0xa04e) = static_cast<byte>(pause);
		at(0xa02b) = fire1 ? static_cast<byte>(fire1) : FIRE_OFF; at(0xa02f) = fire2 ? static_cast<byte>(fire2) : FIRE_OFF;
	}
	if (step != STOCK_STEP) {
		const auto table{ klib::Asm6502::get_file_offset(BANK14, TABLE) };
		for (std::size_t i{ 0 }; i < TABLE_ORIG.size(); ++i)
			if (TABLE_ORIG[i] == STOCK_STEP)
				p_rom[table + i] = static_cast<byte>(step);
	}
	if (body == 1) {
		const auto box{ klib::Asm6502::get_file_offset(BANK14, BOX) };
		for (std::size_t i{ 0 }; i < BOX_FULL.size(); ++i)
			p_rom[box + i] = BOX_FULL[i];
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (init_hook) { code.jmp(init_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_INIT); }
	if (fire_hook) { code.jmp(fire_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_FIRE); }
	if (pause_hook) { code.jmp(pause_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_PAUSE); }
	if (walk_hook) { code.jmp(walk_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WALK); }
	return static_cast<word>(cpu_addr + size);
}
