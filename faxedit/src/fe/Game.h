#ifndef FE_GAME_H
#define FE_GAME_H

#include <map>
#include <unordered_set>
#include <set>
#include <vector>
#include "./../common/klib/NES_tile.h"
#include "Chunk.h"
#include "StageManager.h"

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

	class Config;

	struct Game {

		std::vector<std::vector<klib::NES_tile>> m_tilesets;

		fe::StageManager m_stages;

		// stored as building chunk sprite data, but is globally referred to
		std::vector<fe::Sprite_set> m_npc_bundles;
		std::vector<fe::Chunk> m_chunks;
		std::vector<byte> m_rom_data;
		std::vector<NES_Palette> m_palettes;
		std::vector<fe::Spawn_location> m_spawn_locations;
		fe::Push_block_parameters m_push_block;
		std::vector<byte> m_jump_on_animation;

		Game(const std::vector<byte>& p_rom_data);
		Game(void);

		std::size_t m_ptr_chunk_metadata, m_ptr_chunk_sprite_data, m_ptr_chunk_interchunk_transitions,
			m_ptr_chunk_intrachunk_transitions, m_ptr_chunk_default_palette_idx, m_ptr_chunk_palettes,
			m_ptr_chunk_door_to_chunk, m_ptr_chunk_door_to_screen, m_ptr_chunk_door_reqs;

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

		void generate_tilesets(const fe::Config& p_config);

	private:
		std::size_t get_pointer_address(std::size_t p_offset, std::size_t p_relative_offset = 0) const;
		std::vector<std::size_t> get_screen_pointers(const std::vector<std::size_t>& p_offsets, std::size_t p_chunk_no) const;
		std::vector<std::size_t> get_screen_pointers(std::size_t p_world_ptr) const;
		void set_various(std::size_t p_chunk_no, std::size_t pt_to_various);
		void set_sprites(std::size_t p_chunk_no, std::size_t pt_to_sprites);
		void set_interchunk_scrolling(std::size_t p_chunk_no, std::size_t pt_to_interchunk);
		void set_intrachunk_scrolling(std::size_t p_chunk_no, std::size_t pt_to_intrachunk);

		fe::Sprite_set extract_sprite_set(const std::vector<byte>& p_rom_data, std::size_t p_offset) const;
	};

}

#endif
