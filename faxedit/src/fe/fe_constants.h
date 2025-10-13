#ifndef FE_CONSTANTS_H
#define FE_CONSTANTS_H

#include <vector>

using byte = unsigned char;

namespace fe {

	namespace c {

		// pointers to chunk data pointer tables
		// indexed by chunk no (offet address by (2 * chunk no) to get the ptr to the chunk we're interested in)
		constexpr std::size_t PTR_CHUNK_METADATA{ 0xc010 };
		constexpr std::size_t PTR_CHUNK_PALETTES{ 0x2c010 };
		constexpr std::size_t PTR_CHUNK_SPRITE_DATA{ 0x2c220 };
		constexpr std::size_t PTR_CHUNK_DEFAULT_PALETTE_IDX{ 0x3df5c };
		constexpr std::size_t PTR_CHUNK_INTRACHUNK_TRANSITIONS{ 0x3ea47 };
		constexpr std::size_t PTR_CHUNK_INTERCHUNK_TRANSITIONS{ 0x3eaac };

		// ptrs to the screen data for each of the 8 chunks
		const std::vector<std::size_t> PTR_CHUNK_SCREEN_DATA{ 0x10, 0x12, 0x14, 0x4010, 0x4012, 0x8010, 0x8012, 0x8014 };

		// a map from our chunk indexing to the indexing used by the ROM pointer tables
		inline const std::vector<std::size_t> MAP_CHUNK_IDX{ 0, 3, 1, 2, 6, 4, 5, 7 };

		// map to background gfx start locations - treated as immutable
		// when extracting the 256 nes tiles starting at any of these locations, the chunk tilemap indexes will match
		const std::vector<std::size_t> OFFSETS_BG_GFX {
			0xf810,   // eolis
			0x10810,  // mist
			0x11810,  // town
			0x10010,  // road to apolune, towers, springs and screen outside forepaw and apolune
			0x11010,  // branches
			0x13010,  // dartmoor + evil fortress
			0x11e10,  // guru and king screens
			0x13010,  // dartmoor + evil fortress
			0x12410,  // set used for the shop and building interior screens
			0x12a10   // set used for the training shops
		};
	}

}

#endif
