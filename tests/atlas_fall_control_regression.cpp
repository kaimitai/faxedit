#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/AtlasDevFrameScheduler.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Pins install_AtlasDevFallControl to the Atlas builder's goldens:
//   python3 tools/build_fall_control.py --emit-hex --profile zelda2 --org 0xfcce
//   python3 tools/build_fall_control.py --emit-hex --profile arc --org 0xfcce
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr std::array<byte, 6> MARK_ORIG{ 0xa5, 0xa4, 0x09, 0x04, 0x85, 0xa4 };
	constexpr std::array<byte, 7> STEP_ORIG{ 0xa5, 0xa1, 0x18, 0x69, 0x08, 0x85, 0xa1 };
	constexpr std::array<byte, 6> STEER_ORIG{ 0xa5, 0xa4, 0x29, 0x05, 0xf0, 0x0f };
	const std::string GOLDEN_ZELDA2{
		"a5a42904d00ca200a5a6c920d002a20786a6a5a4090485a460"
		"a6a6e0079004a20786a6a5a1187d02fd85a1e007b002e6a64cfce3"
		"0102030405060708" };
	const std::string GOLDEN_ARC{
		"a5a42904d00ca200a5a6c920d002a20a86a6a5a4090485a460"
		"a6a6e00a9004a20a86a6a5a1187d02fd85a1e00ab002e6a64cfce3"
		"0101010102020404040408"
		"a5a42905f00da5a43006a5162903d0034c88e14c97e1" };
	// python3 tools/build_fall_control.py --emit-hex --profile arc --kind 6 --org 0xfd64
	// (the scheduler core takes the first 150 bytes from $fcce)
	const std::string GOLDEN_ARC_KIND6{
		"add804c906f00cadd904c906f005adda04c90660"
		"2064fdd012a5a42904d00ca200a5a6c920d002a20a86a6a5a4090485a460"
		"2064fdd01ba6a6e00a9004a20a86a6a5a1187dc0fd85a1e00ab002e6a64cfce3a5a118690885a14cfce3"
		"0101010102020404040408"
		"2064fdd016a5a42905f00da5a43006a5162903d0034c88e14c97e1a5a42905f0f74c88e1" };
	constexpr std::array<byte, 5> SCHED_HOOK1_ORIG{ 0xa9, 0x07, 0x8d, 0x14, 0x40 };
	constexpr std::array<byte, 5> SCHED_HOOK2_ORIG{ 0x8d, 0x01, 0x20, 0xa5, 0x5a };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		auto put = [&](word cpu, const auto& bytes) {
			const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
		};
		put(0xe3d1, MARK_ORIG); put(0xe3f5, STEP_ORIG); put(0xe182, STEER_ORIG);
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

	void test_zelda2_matches_the_python_golden() {
		auto rom{ vanilla_rom() };
		const auto n{ install(rom, "AtlasDevFallControl profile=zelda2") };
		require(n == GOLDEN_ZELDA2.size() / 2, "zelda2 body size " + std::to_string(n));
		require(hex_at(rom, ORG, n) == GOLDEN_ZELDA2,
			"zelda2 body bytes differ from the Python emitter: " + hex_at(rom, ORG, n));
		require(hex_at(rom, 0xe3d1, 6) == "20cefceaeaea", "mark hook: " + hex_at(rom, 0xe3d1, 6));
		require(hex_at(rom, 0xe3f5, 7) == "4ce7fceaeaeaea", "step hook: " + hex_at(rom, 0xe3f5, 7));
		require(hex_at(rom, 0xe182, 6) == "4c97e1eaeaea", "steer=2 hook: " + hex_at(rom, 0xe182, 6));
	}

	void test_arc_matches_the_python_golden_and_steer_body() {
		auto rom{ vanilla_rom() };
		const auto n{ install(rom, "AtlasDevFallControl") };   // the default profile is arc
		require(n == GOLDEN_ARC.size() / 2, "arc body size " + std::to_string(n));
		require(hex_at(rom, ORG, n) == GOLDEN_ARC,
			"arc body bytes differ from the Python emitter: " + hex_at(rom, ORG, n));
		require(hex_at(rom, 0xe182, 6) == "4c0dfdeaeaea", "steer=1 hook: " + hex_at(rom, 0xe182, 6));
	}

	void test_kind_gate_with_the_scheduler() {
		auto rom{ vanilla_rom() };
		const auto n{ install(rom, "AtlasDevFrameScheduler\nAtlasDevFallControl profile=arc kind=6") };
		require(n == fh::afs::CORE_SIZE + GOLDEN_ARC_KIND6.size() / 2, "scheduler + gated body size " + std::to_string(n));
		const word body{ static_cast<word>(ORG + fh::afs::CORE_SIZE) };
		require(hex_at(rom, body, GOLDEN_ARC_KIND6.size() / 2) == GOLDEN_ARC_KIND6,
			"gated arc body differs from the Python emitter: " + hex_at(rom, body, GOLDEN_ARC_KIND6.size() / 2));
		require(hex_at(rom, 0xe3d1, 6) == "2078fdeaeaea", "gated mark hook: " + hex_at(rom, 0xe3d1, 6));
		require(hex_at(rom, 0xe3f5, 7) == "4c96fdeaeaeaea", "gated step hook: " + hex_at(rom, 0xe3f5, 7));
		require(hex_at(rom, 0xe182, 6) == "4ccbfdeaeaea", "gated steer hook: " + hex_at(rom, 0xe182, 6));
		const auto sched{ klib::Asm6502::get_file_offset(15, ORG) };
		require(rom[sched + fh::afs::OFF_ARM0] == 6, "boot slot 0 carries kind 6");
		require(rom[sched + fh::afs::OFF_ARM0 + 1] == 0 && rom[sched + fh::afs::OFF_ARM0 + 2] == 0, "other slots untouched");
		const word pre0{ static_cast<word>(rom[sched + fh::afs::OFF_PRE0] | (rom[sched + fh::afs::OFF_PRE0 + 1] << 8)) };
		require(pre0 == ORG + fh::afs::OFF_STUB, "slot 0 PRE vector still the stub");
	}

	void test_kind_gate_boot_off_and_refusals() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevFrameScheduler\nAtlasDevFallControl profile=zelda2 kind=6 boot=false");
		const auto sched{ klib::Asm6502::get_file_offset(15, ORG) };
		require(rom[sched + fh::afs::OFF_ARM0] == 0, "boot=false leaves the arm table alone");
		require(hex_at(rom, 0xe182, 6) == "4cc8fdeaeaea", "gated steer=2 has a body: " + hex_at(rom, 0xe182, 6));
		auto r{ vanilla_rom() };
		bool threw{ false };
		try { install(r, "AtlasDevFallControl profile=arc kind=6"); } catch (const std::exception&) { threw = true; }
		require(threw, "kind without the scheduler was accepted");
		require(r == vanilla_rom(), "a refused gated install changed the ROM");
		auto full{ vanilla_rom() };
		install(full, "AtlasDevFrameScheduler");
		for (std::size_t i{ 0 }; i < 3; ++i) full[sched + fh::afs::OFF_ARM0 + i] = static_cast<byte>(2 + i);
		threw = false;
		try { install(full, "AtlasDevFallControl profile=arc kind=6"); } catch (const std::exception&) { threw = true; }
		require(threw, "a full arm table was accepted");
		auto reuse{ vanilla_rom() };
		install(reuse, "AtlasDevFrameScheduler");
		reuse[sched + fh::afs::OFF_ARM0] = 2;
		reuse[sched + fh::afs::OFF_ARM0 + 1] = 6;
		install(reuse, "AtlasDevFallControl profile=arc kind=6");
		require(reuse[sched + fh::afs::OFF_ARM0 + 1] == 6 && reuse[sched + fh::afs::OFF_ARM0 + 2] == 0,
			"a slot already holding the kind is reused");
		auto vanilla{ vanilla_rom() };
		install(vanilla, "AtlasDevFrameScheduler");
		const auto before{ vanilla };
		require(install(vanilla, "AtlasDevFallControl profile=vanilla kind=6") == 0, "vanilla profile with kind installs nothing");
		require(vanilla == before, "vanilla profile with kind changed the ROM");
	}

	// every shared name resolves; each equals the explicit curve and steer it
	// stands for; unknown names are refused
	void test_shared_profiles() {
		struct P { const char* name; const char* explicit_; };
		for (const P& p : { P{ "metroid", "curve=1+1+2+2+3+3+4+4+5+5+6 steer=2" }, P{ "megaman", "curve=1+2+3+4+5+6+7+8 steer=2" },
			P{ "castlevania", "profile=vanilla" }, P{ "ninjagaiden", "curve=1+2+3+4+5+6+7+8 steer=2" },
			P{ "ghostsngoblins", "profile=vanilla" }, P{ "kidicarus", "curve=1+1+2+2+3+3+4+4+5+5+6 steer=1" },
			P{ "contra", "curve=1+2+3+4+5+6+7+8 steer=2" }, P{ "arcade", "curve=1+2+3+4+5+6+7+8 steer=2" } }) {
			auto a{ vanilla_rom() }, b{ vanilla_rom() };
			install(a, std::string{ "AtlasDevFallControl profile=" } + p.name);
			install(b, std::string{ "AtlasDevFallControl " } + p.explicit_);
			require(a == b, std::string{ "profile " } + p.name + " differs from its explicit knobs");
		}
		auto rom{ vanilla_rom() };
		bool threw{ false };
		try { install(rom, "AtlasDevFallControl profile=doom"); }
		catch (const std::runtime_error&) { threw = true; }
		require(threw, "accepted an unknown profile");
	}

	void test_vanilla_profile_is_byte_identical() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		require(install(rom, "AtlasDevFallControl profile=vanilla") == 0, "vanilla emits nothing");
		require(rom == before, "vanilla profile changed the ROM");
	}

	void test_overrides_and_refusals() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevFallControl profile=moon steer=0");
		require(hex_at(rom, 0xe182, 6) == "a5a42905f00f", "steer=0 override leaves the gate vanilla");
		for (const char* bad : { "AtlasDevFallControl profile=bogus", "AtlasDevFallControl curve=9",
				"AtlasDevFallControl curve=0+0", "AtlasDevFallControl steer=3",
				"AtlasDevFallControl curve=1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1+1",
				"AtlasDevFallControl curve=1+x" }) {
			auto r{ vanilla_rom() };
			bool threw{ false };
			try { install(r, bad); } catch (const std::exception&) { threw = true; }
			require(threw, std::string("accepted: ") + bad);
		}
		auto r{ vanilla_rom() };
		r[klib::Asm6502::get_file_offset(15, 0xe3f5)] = 0x00;
		bool threw{ false };
		try { install(r, "AtlasDevFallControl"); } catch (const std::exception&) { threw = true; }
		require(threw, "a non-vanilla step site was accepted");
	}
}

int main() {
	try {
		test_shared_profiles();
		test_zelda2_matches_the_python_golden();
		test_arc_matches_the_python_golden_and_steer_body();
		test_vanilla_profile_is_byte_identical();
		test_overrides_and_refusals();
		test_kind_gate_with_the_scheduler();
		test_kind_gate_boot_off_and_refusals();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_fall_control_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_fall_control_regression: ok\n";
	return 0;
}
