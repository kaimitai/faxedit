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

// naga control: tunes how naga chases your height. naga faces you and bobs up
// and down; when you are 16 pixels or more above or below it, it also creeps
// toward your height. this hack sets how fast it creeps, in eighths of a pixel
// per frame, and how big a height gap it ignores. the defaults are the stock
// numbers, so the hack alone changes nothing until a value is set. the bob
// stays stock.
//
// two bank 14 instructions are retargeted, where the height gap is checked and
// where the creep speed is set; the new code goes in bank 15, which is always
// mapped. a clear flag, or mode=vanilla, is the stock routine. every site is
// checked against its vanilla bytes first, and they are identical in the us,
// us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word GAP_FN{ 0x831b }, DY_FULL{ 0x0377 };
	constexpr word SITE_ZONE{ 0x9422 }, SITE_CHASE{ 0x9430 };
	constexpr word V_ZONE_BCC{ 0x9427 }, V_ZONE_CMP{ 0x9425 }, V_CHASE_STA{ 0x9437 }, V_CHASE_LDA{ 0x9435 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into, and the gap helper it calls
	constexpr std::array<byte, 7> ZONE_ORIG{ 0x20, 0x1b, 0x83, 0xc9, 0x10, 0x90, 0x18 };
	constexpr std::array<byte, 13> CHASE_ORIG{ 0xa9, 0x00, 0x8d, 0x77, 0x03, 0xa9, 0xc0, 0x8d, 0x76, 0x03, 0x20, 0xc4, 0x85 };
	constexpr std::array<byte, 14> GAP_ORIG{ 0xb5, 0xc2, 0x38, 0xe5, 0xa1, 0xb0, 0x06, 0x49, 0xff, 0x18, 0x69, 0x01, 0x18, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevNagaControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevNagaControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int chase{ get("chase", 6) }, zone{ get("zone", 16) };
	if (chase < 1 || chase > 64)
		throw std::runtime_error(name + ": chase must be 1 to 64");
	if (zone < 0 || zone > 255)
		throw std::runtime_error(name + ": zone must be 0 to 255");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_ZONE, ZONE_ORIG, name);
	require_site(p_rom, SITE_CHASE, CHASE_ORIG, name);
	require_site(p_rom, GAP_FN, GAP_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool zone_hook{ flag >= 0 && zone != 16 }, chase_hook{ flag >= 0 && chase != 6 };
	klib::Asm6502 code;
	// the flag test, inline in each hook: with at most two of them that is smaller than a shared routine
	auto test = [&](const std::string& p_label) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.beq(p_label);
	};
	word zone_addr{ 0 }, chase_addr{ 0 };
	if (zone_hook) {
		zone_addr = static_cast<word>(cpu_addr + code.size());
		test("@vz");
		code.jsr(GAP_FN); code.cmp_imm(static_cast<byte>(zone)); code.jmp(V_ZONE_BCC);
		code.label("@vz"); code.jsr(GAP_FN); code.jmp(V_ZONE_CMP);
	}
	if (chase_hook) {
		chase_addr = static_cast<word>(cpu_addr + code.size());
		test("@vc");
		code.lda_imm(static_cast<byte>(chase >> 3)); code.sta_abs(DY_FULL); code.lda_imm(static_cast<byte>((chase & 7) << 5)); code.jmp(V_CHASE_STA);
		// the test leaves A zero when the flag is clear, which is the stock LDA #$00
		code.label("@vc"); code.sta_abs(DY_FULL); code.jmp(V_CHASE_LDA);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9426) = static_cast<byte>(zone);
		at(0x9431) = static_cast<byte>(chase >> 3); at(0x9436) = static_cast<byte>((chase & 7) << 5);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (zone_hook) { code.jmp(zone_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_ZONE); }
	if (chase_hook) { code.jmp(chase_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_CHASE); }
	return static_cast<word>(cpu_addr + size);
}
