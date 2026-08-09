#ifndef FH_TILEMAPCHANGES_H
#define FH_TILEMAPCHANGES_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

using byte = unsigned char;
using word = uint16_t;

namespace fh {

	struct TileChange {
		byte x, y, id;
	};

	struct ScreenTilemapChange {
		byte flag;
		std::vector<TileChange> changes;
	};

	class TilemapChanges {

		static constexpr byte MAX_WORLD = 127;
		static constexpr std::size_t MAX_SCREENS_PER_WORLD = 63;
		static constexpr std::size_t MAX_TILE_CHANGES = 127;

		std::vector<std::map<byte, ScreenTilemapChange>> data;
		void resize_data_if_needed(byte p_world);

	public:
		TilemapChanges(byte p_init_max_world_idx = 7);
		bool empty(void) const;
		std::vector<byte> to_bytes(word cpu_addr) const;

		static constexpr std::array<byte, 4> EOE_TILEMAP_CHANGE_HEADER{ 'K', 'E', 'F', 0x00 };
		static constexpr std::size_t DESCRIPTOR_SIZE{ 4 };

		void add_screen(byte p_world, byte p_screen, byte p_flag);
		void add_change(byte p_world, byte p_screen, byte p_x, byte p_y, byte p_id);
	};

}

#endif
