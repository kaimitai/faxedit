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

// ishiisu control: tunes ishiisu. ishiisu walks toward you; when you are
// close and facing it, it winds up, throws and recovers; this hack sets how
// fast it walks, how close you must be, the windup and the recovery, and
// whether it attacks even when you are not facing it. the defaults are the
// stock numbers, so the hack alone changes nothing until a value is set. the
// throw itself and what it throws stay stock, since other monsters share them.
//
// five bank 14 instructions are retargeted, where the walk speed is set, where
// the distance is tested, where the attack length is set, where the throw
// moment is tested and where the throw pose is chosen; face=1 also retargets
// the start of the facing test. the new code goes in bank 15, which is always
// mapped; without a flag, face needs none. a clear flag, or mode=vanilla, is
// the stock routine. every site is checked against its vanilla bytes first,
// and they are identical in the us, us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word SPEED_LO{ 0x0374 }, SPEED_HI{ 0x0375 }, TIMER{ 0x02ec }, FACING{ 0x02dc };
	constexpr word SITE_RANGE{ 0x9147 }, SITE_WALK{ 0x914e }, SITE_FACE{ 0x915e };
	constexpr word SITE_LENGTH{ 0x9170 }, SITE_THROW{ 0x9181 }, SITE_POSE{ 0x91a9 };
	constexpr word V_WALK{ 0x9158 }, V_WALK_STOCK{ 0x9153 }, V_FAR{ 0x914b }, V_NEAR{ 0x915e };
	constexpr word V_LENGTH{ 0x9175 }, V_THROW{ 0x9185 }, V_NO_THROW{ 0x918b };
	constexpr word V_POSE_AFTER{ 0x91ad }, V_POSE_BEFORE{ 0x91af }, V_FACE_SKIP{ 0x916d }, V_FACE_AND{ 0x9161 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 4> RANGE_ORIG{ 0xc9, 0x20, 0x90, 0x13 };
	constexpr std::array<byte, 13> WALK_ORIG{ 0xa9, 0xc0, 0x8d, 0x74, 0x03, 0xa9, 0x00, 0x8d, 0x75, 0x03, 0x20, 0x19, 0x84 };
	constexpr std::array<byte, 6> LENGTH_ORIG{ 0xa9, 0x1e, 0x9d, 0xec, 0x02, 0x60 };
	constexpr std::array<byte, 7> THROW_ORIG{ 0xc9, 0x14, 0xd0, 0x06, 0x4c, 0xa0, 0xa0 };
	constexpr std::array<byte, 6> POSE_ORIG{ 0xc9, 0x14, 0xb0, 0x02, 0xa0, 0x03 };
	constexpr std::array<byte, 5> FACE_ORIG{ 0xbd, 0xdc, 0x02, 0x29, 0x01 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevIshiisuControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevIshiisuControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int walk{ get("walk", 3) }, range{ get("range", 32) }, windup{ get("windup", 10) };
	const int recover{ get("recover", 20) }, face{ get("face", 0) };
	if (walk < 1 || walk > 8)
		throw std::runtime_error(name + ": walk must be 1 to 8");
	if (range < 8 || range > 128)
		throw std::runtime_error(name + ": range must be 8 to 128");
	if (windup < 1 || windup > 64)
		throw std::runtime_error(name + ": windup must be 1 to 64");
	if (recover < 1 || recover > 64)
		throw std::runtime_error(name + ": recover must be 1 to 64");
	if (face != 0 && face != 1)
		throw std::runtime_error(name + ": face must be 0 or 1");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_RANGE, RANGE_ORIG, name);
	require_site(p_rom, SITE_WALK, WALK_ORIG, name);
	require_site(p_rom, SITE_LENGTH, LENGTH_ORIG, name);
	require_site(p_rom, SITE_THROW, THROW_ORIG, name);
	require_site(p_rom, SITE_POSE, POSE_ORIG, name);
	if (face == 1)
		require_site(p_rom, SITE_FACE, FACE_ORIG, name);

	// the walk speed, in quarter pixels
	const int speed{ walk * 64 };
	// with a flag, only the values that differ from stock need a hook; face without a flag is a jump
	// past the facing test, with a flag a hook
	const bool walk_hook{ flag >= 0 && walk != 3 }, range_hook{ flag >= 0 && range != 32 }, length_hook{ flag >= 0 && windup + recover != 30 }, throw_hook{ flag >= 0 && recover != 20 }, pose_hook{ flag >= 0 && recover != 20 }, face_hook{ flag >= 0 && face == 1 };
	const int hooks{ (walk_hook ? 1 : 0) + (range_hook ? 1 : 0) + (length_hook ? 1 : 0) + (throw_hook ? 1 : 0) + (pose_hook ? 1 : 0) + (face_hook ? 1 : 0) };
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
	// a hook that runs with the counter in A keeps it across the test, and both paths share the exit
	auto compare = [&](const std::string& p_name, byte p_value, byte p_stock) {
		code.pha(); flag_test(); code.bne("@t" + p_name);
		code.pla(); code.cmp_imm(p_stock); code.jmp("@c" + p_name);
		code.label("@t" + p_name); code.pla();
		code.cmp_imm(p_value);
		code.label("@c" + p_name);
	};
	word walk_addr{ 0 }, range_addr{ 0 }, length_addr{ 0 }, throw_addr{ 0 }, pose_addr{ 0 }, face_addr{ 0 };
	if (walk_hook) {
		walk_addr = static_cast<word>(cpu_addr + code.size());
		test("@vw");
		code.lda_imm(static_cast<byte>(speed & 0xff)); code.sta_abs(SPEED_LO); code.lda_imm(static_cast<byte>(speed >> 8)); code.sta_abs(SPEED_HI); code.jmp(V_WALK);
		code.label("@vw"); code.lda_imm(0xc0); code.sta_abs(SPEED_LO); code.jmp(V_WALK_STOCK);
	}
	if (range_hook) {
		range_addr = static_cast<word>(cpu_addr + code.size());
		compare("range", static_cast<byte>(range), 0x20); code.bcc(3); code.jmp(V_FAR); code.jmp(V_NEAR);
	}
	if (length_hook) {
		length_addr = static_cast<word>(cpu_addr + code.size());
		select("length", static_cast<byte>(windup + recover), 0x1e); code.sta_abs_x(TIMER); code.jmp(V_LENGTH);
	}
	if (throw_hook) {
		throw_addr = static_cast<word>(cpu_addr + code.size());
		compare("throw", static_cast<byte>(recover), 0x14); code.bne(3); code.jmp(V_THROW); code.jmp(V_NO_THROW);
	}
	if (pose_hook) {
		pose_addr = static_cast<word>(cpu_addr + code.size());
		compare("pose", static_cast<byte>(recover), 0x14); code.bcs(3); code.jmp(V_POSE_AFTER); code.jmp(V_POSE_BEFORE);
	}
	if (face_hook) {
		face_addr = static_cast<word>(cpu_addr + code.size());
		test("@vf"); code.jmp(V_FACE_SKIP);
		code.label("@vf"); code.lda_abs_x(FACING); code.jmp(V_FACE_AND);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x914f) = static_cast<byte>(speed & 0xff); at(0x9154) = static_cast<byte>(speed >> 8);
		at(0x9148) = static_cast<byte>(range); at(0x9171) = static_cast<byte>(windup + recover);
		at(0x9182) = static_cast<byte>(recover); at(0x91aa) = static_cast<byte>(recover);
	}
	// the code first: a site jump is assembled in the same buffer, so it can only be written once the
	// hooks are out of it
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (walk_hook) { code.jmp(walk_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WALK); }
	if (range_hook) { code.jmp(range_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_RANGE); }
	if (length_hook) { code.jmp(length_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_LENGTH); }
	if (throw_hook) { code.jmp(throw_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_THROW); }
	if (pose_hook) { code.jmp(pose_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_POSE); }
	if (face_hook) { code.jmp(face_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_FACE); }
	else if (face == 1) {
		// face without a flag: skip the facing test, no free space needed
		code.jmp(V_FACE_SKIP); code.apply_hack_and_clear(p_rom, BANK14, SITE_FACE);
	}
	return static_cast<word>(cpu_addr + size);
}
