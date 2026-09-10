#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/AtlasDevFrameScheduler.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevRunControl to the Atlas builder's goldens:
//   python3 tools/build_run_control.py --emit-hex --org 0xfdbe
//   python3 tools/build_run_control.py --emit-hex --org 0xfdbe --mode instant --walk-cycle 4
//   python3 tools/build_run_control.py --emit-hex --org 0xfe5a --kind 135   (ORG + the 156 byte scheduler core)
namespace {
	constexpr word ORG{ 0xfdbe };
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr std::array<byte, 3> START_ORIG{ 0x20, 0x7f, 0xe2 };
	constexpr std::array<byte, 7> IDLE_ORIG{ 0xa5, 0xa4, 0x29, 0xdf, 0x85, 0xa4, 0x60 };
	constexpr std::array<byte, 8> CAP_ORIG{ 0xa5, 0xa9, 0xc9, 0x80, 0xa5, 0xaa, 0xe9, 0x01 };
	constexpr std::array<byte, 9> RESET_ORIG{ 0xa9, 0xc0, 0x85, 0xa9, 0xa9, 0x00, 0x85, 0xaa, 0x60 };
	constexpr std::array<byte, 5> SCHED_HOOK1_ORIG{ 0xa9, 0x07, 0x8d, 0x14, 0x40 };
	constexpr std::array<byte, 5> SCHED_HOOK2_ORIG{ 0x8d, 0x01, 0x20, 0xa5, 0x5a };
	const std::string GOLDEN_DEFAULT{
		"a5a42940d008f01ba5a42940d015a5162903f00fadf604293ff008a9808df6044c7fe2a9008df6044c7fe2"
		"a5a42920f007a90c8df604d00aadf604293ff003cef604a5a429df85a460"
		"adf604104da5a42905d014a5162901f008a5a42940d008f027a5a42940d021a5a918691085a9a5aa690085aa"
		"a5a9c980a5aae9029009a98085a9a90285aa3860adf604297f8df604a98085a9a90185aa3860a5a9c980a5aae90160" };
	const std::string GOLDEN_INSTANT_WC4{
		"a5a42940d008f021a5a42940d01ba5162903f015adf604293ff00ea9808df604a98085a9a90285aa60a9008df6044c7fe2"
		"a5a42920f007a90c8df604d00aadf604293ff003cef604a5a429df85a460"
		"adf6041053a5a42905d014a5162901f008a5a42940d008f02da5a42940d027a5a918691085a9a5aa690085aa"
		"a5a9c980a5aae9029009a98085a9a90285aa38e6a3e6a3e6a360adf604297f8df604a98085a9a90185aa3860a5a9c980a5aae90160" };
	const std::string GOLDEN_KIND87{
		"add804c987f00cadd904c987f005adda04c98760a5a42940d008f020a5a42940d01a205afed01aa5162903f00fadf604"
		"293ff008a9808df6044c7fe2a9008df6044c7fe2205afed01aa5a42920f007a90c8df604d012adf604293ff00bcef604"
		"4cc2fea9008df604a5a429df85a460205afed052adf604104da5a42905d014a5162901f008a5a42940d008f027a5a429"
		"40d021a5a918691085a9a5aa690085aaa5a9c980a5aae9029009a98085a9a90285aa3860adf604297f8df604a98085a9"
		"a90185aa3860a5a9c980a5aae90160" };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		auto put = [&](word cpu, const auto& bytes) {
			const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
		};
		put(0xe1d5, START_ORIG); put(0xe226, START_ORIG); put(0xe1be, IDLE_ORIG);
		put(0xe2a3, CAP_ORIG); put(0xe27f, RESET_ORIG);
		put(0xc9af, SCHED_HOOK1_ORIG); put(0xc9de, SCHED_HOOK2_ORIG);
		return rom;
	}

	std::vector<fh::GeneralHack> hacks(const std::string& text) {
		return fh::filter_general_hacks(15, fh::parse_general_hacks(text));
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks(spec), nullptr);
	}

	std::string hex_at(const std::vector<byte>& rom, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) { s += d[rom[off + i] >> 4]; s += d[rom[off + i] & 15]; }
		return s;
	}

	std::string op_to(byte op, word target) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		for (byte b : { op, static_cast<byte>(target & 0xff), static_cast<byte>(target >> 8) }) { s += d[b >> 4]; s += d[b & 15]; }
		return s;
	}

	std::size_t label(const std::string& golden, const std::string& needle_hex) {
		// byte offset of the first occurrence of a stub's opening bytes
		return golden.find(needle_hex) / 2;
	}

	void test_default_matches_the_python_golden() {
		auto rom{ vanilla_rom() };
		const auto n{ install(rom, "AtlasDevRunControl") };
		require(n == GOLDEN_DEFAULT.size() / 2, "default body size " + std::to_string(n));
		require(hex_at(rom, ORG, n) == GOLDEN_DEFAULT, "default body differs from the Python emitter: " + hex_at(rom, ORG, n));
		require(hex_at(rom, 0xe1d5, 3) == op_to(0x20, ORG), "right start hook: " + hex_at(rom, 0xe1d5, 3));
		require(hex_at(rom, 0xe226, 3) == op_to(0x20, ORG + 8), "left start hook: " + hex_at(rom, 0xe226, 3));
		const word idle{ static_cast<word>(ORG + label(GOLDEN_DEFAULT, "a5a42920f0")) };
		require(hex_at(rom, 0xe1be, 7) == op_to(0x4c, idle) + "eaeaeaea", "idle hook: " + hex_at(rom, 0xe1be, 7));
		const word cap{ static_cast<word>(ORG + label(GOLDEN_DEFAULT, "adf60410")) };
		require(hex_at(rom, 0xe2a3, 8) == op_to(0x20, cap) + "eaeaeaeaea", "cap hook: " + hex_at(rom, 0xe2a3, 8));
		require(hex_at(rom, 0xe27f, 9) == "a9c085a9a90085aa60", "the speed setter is untouched");
	}

	void test_instant_walk_cycle_matches_the_python_golden() {
		auto rom{ vanilla_rom() };
		const auto n{ install(rom, "AtlasDevRunControl mode=instant walk_cycle=4") };
		require(n == GOLDEN_INSTANT_WC4.size() / 2, "instant body size " + std::to_string(n));
		require(hex_at(rom, ORG, n) == GOLDEN_INSTANT_WC4, "instant body differs: " + hex_at(rom, ORG, n));
	}

	void test_kind_gate_with_the_scheduler() {
		auto rom{ vanilla_rom() };
		const auto n{ install(rom, "AtlasDevFrameScheduler\nAtlasDevRunControl kind=135") };
		require(n == fh::afs::CORE_SIZE + GOLDEN_KIND87.size() / 2, "scheduler + gated size " + std::to_string(n));
		const word body{ static_cast<word>(ORG + fh::afs::CORE_SIZE) };
		require(hex_at(rom, body, GOLDEN_KIND87.size() / 2) == GOLDEN_KIND87, "gated body differs: " + hex_at(rom, body, GOLDEN_KIND87.size() / 2));
		const auto sched{ klib::Asm6502::get_file_offset(15, ORG) };
		require(rom[sched + fh::afs::OFF_ARM0] == 0x87, "boot slot 0 carries kind $87");
		require(rom[sched + fh::afs::OFF_ARM0 + 1] == 0 && rom[sched + fh::afs::OFF_ARM0 + 2] == 0, "other slots untouched");
		auto off{ vanilla_rom() };
		install(off, "AtlasDevFrameScheduler\nAtlasDevRunControl kind=135 boot=false");
		require(off[klib::Asm6502::get_file_offset(15, ORG) + fh::afs::OFF_ARM0] == 0, "boot=false leaves the arm table alone");
	}

	void test_refusals_leave_the_rom_untouched() {
		const auto pristine{ vanilla_rom() };
		for (const char* spec : { "AtlasDevRunControl speed=12", "AtlasDevRunControl speed=65",
			"AtlasDevRunControl accel=256", "AtlasDevRunControl window=0", "AtlasDevRunControl window=64",
			"AtlasDevRunControl mode=dash", "AtlasDevRunControl walk_cycle=5", "AtlasDevRunControl kind=135" }) {
			auto rom{ vanilla_rom() };
			bool threw{ false };
			try { install(rom, spec); } catch (const std::exception&) { threw = true; }
			require(threw, std::string("accepted: ") + spec);
			require(rom == pristine, std::string("a refused install changed the ROM: ") + spec);
		}
		auto taken{ vanilla_rom() };
		taken[klib::Asm6502::get_file_offset(15, 0xe1be)] = 0x4c;
		bool threw{ false };
		try { install(taken, "AtlasDevRunControl"); } catch (const std::exception&) { threw = true; }
		require(threw, "a patched idle site was accepted");
	}

	void test_coexists_with_the_movement_hacks_in_both_orders() {
		for (const char* spec : {
			"AtlasDevFrameScheduler\nAtlasDevJumpControl\nAtlasDevFallControl\nAtlasDevRunControl",
			"AtlasDevFrameScheduler\nAtlasDevRunControl\nAtlasDevJumpControl\nAtlasDevFallControl" }) {
			auto rom{ vanilla_rom() };
			auto put = [&](word cpu, std::initializer_list<byte> b) {
				const auto off{ klib::Asm6502::get_file_offset(15, cpu) }; std::size_t i{ 0 };
				for (byte x : b) rom[off + i++] = x;
			};
			put(0xe3cd, { 0xa5, 0xa5, 0x10, 0x00 }); put(0xe444, { 0xa5, 0xa4, 0x4a, 0xb0, 0x1a });
			put(0xe463, { 0xa6, 0xa6, 0xe0, 0x10 }); put(0xe3d1, { 0xa5, 0xa4, 0x09, 0x04, 0x85, 0xa4 });
			put(0xe3f5, { 0xa5, 0xa1, 0x18, 0x69, 0x08, 0x85, 0xa1 }); put(0xe182, { 0xa5, 0xa4, 0x29, 0x05, 0xf0, 0x0f });
			const auto table{ klib::Asm6502::get_file_offset(15, 0xe4d6) };
			for (std::size_t k{ 0 }; k < 32; ++k) rom[table + k] = static_cast<byte>(k < 16 ? k : 31 - k);
			const auto n{ install(rom, spec) };
			require(n > fh::afs::CORE_SIZE + GOLDEN_DEFAULT.size() / 2, "combined install too small");
			require(hex_at(rom, 0xe2a3, 1) == "20" && hex_at(rom, 0xe1be, 1) == "4c", "run hooks present in the combined build");
		}
	}
}

int main() {
	try {
		test_default_matches_the_python_golden();
		test_instant_walk_cycle_matches_the_python_golden();
		test_kind_gate_with_the_scheduler();
		test_refusals_leave_the_rom_untouched();
		test_coexists_with_the_movement_hacks_in_both_orders();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_run_control_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_run_control_regression: 5 checks passed\n";
	return 0;
}
