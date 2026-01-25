#ifndef FE_CHR_STRUCTURES_H
#define FE_CHR_STRUCTURES_H

#include <optional>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

// structures needed to exhange chr-related data across the application
namespace fe {

	// chr-tile with metadata
	// read-only: can be used for deduplication bmp-import etc, but cannot be re-indexed or touched
	// allowed: can be used at all - chr tiles that are dynamic during runtime are not allowed
	struct ChrGfxTile {
		klib::NES_tile m_tile;
		bool m_readonly, m_allowed;

		ChrGfxTile(void);
		ChrGfxTile(const klib::NES_tile& p_tile,
			bool p_readonly, bool p_allowed);
	};

	// bg metatile-definition - a 2x2 vector of chr-tile indexes, and a palette index
	struct ChrMetaTile {
		std::vector<std::size_t> m_idxs;
		std::size_t m_palette;

		ChrMetaTile(void);
	};

	// the kind of object that is exchanged by the renderer, the bmp-import and such
	// constains chr-metatiles, palette and attribute table
	struct ChrTilemap {
		std::vector<std::vector<std::optional<ChrMetaTile>>> m_tilemap;
		std::vector<klib::NES_tile> m_tiles;
		std::vector<std::vector<byte>> m_palette;

		void set_flat_palette(const std::vector<byte>& p_palette);
		void populate_attribute(byte p_tl, byte p_tr, byte p_bl, byte p_br);
		void set_attribute(std::size_t p_mt_x, std::size_t p_mt_y, byte p_attr);

		static std::vector<std::vector<byte>> flat_pal_to_2d_pal(
			const std::vector<byte>& p_flat_pal);
	};

}

#endif
