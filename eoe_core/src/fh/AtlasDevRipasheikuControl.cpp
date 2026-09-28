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

// ripasheiku control: tunes ripasheiku's rhythm. it rises while it drifts
// sideways, slams down, then sits and fires at you before rising again; this
// hack sets how fast it drifts, in eighths of a pixel per frame, how many
// frames it sits, and how many frames pass between its shots. the defaults
// are the stock numbers, so the hack alone changes nothing until a value is
// set. the climb and the slam stay stock.
//
// three bank 14 instructions are retargeted, where the shot timing is tested,
// where the sit length is set and where the drift speed is set; the new code
// goes in bank 15, which is always mapped. a clear flag, or mode=vanilla, is
// the stock routine. every site is checked against its vanilla bytes first,
// and they are identical in the us, us rev a, eu and jp roms. no ram is
// claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used (a changed sit length still needs its
// hook); with a flag only the ones whose values differ from stock are
// retargeted. at the stock values nothing is written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word COUNTER{ 0x0383 }, TIMER{ 0x02ec }, DX_FULL{ 0x0375 };
	constexpr word SITE_FIRE{ 0x9b45 }, SITE_SIT{ 0x9c34 }, SITE_DRIFT{ 0x9c43 };
	constexpr word V_FIRE_BNE{ 0x9b4c }, V_FIRE_AND{ 0x9b48 }, V_SIT{ 0x9c39 }, V_DRIFT_STA{ 0x9c4a }, V_DRIFT_LDA{ 0x9c48 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 9> FIRE_ORIG{ 0xad, 0x83, 0x03, 0x29, 0x3f, 0xc9, 0x20, 0xd0, 0x1a };
	constexpr std::array<byte, 11> SIT_ORIG{ 0xa9, 0x00, 0x9d, 0xec, 0x02, 0x9d, 0xf4, 0x02, 0x9d, 0xfc, 0x02 };
	constexpr std::array<byte, 14> DRIFT_ORIG{ 0xa9, 0x01, 0x8d, 0x75, 0x03, 0xa9, 0x00, 0x8d, 0x74, 0x03, 0x20, 0x19, 0x84, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevRipasheikuControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevRipasheikuControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int drift{ get("drift", 8) }, sit{ get("sit", 256) }, fire{ get("fire", 64) };
	if (drift < 1 || drift > 64)
		throw std::runtime_error(name + ": drift must be 1 to 64");
	if (sit < 1 || sit > 256)
		throw std::runtime_error(name + ": sit must be 1 to 256");
	if (fire != 16 && fire != 32 && fire != 64 && fire != 128)
		throw std::runtime_error(name + ": fire must be 16, 32, 64 or 128");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_FIRE, FIRE_ORIG, name);
	require_site(p_rom, SITE_SIT, SIT_ORIG, name);
	require_site(p_rom, SITE_DRIFT, DRIFT_ORIG, name);

	// a value left at stock needs nothing; the sit length is the one value that always needs its hook
	const bool fire_hook{ flag >= 0 && fire != 64 }, sit_hook{ sit != 256 }, drift_hook{ flag >= 0 && drift != 8 };
	const int hooks{ (fire_hook ? 1 : 0) + (sit_hook ? 1 : 0) + (drift_hook ? 1 : 0) };
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
	word fire_addr{ 0 }, sit_addr{ 0 }, drift_addr{ 0 };
	if (fire_hook) {
		fire_addr = static_cast<word>(cpu_addr + code.size());
		test("@vf");
		code.lda_abs(COUNTER); code.and_imm(static_cast<byte>(fire - 1)); code.cmp_imm(static_cast<byte>(fire / 2)); code.jmp(V_FIRE_BNE);
		code.label("@vf"); code.lda_abs(COUNTER); code.jmp(V_FIRE_AND);
	}
	if (sit_hook) {
		sit_addr = static_cast<word>(cpu_addr + code.size());
		// with the flag clear the test leaves A zero, which is the stock LDA #$00, so both paths share the
		// store and the LDA #$00 the code after it reads
		if (flag >= 0) test("@vs");
		code.lda_imm(static_cast<byte>(sit & 0xff));
		if (flag >= 0) code.label("@vs");
		code.sta_abs_x(TIMER); code.lda_imm(0x00); code.jmp(V_SIT);
	}
	if (drift_hook) {
		drift_addr = static_cast<word>(cpu_addr + code.size());
		test("@vd");
		code.lda_imm(static_cast<byte>(drift >> 3)); code.sta_abs(DX_FULL); code.lda_imm(static_cast<byte>((drift & 7) << 5)); code.jmp(V_DRIFT_STA);
		code.label("@vd"); code.lda_imm(0x01); code.sta_abs(DX_FULL); code.jmp(V_DRIFT_LDA);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x9b49) = static_cast<byte>(fire - 1); at(0x9b4b) = static_cast<byte>(fire / 2);
		at(0x9c44) = static_cast<byte>(drift >> 3); at(0x9c49) = static_cast<byte>((drift & 7) << 5);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (fire_hook) { code.jmp(fire_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_FIRE); }
	if (sit_hook) { code.jmp(sit_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_SIT); }
	if (drift_hook) { code.jmp(drift_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_DRIFT); }
	return static_cast<word>(cpu_addr + size);
}
