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

// king grieve control: tunes king grieve. king grieve swoops down, hovers and
// fires at you, rises to the top and rests before it swoops again. this hack
// sets how often it fires while it hovers, how long it hovers and how long it
// rests; body=1 makes its box cover its body, which the stock box covers only
// about half of. the defaults are the stock numbers, so the hack alone changes
// nothing until a value is set.
//
// three bank 14 instructions are retargeted, where the shot timing is tested,
// where the time hovering is set and where the time resting is set. the new
// code goes in bank 15, which is always mapped. body=1 rewrites its box, whose
// top stops just short of the top of the screen at its highest point. a clear
// flag, or mode=vanilla, is the stock routine. every site is checked against
// its vanilla bytes first, and they are identical in the us, us rev a, eu and
// jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec }, COUNTER{ 0x0383 };
	constexpr word SITE_SHOTS{ 0x9f65 }, SITE_HOVER{ 0x9f5f }, SITE_REST{ 0x9fbe }, BOX{ 0xb33b };
	constexpr word V_FIRE{ 0x9f6c }, V_SKIP{ 0x9f86 }, V_HOVER{ 0x9f64 }, V_REST{ 0x9fc3 };
	constexpr byte STOCK_MASK{ 0x0f }, STOCK_HOVER{ 0x3c }, STOCK_REST{ 0x1e };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 7> SHOTS_ORIG{ 0xad, 0x83, 0x03, 0x29, 0x0f, 0xd0, 0x1a };
	constexpr std::array<byte, 6> HOVER_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0x60 };
	constexpr std::array<byte, 6> REST_ORIG{ 0xa9, 0x1e, 0x9d, 0xec, 0x02, 0x60 };
	constexpr std::array<byte, 4> BOX_ORIG{ 0x00, 0x00, 0x40, 0x18 };
	// x-8, y-15, 94x55
	constexpr std::array<byte, 4> BOX_FULL{ 0xf8, 0xf1, 0x5e, 0x37 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevKingGrieveControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevKingGrieveControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int shots{ get("shots", 16) }, hover{ get("hover", 60) }, rest{ get("rest", 30) }, body{ get("body", 0) };
	if (shots != 4 && shots != 8 && shots != 16 && shots != 32 && shots != 64 && shots != 128)
		throw std::runtime_error(name + ": shots must be 4, 8, 16, 32, 64 or 128");
	if (hover < 16 || hover > 255)
		throw std::runtime_error(name + ": hover must be 16 to 255");
	if (rest < 16 || rest > 255)
		throw std::runtime_error(name + ": rest must be 16 to 255");
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
	require_site(p_rom, SITE_SHOTS, SHOTS_ORIG, name);
	require_site(p_rom, SITE_HOVER, HOVER_ORIG, name);
	require_site(p_rom, SITE_REST, REST_ORIG, name);
	if (body == 1)
		require_site(p_rom, BOX, BOX_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool shots_hook{ flag >= 0 && shots != 16 }, hover_hook{ flag >= 0 && hover != 60 }, rest_hook{ flag >= 0 && rest != 30 };
	const int hooks{ (shots_hook ? 1 : 0) + (hover_hook ? 1 : 0) + (rest_hook ? 1 : 0) };
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
	// the shot timing: fire when the frame counter is a multiple of shots
	auto shot_test = [&](byte p_mask) {
		code.lda_abs(COUNTER); code.and_imm(p_mask); code.beq(3); code.jmp(V_SKIP); code.jmp(V_FIRE);
	};
	word shots_addr{ 0 }, hover_addr{ 0 }, rest_addr{ 0 };
	if (shots_hook) {
		shots_addr = static_cast<word>(cpu_addr + code.size());
		test("@vs");
		shot_test(static_cast<byte>(shots - 1));
		code.label("@vs"); shot_test(STOCK_MASK);
	}
	if (hover_hook) {
		hover_addr = static_cast<word>(cpu_addr + code.size());
		select("hover", static_cast<byte>(hover), STOCK_HOVER); code.sta_abs_x(TIMER); code.jmp(V_HOVER);
	}
	if (rest_hook) {
		rest_addr = static_cast<word>(cpu_addr + code.size());
		select("rest", static_cast<byte>(rest), STOCK_REST); code.sta_abs_x(TIMER); code.jmp(V_REST);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9f69) = static_cast<byte>(shots - 1); at(0x9f60) = static_cast<byte>(hover); at(0x9fbf) = static_cast<byte>(rest);
	}
	if (body == 1) {
		const auto box{ klib::Asm6502::get_file_offset(BANK14, BOX) };
		for (std::size_t i{ 0 }; i < BOX_FULL.size(); ++i)
			p_rom[box + i] = BOX_FULL[i];
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (shots_hook) { code.jmp(shots_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_SHOTS); }
	if (hover_hook) { code.jmp(hover_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_HOVER); }
	if (rest_hook) { code.jmp(rest_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_REST); }
	return static_cast<word>(cpu_addr + size);
}
