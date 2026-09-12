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

// pins install_AtlasDevLadderControl to the bytes it is expected to emit for
// a given set of parameters. the climb speeds and the attack gate are operand
// bytes the vanilla rom already holds, so a default install writes nothing at
// all, which the first case asserts directly.
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	// the enclosing instruction of every operand this hack may rewrite
	constexpr std::array<byte, 7> UP_LO_ORIG{ 0xa5, 0xa0, 0x38, 0xe9, 0xa0, 0x85, 0xa0 };
	constexpr std::array<byte, 6> UP_HI_ORIG{ 0xa5, 0xa1, 0xe9, 0x00, 0x85, 0xa1 };
	constexpr std::array<byte, 7> DOWN_LO_ORIG{ 0xa5, 0xa0, 0x18, 0x69, 0xc0, 0x85, 0xa0 };
	constexpr std::array<byte, 6> DOWN_HI_ORIG{ 0xa5, 0xa1, 0x69, 0x00, 0x85, 0xa1 };
	constexpr std::array<byte, 7> WING_UP_ORIG{ 0xa5, 0xa1, 0x38, 0xe9, 0x01, 0x85, 0xa1 };
	constexpr std::array<byte, 7> WING_DOWN_ORIG{ 0xa5, 0xa0, 0x18, 0x69, 0x80, 0x85, 0xa0 };
	constexpr std::array<byte, 6> WING_DOWN_HI_ORIG{ 0xa5, 0xa1, 0x69, 0x01, 0x85, 0xa1 };
	constexpr std::array<byte, 5> ATTACK_ORIG{ 0x20, 0xf6, 0xec, 0xb0, 0x0d };
	constexpr std::array<byte, 3> JMP_AFTER_WING_UP{ 0x4c, 0x2b, 0xe3 };
	constexpr std::array<byte, 2> BCS_AFTER_UP{ 0xb0, 0x16 };
	constexpr std::array<byte, 3> JMP_AFTER_WING_DOWN{ 0x4c, 0x79, 0xe3 };
	constexpr std::array<byte, 2> CMP_AFTER_DOWN{ 0xc9, 0xc1 };
	constexpr std::array<byte, 12> POSE_WEAPON_ORIG{
		0xa5, 0xa4, 0x4a, 0x90, 0x07, 0xa5, 0xa4, 0x30, 0x28, 0xa9, 0x03, 0x60 };
	constexpr std::array<byte, 12> POSE_BODY_ORIG{
		0xa5, 0xa4, 0x4a, 0x90, 0x07, 0xa5, 0xa4, 0x30, 0x17, 0xa9, 0x03, 0x60 };

	// both selectors reordered to test the attack bit before the split
	const std::string POSE_WEAPON_FIXED{ "a5a4302d4a9005a90360eaea" };
	const std::string POSE_BODY_FIXED{ "a5a4301c4a9005a90360eaea" };
	// jsr $ecf6 / bcc out / lda $0101 / and #$10 / beq out / clc / out: rts
	const std::string RUNTIME_STUB{ "20f6ec9008ad01012910f0011860" };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		auto put = [&](byte bank, word cpu, const auto& bytes) {
			const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
		};
		put(15, 0xe31e, UP_LO_ORIG); put(15, 0xe325, UP_HI_ORIG);
		put(15, 0xe36c, DOWN_LO_ORIG); put(15, 0xe373, DOWN_HI_ORIG);
		put(15, 0xe314, WING_UP_ORIG);
		put(15, 0xe35c, WING_DOWN_ORIG); put(15, 0xe363, WING_DOWN_HI_ORIG);
		put(15, 0xe107, ATTACK_ORIG);
		// the instructions after each speed block, which read carry or A
		put(15, 0xe31b, JMP_AFTER_WING_UP); put(15, 0xe32b, BCS_AFTER_UP);
		put(15, 0xe369, JMP_AFTER_WING_DOWN); put(15, 0xe379, CMP_AFTER_DOWN);
		put(15, 0xecac, POSE_BODY_ORIG);
		put(14, 0xb927, POSE_WEAPON_ORIG);
		return rom;
	}

	std::vector<fh::GeneralHack> hacks(const std::string& text) {
		return fh::filter_general_hacks(15, fh::parse_general_hacks(text));
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks(spec), nullptr);
	}

	std::string hex_at(const std::vector<byte>& rom, byte bank, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) {
			s += d[rom[off + i] >> 4];
			s += d[rom[off + i] & 0x0f];
		}
		return s;
	}

	byte byte_at(const std::vector<byte>& rom, byte bank, word cpu) {
		return rom[klib::Asm6502::get_file_offset(bank, cpu)];
	}

	// a default install must not touch a single byte
	void test_defaults_write_nothing() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		const auto used{ install(rom, "AtlasDevLadderControl") };
		require(used == 0, "default install consumed cursor space");
		require(rom == before, "default install modified the rom");
	}

	void test_speeds_are_operand_bytes() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevLadderControl up=384 down=448 wingup=2 wingdown=512");
		require(byte_at(rom, 15, 0xe322) == 0x80, "climb up low operand");
		require(byte_at(rom, 15, 0xe328) == 0x01, "climb up high operand");
		require(byte_at(rom, 15, 0xe370) == 0xc0, "climb down low operand");
		require(byte_at(rom, 15, 0xe376) == 0x01, "climb down high operand");
		require(byte_at(rom, 15, 0xe318) == 0x02, "wing boots up operand");
		require(byte_at(rom, 15, 0xe360) == 0x00, "wing boots down low operand");
		require(byte_at(rom, 15, 0xe366) == 0x02, "wing boots down high operand");
		// the surrounding instructions are untouched
		require(byte_at(rom, 15, 0xe321) == 0xe9, "the sbc opcode moved");
		require(byte_at(rom, 15, 0xe36f) == 0x69, "the adc opcode moved");
	}

	void test_attack_is_one_branch_byte() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevLadderControl attack=1");
		require(byte_at(rom, 15, 0xe10b) == 0x00, "the refusing branch was not neutralized");
		require(hex_at(rom, 15, 0xe107, 3) == "20f6ec", "the predicate call was disturbed");
	}

	// patching one selector and not the other is what draws a third arm
	void test_pose_moves_both_selectors() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevLadderControl attack=1 attackpose=1");
		require(hex_at(rom, 14, 0xb927, 12) == POSE_WEAPON_FIXED, "weapon selector body");
		require(hex_at(rom, 15, 0xecac, 12) == POSE_BODY_FIXED, "body selector body");
		// every following address must stay put
		require(byte_at(rom, 14, 0xb933) == 0xff, "the weapon selector overran");
		require(byte_at(rom, 15, 0xecb8) == 0xff, "the body selector overran");
	}

	void test_runtime_flag_redirects_only_the_call() {
		auto rom{ vanilla_rom() };
		const auto used{ install(rom, "AtlasDevLadderControl attackflag=4") };
		require(used == 14, "the runtime stub is not 14 bytes");
		require(hex_at(rom, 15, ORG, 14) == RUNTIME_STUB, "runtime stub body");
		require(hex_at(rom, 15, 0xe107, 3) == "20cefc", "the call was not retargeted");
		// the vanilla refusal is left exactly as it was
		require(byte_at(rom, 15, 0xe10b) == 0x0d, "the refusing branch was altered");
	}

	void test_flag_arithmetic() {
		for (const auto& c : std::vector<std::tuple<std::string, std::string>>{
			{ "attackflag=0", "20f6ec9008ad01012901f0011860" },
			{ "attackflag=7", "20f6ec9008ad01012980f0011860" },
			{ "attackflag=8", "20f6ec9008ad02012901f0011860" },
			{ "attackflag=247", "20f6ec9008ad1f012980f0011860" } }) {
			auto rom{ vanilla_rom() };
			install(rom, "AtlasDevLadderControl " + std::get<0>(c));
			require(hex_at(rom, 15, ORG, 14) == std::get<1>(c),
				"wrong flag address or mask for " + std::get<0>(c));
		}
	}

	// a profile sets up, down, attack and attackpose; games with no ladders
	// are refused by name; an explicit knob overrides the profile
	void test_profiles() {
		struct P { const char* name; byte up_lo, up_hi, down_lo, down_hi, attack, pose; };
		for (const P& p : { P{ "zelda2", 0xe0, 0x00, 0xe0, 0x00, 1, 1 }, P{ "megaman", 0xff, 0x00, 0xff, 0x00, 0, 0 },
			P{ "castlevania", 0xa0, 0x00, 0xa0, 0x00, 0, 0 }, P{ "ninjagaiden", 0xff, 0x00, 0xff, 0x00, 1, 1 },
			P{ "ghostsngoblins", 0xa0, 0x00, 0xc0, 0x00, 0, 0 }, P{ "kidicarus", 0xe0, 0x00, 0xe0, 0x00, 1, 1 },
			P{ "arcade", 0xff, 0x00, 0xff, 0x00, 1, 1 } }) {
			auto rom{ vanilla_rom() };
			install(rom, std::string{ "AtlasDevLadderControl profile=" } + p.name);
			const std::string tag{ std::string{ "profile " } + p.name };
			require(byte_at(rom, 15, 0xe322) == p.up_lo && byte_at(rom, 15, 0xe328) == p.up_hi, tag + " up");
			require(byte_at(rom, 15, 0xe370) == p.down_lo && byte_at(rom, 15, 0xe376) == p.down_hi, tag + " down");
			require(byte_at(rom, 15, 0xe10b) == (p.attack ? 0x00 : 0x0d), tag + " attack");
			require((hex_at(rom, 14, 0xb927, 12) == POSE_WEAPON_FIXED) == (p.pose == 1), tag + " attackpose");
		}
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevLadderControl profile=vanilla");
		require(rom == before, "profile vanilla wrote bytes");
		auto over{ vanilla_rom() };
		install(over, "AtlasDevLadderControl profile=megaman up=384");
		require(byte_at(over, 15, 0xe322) == 0x80 && byte_at(over, 15, 0xe328) == 0x01, "explicit up did not override");
		require(byte_at(over, 15, 0xe370) == 0xff, "profile down lost under an override");
		for (const auto& spec : { "AtlasDevLadderControl profile=metroid", "AtlasDevLadderControl profile=contra",
			"AtlasDevLadderControl profile=doom" }) {
			auto r{ vanilla_rom() };
			const auto b{ r };
			bool threw{ false };
			try { install(r, spec); }
			catch (const std::runtime_error& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevLadderControl") != std::string::npos, "refusal does not name the hack");
			}
			require(threw && r == b, std::string{ "accepted " } + spec);
		}
	}

	void test_refusals() {
		for (const auto& spec : { "AtlasDevLadderControl up=0",
			"AtlasDevLadderControl up=2049", "AtlasDevLadderControl down=0",
			"AtlasDevLadderControl wingup=0", "AtlasDevLadderControl wingup=9",
			"AtlasDevLadderControl wingdown=0", "AtlasDevLadderControl attack=2",
			"AtlasDevLadderControl attackpose=2", "AtlasDevLadderControl attackflag=248",
			"AtlasDevLadderControl attack=1 attackflag=4" }) {
			auto rom{ vanilla_rom() };
			bool threw{ false };
			try { install(rom, spec); }
			catch (const std::runtime_error&) { threw = true; }
			require(threw, std::string{ "accepted " } + spec);
		}
	}

	// a rom whose ladder code has already been altered is refused, naming the site
	void test_refuses_a_disturbed_site() {
		for (const auto& c : std::vector<std::tuple<byte, word, std::string>>{
			{ 15, 0xe31e, "AtlasDevLadderControl up=384" },
			{ 15, 0xe107, "AtlasDevLadderControl attack=1" },
			{ 14, 0xb927, "AtlasDevLadderControl attackpose=1" },
			{ 15, 0xecac, "AtlasDevLadderControl attackpose=1" } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(std::get<0>(c), std::get<1>(c))] = 0x12;
			bool threw{ false };
			try { install(rom, std::get<2>(c)); }
			catch (const std::runtime_error& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevLadderControl") != std::string::npos,
					"the refusal does not name the hack");
			}
			require(threw, "a disturbed site was accepted");
		}
	}
}

	// flag=n turns each changed speed block into a call: the stub tests the
	// flag, then runs either the new constants or the displaced vanilla bytes
	void test_flag_gates_the_speeds() {
		// up=384 (subtract $0180) gated on flag 4: $0101 bit 4
		const std::string UP_STUB{ "ad01012910f00e" "a5a038e98085a0a5a1e90185a160" "a5a038e9a085a0a5a1e90085a160" };
		auto rom{ vanilla_rom() };
		const auto used{ install(rom, "AtlasDevLadderControl up=384 flag=4") };
		require(used == 35, "one 16 bit speed stub is not 35 bytes: " + std::to_string(used));
		require(hex_at(rom, 15, ORG, 35) == UP_STUB, "up stub body: " + hex_at(rom, 15, ORG, 35));
		require(hex_at(rom, 15, 0xe31e, 13) == "20cefceaeaeaeaeaeaeaeaeaea", "up block is a call plus nops: " + hex_at(rom, 15, 0xe31e, 13));
		require(hex_at(rom, 15, 0xe32b, 2) == "b016", "the bcs after the up block moved");
		require(hex_at(rom, 15, 0xe36c, 13) == "a5a01869c085a0a5a1690085a1", "an unchanged block was touched");
		require(hex_at(rom, 15, 0xe314, 7) == "a5a138e90185a1", "the wing up block was touched");
		require(hex_at(rom, 15, 0xe35c, 13) == "a5a018698085a0a5a1690185a1", "the wing down block was touched");

		// the 8 bit wing boots ascent: 23 bytes, a 7 byte call site
		const std::string WING_STUB{ "ad01012910f008" "a5a138e90285a160" "a5a138e90185a160" };
		auto wing{ vanilla_rom() };
		require(install(wing, "AtlasDevLadderControl wingup=2 flag=4") == 23, "the wing up stub is not 23 bytes");
		require(hex_at(wing, 15, ORG, 23) == WING_STUB, "wing up stub body: " + hex_at(wing, 15, ORG, 23));
		require(hex_at(wing, 15, 0xe314, 7) == "20cefceaeaeaea", "wing up block: " + hex_at(wing, 15, 0xe314, 7));
		require(hex_at(wing, 15, 0xe31b, 3) == "4c2be3", "the jmp after the wing up block moved");

		// all four blocks, in emission order up, down, wing up, wing down
		auto all{ vanilla_rom() };
		require(install(all, "AtlasDevLadderControl up=384 down=448 wingup=2 wingdown=512 flag=4") == 35 + 35 + 23 + 35,
			"four stubs size");
		require(hex_at(all, 15, 0xe31e, 3) == "20cefc", "up call");
		require(hex_at(all, 15, 0xe36c, 3) == "20f1fc", "down call at stub 35");
		require(hex_at(all, 15, 0xe314, 3) == "2014fd", "wing up call at stub 70");
		require(hex_at(all, 15, 0xe35c, 3) == "202bfd", "wing down call at stub 93");
		require(hex_at(all, 15, 0xe369, 3) == "4c79e3", "the jmp after the wing down block moved");
		require(hex_at(all, 15, 0xe379, 2) == "c9c1", "the cmp after the down block moved");

		// with attackflag as well, the runtime stub comes first and the speeds follow it
		auto both{ vanilla_rom() };
		require(install(both, "AtlasDevLadderControl up=384 attackflag=4 flag=4") == 14 + 35, "runtime plus speed stub size");
		require(hex_at(both, 15, ORG, 14) == RUNTIME_STUB, "runtime stub first");
		require(hex_at(both, 15, 0xe31e, 3) == "20dcfc", "up call past the runtime stub");
		require(hex_at(both, 15, 0xe107, 3) == "20cefc", "the attack call was not retargeted");

		for (const std::string spec : {
			"AtlasDevLadderControl up=384 flag=248",
			"AtlasDevLadderControl flag=4",
			"AtlasDevLadderControl attackflag=4 flag=4" }) {
			auto r{ vanilla_rom() };
			const auto before{ r };
			bool threw{ false };
			try { install(r, spec); }
			catch (const std::exception& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevLadderControl") != std::string::npos, "rejection names the hack: " + spec);
			}
			require(threw, "accepted " + spec);
			require(r == before, "a refused flag install changed the ROM: " + spec);
		}
		std::cout << "flag gates the speeds: ok\n";
	}

int main() {
	try {
		test_flag_gates_the_speeds();
		test_defaults_write_nothing();
		test_speeds_are_operand_bytes();
		test_attack_is_one_branch_byte();
		test_pose_moves_both_selectors();
		test_runtime_flag_redirects_only_the_call();
		test_flag_arithmetic();
		test_profiles();
		test_refusals();
		test_refuses_a_disturbed_site();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_ladder_control_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_ladder_control_regression: ok\n";
	return 0;
}
