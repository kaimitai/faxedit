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

// necron aides control: tunes the necron aides. a necron aide climbs up and
// down a ladder; when it touches you it lets go, drops and walks the floor;
// this hack sets how fast it climbs, how fast it walks, and whether it keeps
// climbing after it hits you. the defaults are the stock numbers, so the hack
// alone changes nothing until a value is set.
//
// two bank 14 instructions are retargeted, where the climb speed is read and
// where the walk speed is set; cling=1 also retargets the one instruction in
// the enemy touch code that makes a necron aide let go. the new code goes in
// bank 15, which is always mapped; without a flag, cling needs none. a clear
// flag, or mode=vanilla, is the stock routine. every site is checked against
// its vanilla bytes first, and they are identical in the us, us rev a, eu and
// jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used (a slower or faster climb still needs
// its hook); with a flag only the ones whose values differ from stock are
// retargeted. at the stock values nothing is written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word SPEED_LO{ 0x0376 }, SPEED_HI{ 0x0377 }, TABLE_LO{ 0x8e3c }, TABLE_HI{ 0x8e40 };
	constexpr word WALK_CELL{ 0x0375 }, PHASE{ 0x02e4 };
	constexpr word SITE_CLIMB{ 0x8dff }, SITE_WALK{ 0x8e32 }, SITE_CLING{ 0x89bf };
	constexpr word V_CLIMB_TAIL{ 0x85ca }, V_CLIMB_STA{ 0x8e02 }, V_WALK{ 0x8e37 };
	constexpr word V_CLING_SKIP{ 0x89c7 }, V_CLING_ORA{ 0x89c2 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 6> CLIMB_ORIG{ 0xb9, 0x3c, 0x8e, 0x8d, 0x76, 0x03 };
	constexpr std::array<byte, 8> WALK_ORIG{ 0xa9, 0x01, 0x8d, 0x75, 0x03, 0x4c, 0x94, 0x84 };
	constexpr std::array<byte, 8> CLING_ORIG{ 0xbd, 0xe4, 0x02, 0x09, 0x80, 0x9d, 0xe4, 0x02 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevNecronAidesControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevNecronAidesControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int climb{ get("climb", 2) }, walk{ get("walk", 1) }, cling{ get("cling", 0) };
	if (climb != 1 && climb != 2 && climb != 4)
		throw std::runtime_error(name + ": climb must be 1, 2 or 4");
	if (walk < 1 || walk > 4)
		throw std::runtime_error(name + ": walk must be 1 to 4");
	if (cling != 0 && cling != 1)
		throw std::runtime_error(name + ": cling must be 0 or 1");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_CLIMB, CLIMB_ORIG, name);
	require_site(p_rom, SITE_WALK, WALK_ORIG, name);
	if (cling == 1)
		require_site(p_rom, SITE_CLING, CLING_ORIG, name);

	// the climb needs code (the stock speed doubled or halved); the walk speed is only a number; cling
	// without a flag is a jump past the let-go bit, with a flag a hook
	const bool climb_hook{ climb != 2 }, walk_hook{ flag >= 0 && walk != 1 }, cling_hook{ flag >= 0 && cling == 1 };
	const int hooks{ (climb_hook ? 1 : 0) + (walk_hook ? 1 : 0) + (cling_hook ? 1 : 0) };
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
	// the value the store takes: the tuned one with the flag set, the stock one with it clear. the load
	// sets Z, so the branch after it is always taken, and one store serves both paths
	auto select = [&](const std::string& p_name, byte p_value, byte p_stock) {
		test("@v" + p_name);
		code.lda_imm(p_value);
		if (p_value != 0) code.bne("@s" + p_name); else code.beq("@s" + p_name);
		code.label("@v" + p_name); code.lda_imm(p_stock);
		code.label("@s" + p_name);
	};
	word climb_addr{ 0 }, walk_addr{ 0 }, cling_addr{ 0 };
	if (climb_hook) {
		climb_addr = static_cast<word>(cpu_addr + code.size());
		if (flag >= 0) test("@vc");
		code.lda_abs_y(TABLE_LO); code.sta_abs(SPEED_LO); code.lda_abs_y(TABLE_HI);
		if (climb == 4) {
			// asl speed_lo / rol a
			code.db(0x0e); code.db(static_cast<byte>(SPEED_LO & 0xff)); code.db(static_cast<byte>(SPEED_LO >> 8)); code.db(0x2a);
		}
		else if (climb == 1) {
			// lsr a / ror speed_lo
			code.lsr_a(); code.db(0x6e); code.db(static_cast<byte>(SPEED_LO & 0xff)); code.db(static_cast<byte>(SPEED_LO >> 8));
		}
		code.sta_abs(SPEED_HI); code.jmp(V_CLIMB_TAIL);
		if (flag >= 0) { code.label("@vc"); code.lda_abs_y(TABLE_LO); code.jmp(V_CLIMB_STA); }
	}
	if (walk_hook) {
		walk_addr = static_cast<word>(cpu_addr + code.size());
		select("walk", static_cast<byte>(walk), 0x01); code.sta_abs(WALK_CELL); code.jmp(V_WALK);
	}
	if (cling_hook) {
		cling_addr = static_cast<word>(cpu_addr + code.size());
		test("@vg"); code.jmp(V_CLING_SKIP);
		code.label("@vg"); code.lda_abs_x(PHASE); code.jmp(V_CLING_ORA);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x8e33) = static_cast<byte>(walk);
	}
	// the code first: a site jump is assembled in the same buffer, so it can only be written once the
	// hooks are out of it
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (climb_hook) { code.jmp(climb_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_CLIMB); }
	if (walk_hook) { code.jmp(walk_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WALK); }
	if (cling_hook) { code.jmp(cling_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_CLING); }
	else if (cling == 1) {
		// cling without a flag: skip the let-go bit, no free space needed
		code.jmp(V_CLING_SKIP); code.apply_hack_and_clear(p_rom, BANK14, SITE_CLING);
	}
	return static_cast<word>(cpu_addr + size);
}
