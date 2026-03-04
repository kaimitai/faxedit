#ifndef FE_SPRITEGFXMANAGER_H
#define FE_SPRITEGFXMANAGER_H

#include <string>
#include <utility>
#include <vector>
#include "SpriteAnimationFrame.h"
#include "./../../common/klib/NES_tile.h"

using byte = unsigned char;

namespace fe {

	class Config;
	class ROM_Manager;

	struct SpriteGfxCollection {
		std::vector<klib::NES_tile> tiles;
		std::vector<SpriteAnimationFrame> frames;
	};

	struct SpriteGfxManager {

		SpriteGfxCollection portraits, player;
		std::vector<SpriteGfxCollection> npcs;
		std::vector<klib::NES_tile> common_chr;

		// calculating functions
		std::vector<byte> calc_portrait_ppu_load_list(std::size_t p_portrait_no) const;
		std::size_t get_sprite_chr_cutoff(const fe::Config& p_config, const std::vector<byte>& p_rom) const;

		// patching functions
		void patch_portrait_data(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);
		void patch_npc_data(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr) const;

		void load_portrait_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);
		void load_player_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);
		void load_npc_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);

	public:
		SpriteGfxManager(void) = default;

		void load_rom_data(const fe::Config& p_config, const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);
		void patch_rom(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_rom_mgr);

		void merge_portrait_collection(const SpriteGfxCollection& coll);

		std::vector<byte> extract_ppu_idx_lookup_ff_delim(const std::vector<byte>& p_rom, std::size_t p_offset) const;
		std::vector<klib::NES_tile> extract_chr_tiles(const std::vector<byte>& p_rom, std::size_t p_offset, std::size_t p_count) const;
		void canonicalize_gfx_collection(SpriteGfxCollection& coll);
		void canonicalize_gfx_collection_chr_bank(SpriteGfxCollection& coll);
		void dedup_gfx_collection(SpriteGfxCollection& coll);
		void sort_gfx_collection(SpriteGfxCollection& coll);

		int normalize_frame(const std::vector<byte>& ppu_order, fe::SpriteAnimationFrame& frame,
			std::size_t p_chr_bank_size);

		template <class T>
		static std::size_t find_with_overflow(const std::vector<T>& needle,
			const std::vector<T>& haystack) {
			const std::size_t hsize = haystack.size();
			const std::size_t nsize = needle.size();

			for (std::size_t i = 0; i < hsize; ++i)
			{
				bool match = true;

				for (std::size_t j = 0; j < nsize; ++j)
				{
					const std::size_t hindex = i + j;

					if (hindex >= hsize)
						break; // overflow allowed -> remaining needle auto-matches

					if (!(haystack[hindex] == needle[j])) {
						match = false;
						break;
					}
				}

				if (match)
					return i;
			}

			return hsize; // no match
		}
	};

}

#endif
