#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevLandingTuck to known bytes: each profile, a flag at
// either end of a flag byte, off and mode=vanilla which install nothing, and
// the crop taken from the player frame directory, including two gear groups
// that crop to the same record and must share one. every refusal must write
// nothing. the hook sites are in banks 14 and 15, the body is in bank 15
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr std::size_t FRAME_PTR{ 0x1c018 }, BANK7_ZERO{ 0x1c010 };
	constexpr std::size_t TILE_COUNTS{ 0x3eda5 };
	constexpr std::size_t DIRECTORY{ 0x0a00 };     // relative to the bank 7 zero
	constexpr std::size_t FRAMES{ 0x0b00 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	// the seven vanilla sites the hack replaces
	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 15, 0xe0d3, from_hex("20e8e0"));           // jsr $e0e8
		put(rom, 15, 0xec43, from_hex("2039f0"));           // jsr $f039
		put(rom, 14, 0xb9d1, from_hex("4c39f0"));           // jmp $f039
		put(rom, 14, 0xb875, from_hex("4c39f0"));           // jmp $f039
		put(rom, 15, 0xd127, from_hex("8654a50d"));         // stx $54 / lda $0d
		put(rom, 15, 0xe0aa, from_hex("a900859f"));         // lda #$00 / sta $9f
		put(rom, 15, 0xd8f4, from_hex("a5a42940"));         // lda $a4 / and #$40

		// a player frame directory: eight gear groups of eight frames. only
		// frame 3 of each group is read, and groups 5, 6 and 7 are given the
		// same art so the installer has something to fold together
		rom[FRAME_PTR] = static_cast<byte>(DIRECTORY);
		rom[FRAME_PTR + 1] = static_cast<byte>(DIRECTORY >> 8);
		for (std::size_t group{ 0 }; group < 8; ++group) {
			const std::size_t art{ group < 5 ? group : 5 };
			const std::size_t frame{ FRAMES + art * 0x20 };
			const std::size_t entry{ BANK7_ZERO + DIRECTORY + 2 * (group * 8 + 3) };
			rom[entry] = static_cast<byte>(frame);
			rom[entry + 1] = static_cast<byte>(frame >> 8);
			// 16x32, no offset, pivot 8, then six cells; the third is empty
			std::size_t at{ BANK7_ZERO + frame };
			for (byte b : from_hex("31000008")) rom[at++] = b;
			for (std::size_t cell{ 0 }; cell < 6; ++cell) {
				if (cell == 2) { rom[at++] = 0xff; continue; }
				rom[at++] = static_cast<byte>(0x10 + art * 8 + cell);
				rom[at++] = static_cast<byte>(cell & 3);
			}
			rom[TILE_COUNTS + group] = 0x7f;
		}
		return rom;
	}

	fe::Config config(const std::vector<byte>& rom) {
		fe::Config result;
		result.load_definitions(EOE_TEST_CONFIG_PATH, "");
		result.set_region("us");
		result.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		return result;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto cfg{ config(rom) };
		const auto all{ fh::parse_general_hacks(spec) };
		auto used{ fh::HackManager{}.install_general_hacks(cfg, rom, 15,
			ORG, 0xffe0, fh::filter_general_hacks(15, all), nullptr) };
		fh::HackManager{}.install_general_hacks(cfg, rom, 14,
			0xbdb5, 0xc000, fh::filter_general_hacks(14, all), nullptr);
		return used;
	}

	void refused(std::vector<byte> rom, const std::string& spec, const std::string& why) {
		const auto before{ rom };
		bool threw{ false };
		try { install(rom, spec); } catch (const std::exception&) { threw = true; }
		require(threw, "must refuse: " + why);
		require(rom == before, "refusal must not write: " + why);
	}

	const std::string MEDIUM_HEX{ "20e8e0084820d9fc682860a554c9ffd03aad3804d035a5a42998d02fa5a52983d029a51929f0d023a5a42905f006a9808dfe0460adfe04300529074c0efda904f00938e90109408dfe0460a9008dfe04600848adfe042940d00568284c39f068284a4a4a0aaabd89fd853abd8afd853bad0001484c7df00848adfe042940f009a528186908b002852868284c39f00848adfe042940f00368286068284c39f0a9008dfe0460206dfd8654a50d60206dfd859f60206dfda5a429406099fda8fdb7fdc6fdd5fde4fde4fde4fd2100080810001101ff1303140015012100080818001901ff1b031c001d012100080820002101ff2303240025012100080828002901ff2b032c002d012100080830003101ff3303340035012100080838003901ff3b033c003d01" };
	const std::string HEAVY_FLAG_HEX{ "20e8e0084820d9fc682860ad01012980f040a554c9ffd03aad3804d035a5a42998d02fa5a52983d029a51929f0d023a5a42905f006a9808dfe0460adfe04300529074c15fda906f00938e90109408dfe0460a9008dfe04600848adfe042940d00568284c39f068284a4a4a0aaabd90fd853abd91fd853bad0001484c7df00848adfe042940f009a528186908b002852868284c39f00848adfe042940f00368286068284c39f0a9008dfe04602074fd8654a50d602074fd859f602074fda5a4294060a0fdaffdbefdcdfddcfdebfdebfdebfd2100080810001101ff1303140015012100080818001901ff1b031c001d012100080820002101ff2303240025012100080828002901ff2b032c002d012100080830003101ff3303340035012100080838003901ff3b033c003d01" };

	std::string hex_of(const std::vector<byte>& rom, byte bank, word cpu, std::size_t n) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		std::string out;
		for (std::size_t i{ 0 }; i < n; ++i) {
			const char* d{ "0123456789abcdef" };
			out += d[rom[off + i] >> 4];
			out += d[rom[off + i] & 15];
		}
		return out;
	}
}

