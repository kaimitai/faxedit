#ifndef FE_GAME_H
#define FE_GAME_H

#include <map>
#include <unordered_set>
#include <set>
#include <vector>
#include "./../common/klib/NES_tile.h"
#include "Chunk.h"
#include "StageManager.h"
#include "Scene.h"
#include "PaletteMusicMap.h"
#include "GameGfxTilemap.h"

using byte = unsigned char;
using Tilemap = std::vector<std::vector<byte>>;
using NES_Palette = std::vector<byte>;

namespace fe {

	struct Spawn_location {
		byte m_world, m_screen, m_stage, m_x, m_y,
			m_sprite_set;
	};

	struct Push_block_parameters {
		byte m_stage, m_screen, m_x, m_y,
			m_block_count, m_source_0, m_source_1,
			m_target_0, m_target_1,
			m_pos_delta, m_draw_block,
			m_cover_x, m_cover_y;
	};

	struct Fog {
		byte m_world_no, m_palette_no;

		Fog(byte p_world_no,
			byte p_palette_no) :
			m_world_no{ p_world_no },
			m_palette_no{ p_palette_no }
		{
		}

		Fog(void) :
			m_world_no{ 2 },
			m_palette_no{ 10 }
		{
		}
	};

	class Config;

	struct Game {

		std::vector<std::vector<klib::NES_tile>> m_tilesets;

		fe::StageManager m_stages;
		std::vector<fe::Scene> m_building_scenes;
		fe::PaletteMusicMap m_pal_to_music;

		// stored as building chunk sprite data, but is globally referred to
		std::vector<fe::Sprite_set> m_npc_bundles;
		std::vector<fe::Chunk> m_chunks;
		std::vector<byte> m_rom_data;
		std::vector<NES_Palette> m_palettes;
		std::vector<fe::Spawn_location> m_spawn_locations;
		fe::Push_block_parameters m_push_block;
		std::vector<byte> m_jump_on_animation;
		fe::Fog m_fog;

		// gfx objects which can be loaded and patched
		std::vector<fe::GameGfxTilemap> m_game_gfx;

		Game(const fe::Config& p_config, const std::vector<byte>& p_rom_data);
		Game(void);

		// map from spawn point no to iscript no
		// the script in which the spawn is set to the key
		std::map<byte, byte> m_spawn_to_script_no;

		int calculate_spawn_locations_by_guru(void);
		bool calculate_push_block_parameters(void);

		std::set<byte> get_referenced_metatiles(std::size_t p_chunk_no) const;
		std::set<byte> get_referenced_screens(std::size_t p_chunk_no) const;

		bool is_metatile_referenced(std::size_t p_chunk_no, std::size_t p_metatile_no) const;
		bool is_screen_referenced(std::size_t p_chunk_no, std::size_t p_metatile_no) const;

		void delete_metatiles(std::size_t p_chunk_no, const std::unordered_set<byte>& p_mt_to_delete);
		void delete_screens(std::size_t p_chunk_no, const std::unordered_set<byte>& p_scr_to_delete);

		std::size_t delete_unreferenced_metatiles(std::size_t p_chunk_no);
		std::size_t delete_unreferenced_screens(std::size_t p_chunk_no);

		void generate_tilesets(const fe::Config& p_config,
			std::vector<std::size_t>& p_tileset_start,
			std::vector<std::size_t>& p_tileset_size);

		std::size_t get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_default_palette_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;

		void extract_scenes_if_empty(const fe::Config& p_config);
		void extract_palette_to_music(const fe::Config& p_config);
		void extract_fog_parameters(const fe::Config& p_config);

		// gfx functions
		void initialize_game_gfx_metadata(const fe::Config& p_config);

	private:
		std::size_t get_pointer_address(std::size_t p_offset, std::size_t p_zero_addr_rom_offset = 0) const;
		std::vector<std::size_t> get_screen_pointers(const std::vector<std::size_t>& p_offsets, std::size_t p_chunk_no) const;
		std::vector<std::size_t> get_screen_pointers(std::size_t p_world_ptr) const;
		void set_various(const fe::Config& p_config, std::size_t p_chunk_no);
		void set_sprites(size_t p_chunk_no, std::pair<std::size_t, std::size_t> pt_to_sprites);
		void set_interchunk_scrolling(const fe::Config& p_config, std::size_t p_chunk_no);
		void set_intrachunk_scrolling(const fe::Config& p_config, std::size_t p_chunk_no);

		fe::Sprite_set extract_sprite_set(const std::vector<byte>& p_rom_data, std::size_t p_offset) const;
	};

}

#endif
