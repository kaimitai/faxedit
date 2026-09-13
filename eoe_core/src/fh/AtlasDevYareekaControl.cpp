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

// yareeka control: tunes yareeka's dash. it speeds up, dashes sideways at
// 2 pixels per frame for 64 frames, then slows down, over and over; this
// hack sets how fast the dash is, in eighths of a pixel per frame, and how
// many frames it lasts. the defaults are the stock numbers, so the hack alone
// changes nothing until a value is set. the speed-up and slow-down stay
// stock (another routine uses them too), and walls still turn it.
//
// two bank 14 instructions are retargeted: where the dash length is set, at
// $955f, and where the dash fraction is stored, at $956c. a clear flag, or
// mode=vanilla, is the stock routine. every site is checked against its
// vanilla bytes first, and they are identical in the us, us rev a, eu and jp
// roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the knob whose
// value differs from stock is retargeted, and only its hook is written. at
// the stock values nothing is written.
namespace {
	constexpr byte BANK{ 14 };
	constexpr word TIMER{ 0x02ec }, DX_FRAC{ 0x0374 }, DX_FULL{ 0x0375 }, V_AFTER_STA{ 0x956f };
	// site b is the stock store of the dash fraction: the jmp covers it exactly, and the stock lda #$00
	// before it still runs, so a hook entered with the flag clear already holds the stock value
	constexpr word FLAG_BASE{ 0x0101 }, SITE_INIT{ 0x9538 }, MACHINE{ 0x9548 }, SITE_A{ 0x955f }, SITE_B{ 0x956c };
	constexpr word RAMP{ 0x9593 }, MOVE_X{ 0x8419 };
	constexpr int MAX_FLAG{ 247 };

	constexpr std::array<byte, 16> INIT_ORIG{ 0x20, 0x85, 0xa8, 0xd0, 0x0b, 0xa9, 0x00, 0x9d, 0xec, 0x02, 0x9d, 0xe4, 0x02, 0x20, 0x94, 0xa8 };
	constexpr std::array<byte, 75> MACHINE_ORIG{
		0xbc, 0xe4, 0x02, 0xc0, 0x02, 0xf0, 0x31, 0x88, 0xf0, 0x13, 0x20, 0x93, 0x95, 0xbd, 0xec, 0x02,
		0xc9, 0x40, 0x90, 0x08, 0xfe, 0xe4, 0x02, 0xa9, 0x40, 0x9d, 0xec, 0x02, 0x60, 0xa9, 0x02, 0x8d,
		0x75, 0x03, 0xa9, 0x00, 0x8d, 0x74, 0x03, 0x20, 0x19, 0x84, 0xde, 0xec, 0x02, 0xd0, 0x08, 0xfe,
		0xe4, 0x02, 0xa9, 0x40, 0x9d, 0xec, 0x02, 0x60, 0x20, 0x93, 0x95, 0xbd, 0xec, 0x02, 0xc9, 0x80,
		0x90, 0x08, 0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x9d, 0xec, 0x02, 0x60 };
	constexpr std::array<byte, 27> RAMP_ORIG{
		0xbd, 0xec, 0x02, 0xfe, 0xec, 0x02, 0xa0, 0x02, 0x20, 0xe1, 0x83, 0xa0, 0x03, 0x20, 0xc1, 0x83,
		0xad, 0x75, 0x03, 0x29, 0x01, 0x8d, 0x75, 0x03, 0x4c, 0x19, 0x84 };
	constexpr std::array<byte, 14> MOVE_X_ORIG{ 0x20, 0x94, 0x84, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x01, 0x9d, 0xdc, 0x02, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevYareekaControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevYareekaControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int dash{ get("dash", 16) }, dashlen{ get("dashlen", 64) };
	if (dash < 1 || dash > 64)
		throw std::runtime_error(name + ": dash must be 1 to 64");
	if (dashlen < 1 || dashlen > 255)
		throw std::runtime_error(name + ": dashlen must be 1 to 255");
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
	require_site(p_rom, MACHINE, MACHINE_ORIG, name);
	require_site(p_rom, RAMP, RAMP_ORIG, name);
	require_site(p_rom, MOVE_X, MOVE_X_ORIG, name);

	// every value at stock: nothing to install
	if (dash == 16 && dashlen == 64)
		return cpu_addr;
	// without a flag the dash length and speed go straight into the stock instructions, and
	// no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK, p_addr)]; };
		at(0x9560) = static_cast<byte>(dashlen);
		at(0x9566) = static_cast<byte>(dash >> 3);
		at(0x956b) = static_cast<byte>((dash & 7) << 5);
		return cpu_addr;
	}

	// with a flag, only the knob that differs from stock needs a hook
	const bool length_hook{ dashlen != 64 }, speed_hook{ dash != 16 };
	const word flag_byte{ static_cast<word>(FLAG_BASE + (flag >> 3)) };
	const byte flag_mask{ static_cast<byte>(1 << (flag & 7)) };
	klib::Asm6502 code;
	word length_addr{ 0 }, speed_addr{ 0 };
	// the dash length, stored when the speed-up ends
	if (length_hook) {
		length_addr = cpu_addr;
		code.lda_abs(flag_byte); code.and_imm(flag_mask); code.beq("@va");
		code.lda_imm(static_cast<byte>(dashlen));                      // 1 to 255, never zero, so the branch is taken
		code.bne("@sa");
		code.label("@va"); code.lda_imm(0x40);
		code.label("@sa"); code.sta_abs_x(TIMER); code.rts();
	}
	// the dash speed, set on every dash frame. the test leaves a zero when the flag is clear, which is
	// the stock fraction, and the stock whole pixels are already stored, so only a different whole part
	// is written again
	if (speed_hook) {
		speed_addr = static_cast<word>(cpu_addr + code.size());
		code.lda_abs(flag_byte); code.and_imm(flag_mask); code.beq("@sb");
		if ((dash >> 3) != 2) { code.lda_imm(static_cast<byte>(dash >> 3)); code.sta_abs(DX_FULL); }
		code.lda_imm(static_cast<byte>((dash & 7) << 5));
		code.label("@sb"); code.sta_abs(DX_FRAC); code.jmp(V_AFTER_STA);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));
	code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
	if (length_hook) { code.jmp(length_addr); code.apply_hack_and_clear(p_rom, BANK, SITE_A); }
	if (speed_hook) { code.jmp(speed_addr); code.apply_hack_and_clear(p_rom, BANK, SITE_B); }
	return static_cast<word>(cpu_addr + size);
}
