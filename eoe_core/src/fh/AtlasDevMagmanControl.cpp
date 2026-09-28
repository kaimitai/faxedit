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

// magman control: tunes magman. magman hides off screen, then appears right in
// front of you at your height, stays there facing you for a while and hides
// again; its only attack is its touch. this hack sets how long it stays
// hidden, how long it stays out and how far in front of you it appears. the
// defaults are the stock numbers, so the hack alone changes nothing until a
// value is set.
//
// four bank 14 instructions are retargeted, where the first time hidden is
// set, where each later time hidden is set, where the time out is set and
// where its position in front of you is worked out. the new code goes in bank
// 15, which is always mapped. a clear flag, or mode=vanilla, is the stock
// routine. every site is checked against its vanilla bytes first, and they are
// identical in the us, us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec };
	constexpr byte FACING{ 0xa4 };
	constexpr word SITE_INIT{ 0x95c5 }, SITE_HIDE{ 0x960d }, SITE_STAY{ 0x95e8 }, SITE_DIST{ 0x95ed };
	constexpr word V_INIT{ 0x95ca }, V_HIDE{ 0x9612 }, V_STAY{ 0x95ed }, V_PLACE{ 0x95f7 };
	constexpr byte STOCK_HIDE{ 0x3c }, STOCK_STAY{ 0x78 }, STOCK_DIST{ 0x30 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 7> INIT_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0xa9, 0x00 };
	constexpr std::array<byte, 7> HIDE_ORIG{ 0xa9, 0x3c, 0x9d, 0xec, 0x02, 0xa9, 0xd0 };
	constexpr std::array<byte, 5> STAY_ORIG{ 0xa9, 0x78, 0x9d, 0xec, 0x02 };
	constexpr std::array<byte, 11> DIST_ORIG{ 0xa0, 0x30, 0xa5, 0xa4, 0x29, 0x40, 0xd0, 0x02, 0xa0, 0xd0, 0x98 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevMagmanControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevMagmanControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int hide{ get("hide", 60) }, stay{ get("stay", 120) }, distance{ get("distance", 48) };
	if (hide < 16 || hide > 255)
		throw std::runtime_error(name + ": hide must be 16 to 255");
	if (stay < 16 || stay > 255)
		throw std::runtime_error(name + ": stay must be 16 to 255");
	if (distance < 8 || distance > 112)
		throw std::runtime_error(name + ": distance must be 8 to 112");
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
	require_site(p_rom, SITE_HIDE, HIDE_ORIG, name);
	require_site(p_rom, SITE_STAY, STAY_ORIG, name);
	require_site(p_rom, SITE_DIST, DIST_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool init_hook{ flag >= 0 && hide != 60 }, hide_hook{ flag >= 0 && hide != 60 }, stay_hook{ flag >= 0 && stay != 120 }, dist_hook{ flag >= 0 && distance != 48 };
	const int hooks{ (init_hook ? 1 : 0) + (hide_hook ? 1 : 0) + (stay_hook ? 1 : 0) + (dist_hook ? 1 : 0) };
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
	// where it appears: distance ahead of you when you face right, behind the other way
	auto place = [&](int p_distance) {
		code.ldy_imm(static_cast<byte>(p_distance)); code.lda_zp(FACING); code.and_imm(0x40); code.bne(2);
		code.ldy_imm(static_cast<byte>((256 - p_distance) & 0xff)); code.jmp(V_PLACE);
	};
	word init_addr{ 0 }, hide_addr{ 0 }, stay_addr{ 0 }, dist_addr{ 0 };
	if (init_hook) {
		init_addr = static_cast<word>(cpu_addr + code.size());
		select("init", static_cast<byte>(hide), STOCK_HIDE); code.sta_abs_x(TIMER); code.jmp(V_INIT);
	}
	if (hide_hook) {
		hide_addr = static_cast<word>(cpu_addr + code.size());
		select("hide", static_cast<byte>(hide), STOCK_HIDE); code.sta_abs_x(TIMER); code.jmp(V_HIDE);
	}
	if (stay_hook) {
		stay_addr = static_cast<word>(cpu_addr + code.size());
		select("stay", static_cast<byte>(stay), STOCK_STAY); code.sta_abs_x(TIMER); code.jmp(V_STAY);
	}
	if (dist_hook) {
		dist_addr = static_cast<word>(cpu_addr + code.size());
		test("@vd");
		place(distance);
		code.label("@vd"); place(STOCK_DIST);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x95c6) = static_cast<byte>(hide); at(0x960e) = static_cast<byte>(hide); at(0x95e9) = static_cast<byte>(stay);
		at(0x95ee) = static_cast<byte>(distance); at(0x95f6) = static_cast<byte>((256 - distance) & 0xff);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (init_hook) { code.jmp(init_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_INIT); }
	if (hide_hook) { code.jmp(hide_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_HIDE); }
	if (stay_hook) { code.jmp(stay_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_STAY); }
	if (dist_hook) { code.jmp(dist_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_DIST); }
	return static_cast<word>(cpu_addr + size);
}
