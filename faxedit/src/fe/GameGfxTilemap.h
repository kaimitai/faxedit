#ifndef FE_GAME_GFX_TILEMAP_H
#define FE_GAME_GFX_TILEMAP_H

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "./../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fe {


	struct ChrGfxTile {
		klib::NES_tile m_tile;
		bool m_readonly, m_allowed;

		ChrGfxTile(void);
		ChrGfxTile(const klib::NES_tile& p_tile,
			bool p_readonly, bool p_allowed);
	};


	struct ChrMetaTile {
		std::vector<std::size_t> m_idxs;
		std::size_t m_palette;

		ChrMetaTile(void);
	};

	struct ChrTilemap {
		std::vector<std::vector<std::optional<ChrMetaTile>>> m_tilemap;
		std::vector<klib::NES_tile> m_tiles;
		std::vector<std::vector<byte>> m_palette;

		void set_flat_palette(const std::vector<byte>& p_palette);
		void populate_attribute(byte p_tl, byte p_tr, byte p_bl, byte p_br);
		void set_attribute(std::size_t p_mt_x, std::size_t p_mt_y, byte p_attr);
	};

	struct GameGfxTilemap {
		std::string m_gfx_name;

		std::size_t mt_x, mt_y, mt_w, mt_h;

		std::size_t m_rom_offset_tilemap,
			m_rom_offset_attr,
			m_rom_offset_pal,
			m_rom_offset_chr,
			m_chr_ppu_index,
			m_chr_ppu_count,
			m_alpha_chr_offset,
			m_numeric_chr_offset;

		bool m_patch_attributes, m_patch_palette, m_fix_tile_0,
			m_add_alphanumeric;
		bool m_loaded;

		std::vector<std::vector<ChrMetaTile>> m_tilemap;
		std::vector<fe::ChrGfxTile> m_chr_tiles;
		std::vector<std::vector<std::size_t>> m_attributes;
		std::vector<byte> m_palette;

		GameGfxTilemap(const std::string& p_name,
			std::size_t p_mtx, std::size_t p_mty,
			std::size_t p_mtw, std::size_t p_mth,
			std::size_t p_tilemap_rom_offset,
			std::size_t p_chr_rom_offset,
			std::size_t p_ppu_index,
			std::size_t p_ppu_count,
			bool p_fix_tile_0,
			bool p_patch_attr,
			bool p_patch_palette,
			std::size_t p_attr_rom_offset = 0,
			std::size_t p_pal_rom_offset = 0,
			bool p_add_alphanumeric = false,
			std::size_t p_alpha_chr_offset = 0,
			std::size_t p_numeric_chr_offset = 0);

		void load_from_rom(const std::vector<byte>& p_rom);
		void commit_import(const fe::ChrTilemap& p_result);

		// helpers
		void initialize_attributes(std::size_t p_attr);

		// convert to the type of tilemap object the gfx renderer requires
		fe::ChrTilemap get_chrtilemap(void) const;
		
		// ROM patching
		void patch_rom(std::vector<byte>& p_rom) const;
		void patch_palette(std::vector<byte>& p_rom) const;
		void patch_attributes(std::vector<byte>& p_rom) const;
		void patch_tilemap(std::vector<byte>& p_rom) const;
		void patch_chr_tiles(std::vector<byte>& p_rom) const;

		static std::pair<std::size_t, std::size_t> mt_coords_to_attribute(
			std::size_t mtx, std::size_t mty);
		static std::vector<std::vector<byte>> flat_pal_to_2d_pal(const std::vector<byte>& p_flat_pal);
	};

}

#endif
