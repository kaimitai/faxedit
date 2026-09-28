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

// giant bees control: tunes the giant bees. a giant bee climbs to the top of
// the screen, dives at you, hovers for a while and climbs again; this hack
// sets how fast it climbs, how fast it moves sideways while it dives and how
// many frames it hovers. the defaults are the stock numbers, so the hack
// alone changes nothing until a value is set. the dive's fall, the bob and
// the hitbox stay stock.
//
// three bank 14 instructions are retargeted, where the climb speed is set,
// where the dive's sideways speed is set and where the hover's end is tested;
// the new code goes in bank 15, which is always mapped. a clear flag, or
// mode=vanilla, is the stock routine. every site is checked against its
// vanilla bytes first, and they are identical in the us, us rev a, eu and jp
// roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word DY_FULL{ 0x0377 }, DX_FULL{ 0x0375 }, COUNTER{ 0x02f4 };
	constexpr word SITE_RISE{ 0x92fd }, SITE_DIVE{ 0x933c }, SITE_HOVER{ 0x9397 };
	constexpr word V_RISE{ 0x9302 }, V_DIVE{ 0x9341 }, V_HOVER{ 0x939c }, V_HOVER_AND{ 0x939a };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 8> RISE_ORIG{ 0xa9, 0x04, 0x8d, 0x77, 0x03, 0x20, 0x9c, 0x85 };
	constexpr std::array<byte, 8> DIVE_ORIG{ 0xa9, 0x01, 0x8d, 0x75, 0x03, 0x20, 0x27, 0x84 };
	constexpr std::array<byte, 7> HOVER_ORIG{ 0xbd, 0xf4, 0x02, 0x29, 0x7f, 0xd0, 0x0d };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevGiantBeesControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevGiantBeesControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int rise{ get("rise", 4) }, dive{ get("dive", 1) }, hover{ get("hover", 128) };
	if (rise < 1 || rise > 8)
		throw std::runtime_error(name + ": rise must be 1 to 8");
	if (dive < 1 || dive > 4)
		throw std::runtime_error(name + ": dive must be 1 to 4");
	if (hover != 32 && hover != 64 && hover != 128 && hover != 256)
		throw std::runtime_error(name + ": hover must be 32, 64, 128 or 256");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_RISE, RISE_ORIG, name);
	require_site(p_rom, SITE_DIVE, DIVE_ORIG, name);
	require_site(p_rom, SITE_HOVER, HOVER_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool rise_hook{ flag >= 0 && rise != 4 }, dive_hook{ flag >= 0 && dive != 1 }, hover_hook{ flag >= 0 && hover != 128 };
	const int hooks{ (rise_hook ? 1 : 0) + (dive_hook ? 1 : 0) + (hover_hook ? 1 : 0) };
	klib::Asm6502 code;
	// the flag test: a shared routine once three hooks pay for the calls, else inline in each hook
	const bool shared{ hooks >= 3 };
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
	word rise_addr{ 0 }, dive_addr{ 0 }, hover_addr{ 0 };
	if (rise_hook) {
		rise_addr = static_cast<word>(cpu_addr + code.size());
		select("rise", static_cast<byte>(rise), 0x04); code.sta_abs(DY_FULL); code.jmp(V_RISE);
	}
	if (dive_hook) {
		dive_addr = static_cast<word>(cpu_addr + code.size());
		select("dive", static_cast<byte>(dive), 0x01); code.sta_abs(DX_FULL); code.jmp(V_DIVE);
	}
	if (hover_hook) {
		hover_addr = static_cast<word>(cpu_addr + code.size());
		test("@vh");
		code.lda_abs_x(COUNTER); code.and_imm(static_cast<byte>(hover - 1)); code.jmp(V_HOVER);
		code.label("@vh"); code.lda_abs_x(COUNTER); code.jmp(V_HOVER_AND);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x92fe) = static_cast<byte>(rise); at(0x933d) = static_cast<byte>(dive); at(0x939b) = static_cast<byte>(hover - 1);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (rise_hook) { code.jmp(rise_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_RISE); }
	if (dive_hook) { code.jmp(dive_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_DIVE); }
	if (hover_hook) { code.jmp(hover_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_HOVER); }
	return static_cast<word>(cpu_addr + size);
}
