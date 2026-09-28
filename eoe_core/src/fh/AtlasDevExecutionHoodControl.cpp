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

// execution hood control: tunes execution hood. it walks for a while, stops
// and faces you, throws at you and walks again. this hack sets how fast it
// walks, how long it walks between throws and how long it stands before each
// throw. the defaults are the stock numbers, so the hack alone changes nothing
// until a value is set. what it throws stays stock, since other monsters throw
// it too.
//
// three bank 14 instructions are retargeted, where the walk speed is set,
// where the walk length is tested and where the pause is set. the new code
// goes in bank 15, which is always mapped. a clear flag, or mode=vanilla, is
// the stock routine. every site is checked against its vanilla bytes first,
// and they are identical in the us, us rev a, eu and jp roms. no ram is
// claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word SPEED_LO{ 0x0374 }, SPEED_HI{ 0x0375 }, STEP{ 0x02f4 }, TIMER{ 0x02ec };
	constexpr word SITE_WALK{ 0x91cc }, SITE_LENGTH{ 0x91fb }, SITE_PAUSE{ 0x9205 };
	constexpr word V_MOVE{ 0x91d6 }, V_NEXT_PHASE{ 0x9202 }, V_DONE{ 0x920a };
	constexpr int STOCK_WALK{ 4 }, STOCK_LENGTH{ 48 };
	constexpr byte STOCK_PAUSE{ 0x0f };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 13> WALK_ORIG{ 0xa9, 0x80, 0x8d, 0x74, 0x03, 0xa9, 0x00, 0x8d, 0x75, 0x03, 0x20, 0x19, 0x84 };
	constexpr std::array<byte, 10> LENGTH_ORIG{ 0xbd, 0xf4, 0x02, 0x29, 0x0f, 0xd0, 0x08, 0xfe, 0xe4, 0x02 };
	constexpr std::array<byte, 6> PAUSE_ORIG{ 0xa9, 0x0f, 0x9d, 0xec, 0x02, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevExecutionHoodControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevExecutionHoodControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int walk{ get("walk", STOCK_WALK) }, length{ get("length", STOCK_LENGTH) }, pause{ get("pause", 15) };
	if (walk < 1 || walk > 16)
		throw std::runtime_error(name + ": walk must be 1 to 16");
	// the walk is three phases of length / 3 frames, each ended by a power-of-two test on the step counter
	if (length != 24 && length != 48 && length != 96 && length != 192)
		throw std::runtime_error(name + ": length must be 24, 48, 96 or 192");
	if (pause < 1 || pause > 255)
		throw std::runtime_error(name + ": pause must be 1 to 255");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_WALK, WALK_ORIG, name);
	require_site(p_rom, SITE_LENGTH, LENGTH_ORIG, name);
	require_site(p_rom, SITE_PAUSE, PAUSE_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool walk_hook{ flag >= 0 && walk != STOCK_WALK }, length_hook{ flag >= 0 && length != STOCK_LENGTH }, pause_hook{ flag >= 0 && pause != 15 };
	const int hooks{ (walk_hook ? 1 : 0) + (length_hook ? 1 : 0) + (pause_hook ? 1 : 0) };
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
	// the walk speed, in 1/256 px: walk is in eighths of a pixel
	auto speed = [&](int p_walk) {
		const int sp{ p_walk * 32 };
		code.lda_imm(static_cast<byte>(sp & 0xff)); code.sta_abs(SPEED_LO);
		code.lda_imm(static_cast<byte>(sp >> 8)); code.sta_abs(SPEED_HI); code.jmp(V_MOVE);
	};
	// the walk length: the next phase when the step counter's low bits are zero
	auto phase_test = [&](int p_length) {
		code.lda_abs_x(STEP); code.and_imm(static_cast<byte>(p_length / 3 - 1)); code.beq(3);
		code.jmp(V_DONE); code.jmp(V_NEXT_PHASE);
	};
	word walk_addr{ 0 }, length_addr{ 0 }, pause_addr{ 0 };
	if (walk_hook) {
		walk_addr = static_cast<word>(cpu_addr + code.size());
		test("@vw");
		speed(walk);
		code.label("@vw"); speed(STOCK_WALK);
	}
	if (length_hook) {
		length_addr = static_cast<word>(cpu_addr + code.size());
		test("@vl");
		phase_test(length);
		code.label("@vl"); phase_test(STOCK_LENGTH);
	}
	if (pause_hook) {
		pause_addr = static_cast<word>(cpu_addr + code.size());
		select("pause", static_cast<byte>(pause), STOCK_PAUSE); code.sta_abs_x(TIMER); code.jmp(V_DONE);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0x91cd) = static_cast<byte>((walk * 32) & 0xff); at(0x91d2) = static_cast<byte>((walk * 32) >> 8);
		at(0x91ff) = static_cast<byte>(length / 3 - 1); at(0x9206) = static_cast<byte>(pause);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (walk_hook) { code.jmp(walk_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_WALK); }
	if (length_hook) { code.jmp(length_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_LENGTH); }
	if (pause_hook) { code.jmp(pause_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_PAUSE); }
	return static_cast<word>(cpu_addr + size);
}
