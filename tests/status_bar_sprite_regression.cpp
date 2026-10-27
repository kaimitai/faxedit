#include "fe/Config.h"
#include "fe/game/GameManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins sync_status_bar_sprite: an original sprite 0 copy takes the hud's P,
// found through the common sprite chr pointer wherever the block is; a copy
// edited on its own, or one that already matches, is left alone. the tiles
// here are made-up bytes (the copy has the original P's crc, since the sync
// checks the crc), so no game data is in the test.
namespace {
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr std::size_t TILE_BYTES{ 16 };
	constexpr std::size_t HUD_P_OFFSET{ 0x28340 }, SPRITE0_OFFSET{ 0x21004 };
	constexpr std::size_t COMMON_VANILLA{ 0x0c04 }, COMMON_MOVED{ 0x1234 };
	const std::vector<byte> ORIGINAL_COPY{ 0x07, 0x3c, 0x71, 0xa6, 0xdb, 0x10, 0x45, 0x7a,
		0xaf, 0xe4, 0x19, 0x4e, 0x8a, 0xb6, 0x17, 0x56 };
	const std::vector<byte> EDITED_P{ 0x81, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x81,
		0x00, 0x7e, 0x00, 0x7e, 0x00, 0x7e, 0x00, 0x7e };

	void require(bool p_ok, const std::string& p_what) {
		if (!p_ok)
			throw std::runtime_error(p_what);
	}

	std::vector<byte> rom_with(const fe::Config& p_config, std::size_t p_common,
		const std::vector<byte>& p_hud_p, const std::vector<byte>& p_copy) {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		const auto ptr{ p_config.pointer("gfx_common_chr_data_ptr") };
		rom[ptr.first] = static_cast<byte>(p_common);
		rom[ptr.first + 1] = static_cast<byte>(p_common >> 8);
		const std::size_t hud{ p_config.constant("chr_hud_tile_offset") + 0x1f * TILE_BYTES };
		const std::size_t copy{ ptr.second + p_common + 0x3f * TILE_BYTES };
		for (std::size_t i{ 0 }; i < TILE_BYTES; ++i) {
			rom[hud + i] = p_hud_p[i];
			rom[copy + i] = p_copy[i];
		}
		return rom;
	}

	void test_region(const std::string& p_region) {
		const fe::Config config(EOE_TEST_CONFIG_PATH, "", std::vector<byte>(ROM_SIZE, 0xff), p_region);
		const auto ptr{ config.pointer("gfx_common_chr_data_ptr") };
		// both tiles sit at the same place in every region's rom
		require(config.constant("chr_hud_tile_offset") + 0x1f * TILE_BYTES == HUD_P_OFFSET,
			p_region + ": the hud P is not at file offset 0x28340");
		require(ptr.second + COMMON_VANILLA + 0x3f * TILE_BYTES == SPRITE0_OFFSET,
			p_region + ": sprite tile $7f is not at file offset 0x21004");

		for (const std::size_t common : { COMMON_VANILLA, COMMON_MOVED }) {
			const std::size_t copy{ ptr.second + common + 0x3f * TILE_BYTES };
			const std::string where{ p_region + " common chr at " + std::to_string(common) };

			auto rom{ rom_with(config, common, EDITED_P, ORIGINAL_COPY) };
			require(fe::game::sync_status_bar_sprite(config, rom), where + ": an original copy was not synced");
			auto want{ rom_with(config, common, EDITED_P, EDITED_P) };
			require(rom == want, where + ": the sync changed more than the copy, or not to the hud P");

			auto edited_copy{ ORIGINAL_COPY };
			edited_copy[5] ^= 0x01;
			rom = rom_with(config, common, EDITED_P, edited_copy);
			auto before{ rom };
			require(!fe::game::sync_status_bar_sprite(config, rom) && rom == before,
				where + ": a copy edited on its own was overwritten");

			rom = rom_with(config, common, ORIGINAL_COPY, ORIGINAL_COPY);
			before = rom;
			require(!fe::game::sync_status_bar_sprite(config, rom) && rom == before,
				where + ": a copy that matches the hud P was rewritten");
			require(copy + TILE_BYTES <= ROM_SIZE, where + ": copy out of range");
		}
	}
}

int main() {
	try {
		for (const std::string region : { "us", "us-rev-a", "eu", "jp", "en-transl" })
			test_region(region);
		std::cout << "status bar sprite regression passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
