#ifndef FE_SPRITEANIMATIONFRAME_H
#define FE_SPRITEANIMATIONFRAME_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <optional>

using byte = unsigned char;

namespace fe {

	struct SpriteFrameTile {
		byte index;
		byte sub_palette;
		bool v_flip, h_flip;
		std::vector<byte> to_bytes(const std::map<byte, byte>& remap) const;
	};

	struct SpriteAnimationFrame {
		int offset_x, offset_y, pivot_x;
		std::vector<std::vector<std::optional<SpriteFrameTile>>> tilemap;

		SpriteAnimationFrame(const std::vector<byte>& p_rom, std::size_t p_offset);
		SpriteAnimationFrame(void); // careful! to_bytes fails for empty frames
		std::size_t w(void) const;
		std::size_t h(void) const;
		std::vector<byte> to_bytes(const std::map<byte, byte>& remap = std::map<byte, byte>()) const;
		std::vector<byte> to_bytes(const std::vector<byte>& load_list) const;
		std::map<byte, int> get_tile_usage(void) const;
		int get_empty_tile_count(void) const;

		std::string to_string(void) const;
	};

}

#endif
