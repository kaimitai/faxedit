#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr std::size_t TILE_FIVE{ 0x35 }, TILE_BYTES{ 16 };
	constexpr std::size_t ORG{ 0xfcce };
	constexpr std::size_t FIVE_OFFSET{ 0x284a0 };
	// made-up tile with the original crc, so the test needs no game graphics
	const std::vector<byte> STAND_IN{ 0x0b, 0x30, 0x55, 0x7a, 0x9f, 0xc4, 0xe9, 0x0e,
		0x33, 0x58, 0x7d, 0xa2, 0xa6, 0x1d, 0x5c, 0x7c };

	void require(bool p_ok, const std::string& p_what) {
		if (!p_ok)
			throw std::runtime_error(p_what);
	}

	std::vector<byte> rom_with(const std::vector<byte>& p_tile, std::size_t p_off) {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		const byte header[]{ 'N', 'E', 'S', 0x1a, 16, 0, 0x12, 0 };
		for (std::size_t i{ 0 }; i < sizeof header; ++i) rom[i] = header[i];
		for (std::size_t i{ 0 }; i < p_tile.size(); ++i) rom[p_off + i] = p_tile[i];
		return rom;
	}

	void install(const fe::Config& p_config, std::vector<byte>& p_rom, const std::string& p_spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(p_spec)) };
		fh::HackManager{}.install_general_hacks(p_config, p_rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

	void test_region(const std::string& p_region) {
		const fe::Config config(EOE_TEST_CONFIG_PATH, "", rom_with({}, 0), p_region);
		const std::size_t off{ config.constant("chr_hud_tile_offset") + TILE_FIVE * TILE_BYTES };
		require(off == FIVE_OFFSET, p_region + ": the hud 5 is not at file offset 0x284a0");

		auto fixed{ rom_with(STAND_IN, off) }, plain{ rom_with(STAND_IN, off) }, off_rom{ rom_with(STAND_IN, off) };
		install(config, fixed, "BugFixes hud_five_fix=true");
		install(config, plain, "BugFixes");
		install(config, off_rom, "BugFixes hud_five_fix=false");
		auto expected{ plain };
		expected[off + 3] = 0x06;
		require(fixed == expected, p_region + ": the fix changed more than the 5's row 3");
		require(plain[off + 3] == STAND_IN[3], p_region + ": plain BugFixes changed the 5");
		require(off_rom == plain, p_region + ": hud_five_fix=false is not plain BugFixes");

		auto changed{ STAND_IN };
		changed[3] ^= 0x01;
		auto rom{ rom_with(changed, off) };
		const auto before{ rom };
		bool threw{ false };
		try { install(config, rom, "BugFixes hud_five_fix=true"); } catch (const std::exception&) { threw = true; }
		require(threw, p_region + ": a changed 5 was accepted");
		require(rom == before, p_region + ": refused but changed the rom");
	}
}

int main() {
	try {
		for (const std::string region : { "us", "us-rev-a", "eu", "jp", "en-transl" })
			test_region(region);
		std::cout << "bugfixes hud_five_fix regression passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
