#ifndef FG_GRAPHICS_MANAGER_H
#define FG_GRAPHICS_MANAGER_H

#include <vector>

#include "AnimationFrame.h"
#include "GraphicsCollection.h"
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fg {

	enum GfxCollectionType { Player, Portrait };

	struct GraphicsManager {
		GraphicsCollection m_portraits, m_player;

		void extract_graphics_collection(fg::GraphicsCollection& p_coll,
			const std::vector<byte>& p_rom,
			std::size_t p_portrait_count,
			std::size_t p_portrait_anim_frame_count,
			std::size_t p_portrait_chr_offset,
			std::size_t p_portrait_chr_count,
			const std::vector<std::size_t>& p_lookup_table_offsets,
			const std::vector<std::size_t>& p_anim_frame_offsets,
			std::size_t p_palette_offset,
			std::size_t p_frames_per_object = 1,
			const std::vector<std::size_t>& p_lookup_table_sizes = std::vector<std::size_t>());

		std::vector<klib::NES_tile> extract_nes_tiles(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_tile_count) const;
		std::vector<std::size_t> extract_lookup_table_ff_delim(const std::vector<byte>& p_rom,
			std::size_t p_offset, std::size_t p_max_size = 255) const;
		std::vector<std::vector<byte>> extract_palette(const std::vector<byte>& p_rom,
			std::size_t p_offset) const;
	};

}

#endif