int main() {
	try {
		// the default profile, medium, holds the pose four frames
		auto rom{ vanilla_rom() };
		require(install(rom, "AtlasDevLandingTuck") == 293, "medium is 293 bytes");
		require(hex_of(rom, 15, ORG, 293) == MEDIUM_HEX, "medium body");
		require(hex_of(rom, 15, 0xe0d3, 3) == "20cefc", "the tick site calls the state machine");
		require(hex_of(rom, 15, 0xec43, 3) == "201ffd", "the pose site calls the chooser");
		require(hex_of(rom, 14, 0xb9d1, 3) == "4c45fd", "the shield drawer jumps to the chooser");
		require(hex_of(rom, 14, 0xb875, 3) == "4c5cfd", "the weapon drawer jumps to the chooser");
		require(hex_of(rom, 15, 0xd127, 4) == "2073fdea", "the transition site clears the state");
		require(hex_of(rom, 15, 0xe0aa, 4) == "207bfdea", "the init site clears the state");
		require(hex_of(rom, 15, 0xd8f4, 4) == "2081fdea", "the death site clears the state");

		// a flag costs seven bytes and nothing else moves but the labels
		auto flagged{ vanilla_rom() };
		require(install(flagged, "AtlasDevLandingTuck profile=heavy flag=7") == 300, "heavy with a flag is 300 bytes");
		require(hex_of(flagged, 15, ORG, 300) == HEAVY_FLAG_HEX, "heavy flag body");

		auto off_rom{ vanilla_rom() };
		const auto before{ off_rom };
		require(install(off_rom, "AtlasDevLandingTuck profile=off") == 0, "off must install nothing");
		require(off_rom == before, "off must not write");
		require(install(off_rom, "AtlasDevLandingTuck mode=vanilla") == 0, "vanilla must install nothing");
		require(off_rom == before, "vanilla must not write");

		refused(vanilla_rom(), "AtlasDevLandingTuck profile=deep", "an unknown profile");
		refused(vanilla_rom(), "AtlasDevLandingTuck mode=off", "mode must be vanilla");
		refused(vanilla_rom(), "AtlasDevLandingTuck flag=248", "a flag above 247");
		refused(vanilla_rom(), "AtlasDevLandingTuck profile=off flag=248", "a bad flag even when installing nothing");
		refused(vanilla_rom(), "AtlasDevLandingTuck tuck=3", "an unknown parameter");

		// every verified site, one byte at a time
		const std::vector<std::pair<byte, word>> sites{
			{ 15, 0xe0d3 }, { 15, 0xec43 }, { 14, 0xb9d1 }, { 14, 0xb875 },
			{ 15, 0xd127 }, { 15, 0xe0aa }, { 15, 0xd8f4 } };
		for (const auto& [bank, cpu] : sites) {
			auto dirty{ vanilla_rom() };
			dirty[klib::Asm6502::get_file_offset(bank, cpu)] ^= 0xff;
			refused(dirty, "AtlasDevLandingTuck", std::format("a changed site at ${:04x}", cpu));
		}

		// a jump frame that is not the crop's source shape
		auto wrong{ vanilla_rom() };
		wrong[BANK7_ZERO + FRAMES] = 0x30;
		refused(wrong, "AtlasDevLandingTuck", "a jump frame of the wrong shape");

		// a tile the gear group does not load
		auto over{ vanilla_rom() };
		over[TILE_COUNTS] = 0x01;
		refused(over, "AtlasDevLandingTuck", "a crop tile outside the group's load list");

		// no free space left
		auto full{ vanilla_rom() };
		full[klib::Asm6502::get_file_offset(15, 0xfd00)] = 0x00;
		refused(full, "AtlasDevLandingTuck", "occupied bank 15 space");

		std::cout << "atlas_landing_tuck_regression: ok\n";
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_landing_tuck_regression: " << e.what() << "\n";
		return 1;
	}
}
