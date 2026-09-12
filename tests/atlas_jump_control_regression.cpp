#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevJumpControl's two runtime switches against each
// other: flag=n emits the same stubs as switchable=1 with every nine byte
// scheduler slot scan replaced by a seven byte extended flag test, and the
// installer refuses the combinations that would mean two switches at once.
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	constexpr std::array<byte, 4> FALL_ORIG{ 0xa5, 0xa5, 0x10, 0x00 };
	constexpr std::array<byte, 5> INIT_ORIG{ 0xa5, 0xa4, 0x4a, 0xb0, 0x1a };
	constexpr std::array<byte, 4> ARC_ORIG{ 0xa6, 0xa6, 0xe0, 0x10 };
	constexpr std::array<byte, 5> SCHED_HOOK1_ORIG{ 0xa9, 0x07, 0x8d, 0x14, 0x40 };
	constexpr std::array<byte, 5> SCHED_HOOK2_ORIG{ 0x8d, 0x01, 0x20, 0xa5, 0x5a };

	// lda $0103 / and #$10 / bne +3 / jmp: the gate for flag 20 at a stub start
	const std::string FLAG20_GATE{ "ad03012910d0034c" };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		auto put = [&](word cpu, const auto& bytes) {
			const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
		};
		put(0xe3cd, FALL_ORIG); put(0xe444, INIT_ORIG); put(0xe463, ARC_ORIG);
		put(0xc9af, SCHED_HOOK1_ORIG); put(0xc9de, SCHED_HOOK2_ORIG);
		// the 32 entry jump arc table must be a mirror
		const auto table{ klib::Asm6502::get_file_offset(15, 0xe4d6) };
		for (std::size_t k{ 0 }; k < 32; ++k)
			rom[table + k] = static_cast<byte>(k < 16 ? k : 31 - k);
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

	byte byte_at(const std::vector<byte>& rom, word cpu) {
		return rom[klib::Asm6502::get_file_offset(15, cpu)];
	}

	// the three gated stubs each open with the flag test
	void test_flag_gate_opens_every_stub() {
		auto rom{ vanilla_rom() };
		const auto used{ install(rom, "AtlasDevJumpControl flag=20 airjumps=1") };
		require(used > 0, "flag install emitted nothing");
		require(hex_at(rom, ORG, 8) == FLAG20_GATE, "fall stub does not open with the flag gate");
		require(byte_at(rom, 0xe3cd) == 0x4c, "fall hook not retargeted");
		require(byte_at(rom, 0xe444) == 0x4c, "init hook not retargeted");
		require(byte_at(rom, 0xe463) == 0x20, "arc hook not retargeted");
		// the init stub is reached through the hook operand and gates the same way
		const word init{ static_cast<word>(byte_at(rom, 0xe445) | (byte_at(rom, 0xe446) << 8)) };
		require(hex_at(rom, init, 6) == "ad03012910d0", "init stub does not open with the flag gate");
		const word hop{ static_cast<word>(byte_at(rom, 0xe464) | (byte_at(rom, 0xe465) << 8)) };
		require(hex_at(rom, hop, 8) == "a6a6ad03012910d0", "hop stub does not gate after loading the phase");
		std::cout << "flag gate opens every stub: ok\n";
	}

	// each of the three gates shrinks from a 21 byte three slot scan to a 7 byte flag test
	void test_flag_body_is_switchable_body_minus_42() {
		auto a{ vanilla_rom() };
		const auto flagged{ install(a, "AtlasDevJumpControl flag=3 airjumps=1") };
		auto b{ vanilla_rom() };
		const auto sched{ fh::HackManager{}.install_general_hacks(fe::Config{}, b, 15, ORG, 0xfff0,
			hacks("AtlasDevFrameScheduler"), nullptr) };
		const auto switched{ fh::HackManager{}.install_general_hacks(fe::Config{}, b, 15,
			static_cast<word>(ORG + sched), 0xfff0, hacks("AtlasDevJumpControl switchable=1 airjumps=1"), nullptr) };
		require(switched == flagged + 42, "flag body is not the switchable body minus three fourteen byte gate savings");
		// flag 3 lives in $0101 bit 3
		require(hex_at(a, ORG, 5) == "ad01012908", "flag 3 address or mask");
		std::cout << "flag body is the switchable body minus 42: ok\n";
	}

	// no flag, no gate: the plain body is shorter still and starts on the coyote counter
	void test_plain_install_has_no_gate() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevJumpControl");
		require(hex_at(rom, ORG, 2) != "ad", "plain install must not start with a gate");
		require(hex_at(rom, ORG, 1) == "e6", "plain install starts with inc of the coyote counter");
		std::cout << "plain install has no gate: ok\n";
	}

	void test_rejections() {
		for (const std::string spec : {
			"AtlasDevJumpControl flag=248",
			"AtlasDevJumpControl flag=1 switchable=1",
			"AtlasDevJumpControl flag=1 armed=0",
			"AtlasDevJumpControl flag=1 armed=1" }) {
			auto rom{ vanilla_rom() };
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, spec); }
			catch (const std::exception& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevJumpControl") != std::string::npos,
					"rejection names the hack: " + spec);
			}
			require(threw, "accepted " + spec);
			require(rom == before, "a refused install modified the rom: " + spec);
		}
		std::cout << "rejections: ok\n";
	}
}

int main() {
	try {
		test_flag_gate_opens_every_stub();
		test_flag_body_is_switchable_body_minus_42();
		test_plain_install_has_no_gate();
		test_rejections();
	}
	catch (const std::exception& e) {
		std::cerr << "FAIL: " << e.what() << '\n';
		return 1;
	}
	return 0;
}
