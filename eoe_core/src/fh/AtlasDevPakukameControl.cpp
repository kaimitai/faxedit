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

// pakukame control: tunes how pakukame spawns liliths. pakukame sits still,
// waits, winds up and spawns a lilith, at most three alive at once; this hack
// sets how many frames it waits, how many liliths may be alive, and how many
// frames each windup step takes. the defaults are the stock numbers, so the
// hack alone changes nothing until a value is set.
//
// three bank 14 instructions are retargeted, where the wait is tested, where
// the windup step is tested and where the lilith count is compared; the new
// code goes in bank 15, which is always mapped. a clear flag, or mode=vanilla,
// is the stock routine. every site is checked against its vanilla bytes first,
// and they are identical in the us, us rev a, eu and jp roms. no ram is
// claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec }, COUNTER{ 0x0383 };
	constexpr byte COUNT{ 0x00 };
	constexpr word SITE_DELAY{ 0x9d11 }, SITE_WINDUP{ 0x9d2b }, SITE_CAP{ 0x9d87 };
	constexpr word V_DELAY_BCC{ 0x9d16 }, V_DELAY_CMP{ 0x9d14 }, V_WINDUP_BNE{ 0x9d30 }, V_WINDUP_AND{ 0x9d2e }, V_CAP_BCC{ 0x9d8b };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 7> DELAY_ORIG{ 0xbd, 0xec, 0x02, 0xc9, 0x40, 0x90, 0x12 };
	constexpr std::array<byte, 7> WINDUP_ORIG{ 0xad, 0x83, 0x03, 0x29, 0x07, 0xd0, 0x12 };
	constexpr std::array<byte, 6> CAP_ORIG{ 0xa5, 0x00, 0xc9, 0x03, 0x90, 0x02 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevPakukameControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevPakukameControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int delay{ get("delay", 64) }, cap{ get("cap", 3) }, windup{ get("windup", 8) };
	if (delay < 1 || delay > 255)
		throw std::runtime_error(name + ": delay must be 1 to 255");
	if (cap < 1 || cap > 8)
		throw std::runtime_error(name + ": cap must be 1 to 8");
	if (windup != 2 && windup != 4 && windup != 8 && windup != 16)
		throw std::runtime_error(name + ": windup must be 2, 4, 8 or 16");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_DELAY, DELAY_ORIG, name);
	require_site(p_rom, SITE_WINDUP, WINDUP_ORIG, name);
	require_site(p_rom, SITE_CAP, CAP_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool delay_hook{ flag >= 0 && delay != 64 }, windup_hook{ flag >= 0 && windup != 8 }, cap_hook{ flag >= 0 && cap != 3 };
	const int hooks{ (delay_hook ? 1 : 0) + (windup_hook ? 1 : 0) + (cap_hook ? 1 : 0) };
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
	word delay_addr{ 0 }, windup_addr{ 0 }, cap_addr{ 0 };
	if (delay_hook) {
		delay_addr = static_cast<word>(cpu_addr + code.size());
		test("@vd");
		code.lda_abs_x(TIMER); code.cmp_imm(static_cast<byte>(delay)); code.jmp(V_DELAY_BCC);
		code.label("@vd"); code.lda_abs_x(TIMER); code.jmp(V_DELAY_CMP);
	}
	if (windup_hook) {
		windup_addr = static_cast<word>(cpu_addr + code.size());
		test("@vw");
		code.lda_abs(COUNTER); code.and_imm(static_cast<byte>(windup - 1)); code.jmp(V_WINDUP_BNE);
		code.label("@vw"); code.lda_abs(COUNTER); code.jmp(V_WINDUP_AND);
	}
	if (cap_hook) {
		cap_addr = static_cast<word>(cpu_addr + code.size());
		test("@vc");
		code.lda_zp(COUNT); code.cmp_imm(static_cast<byte>(cap)); code.jmp(V_CAP_BCC);
		code.label("@vc"); code.lda_zp(COUNT); code.cmp_imm(0x03); code.jmp(V_CAP_BCC);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9d15) = static_cast<byte>(delay); at(0x9d2f) = static_cast<byte>(windup - 1); at(0x9d8a) = static_cast<byte>(cap);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (delay_hook) { code.jmp(delay_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_DELAY); }
	if (windup_hook) { code.jmp(windup_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WINDUP); }
	if (cap_hook) { code.jmp(cap_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_CAP); }
	return static_cast<word>(cpu_addr + size);
}
