#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include <deque>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include "gfx.h"
#include "./../fe/Config.h"
#include "./../fe/Game.h"
#include "./../fe/ROM_Manager.h"
#include "./../fi/IScriptLoader.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

namespace fe {

	enum EditMode { Tilemap, Sprites, Doors, Scrolling, Transitions, Other };
	enum ChrPickerMode { Default, HUD, All };
	enum GfxEditMode { WorldChr, Palettes };

	struct Size4 {
		std::size_t x, y, w, h;
	};

	struct AnimationGUIData {
		// w and h for the sprite
		int w, h;
		// sprite category
		fe::SpriteCategory m_cat;
		// x and y offsets per frame
		std::vector<std::pair<int, int>> offsets;
	};

	struct Message {
		std::string text;
		int status; // 0=neutral, 1=good, 2=bad
	};

	class MainWindow {

		// config
		fe::Config m_config;

		// config values we will cache
		std::map<byte, std::string> m_labels_cmd_byte,
			m_labels_door_reqs, m_labels_block_props,
			m_labels_palettes, m_labels_spec_sprite_sets;
		std::vector<std::string> m_labels_worlds, m_labels_sprites,
			m_labels_buildings, m_labels_tilesets;
		std::vector<std::size_t> m_world_tileset_idx,
			m_building_rooms_tileset_idx,
			m_tileset_start, m_tileset_size;

		std::size_t m_sprite_count, m_iscript_count;

		// file info
		std::filesystem::path m_path;
		std::string m_filename;

		std::size_t
			// world, screen
			m_sel_chunk, m_sel_screen,
			// screen objects
			m_sel_door, m_sel_sprite,
			// tilemap editor
			// select a tile or a region
			m_sel_tile_x, m_sel_tile_y,
			m_sel_tile_x2, m_sel_tile_y2,
			// metatile select editor
			m_sel_metatile, m_sel_tilemap_sub_palette,

			// metatile definition editor
			m_sel_nes_tile,

			// spawn location editor
			m_sel_spawn_location,
			// npc bundles aka building parameters
			m_sel_npc_bundle, m_sel_npc_bundle_sprite,
			// selected stage
			m_sel_stage,
			// selected iscript
			m_sel_iscript,

			// gfx selectors
			m_sel_gfx_ts_world,
			m_sel_gfx_ts_screen;

		// sprite dimensions; holding sprite size and
		// cartesian offsets per animation frame
		std::vector<fe::AnimationGUIData> m_sprite_dims;

		// clipboard maps from chunk id -> rectangle with data
		std::map<std::size_t, std::vector<std::vector<byte>>> m_clipboard;

		// rendering options
		bool m_animate, m_mattock_overlay, m_door_req_overlay, m_iscript_window,
			m_show_grid, m_iscript_win_set_focus,
			m_gfx_window;

		// functionality toggles
		bool m_show_sprite_sets_in_buildings;

		std::vector<char> m_overlays;

		fe::EditMode m_emode;
		fe::GfxEditMode m_gfx_emode;
		fe::ChrPickerMode m_chr_picker_mode;
		std::size_t m_atlas_tileset_no, m_atlas_palette_no,
			m_atlas_new_tileset_no, m_atlas_new_palette_no;
		bool m_atlas_force_update;
		std::optional<fe::Game> m_game;
		fe::gfx m_gfx;
		std::deque<fe::Message> m_messages;
		fe::ROM_Manager m_rom_manager;
		std::map<std::size_t, std::vector<fi::AsmToken>> m_iscripts;

		// oscillating color for selected object
		SDL_Color m_pulse_color;
		float m_pulse_time, m_anim_time;
		std::size_t m_anim_frame;

		void imgui_text(const std::string& p_str) const;
		void regenerate_atlas_if_needed(SDL_Renderer* p_rnd);
		void load_rom(SDL_Renderer* p_rnd, const std::string& p_filepath,
			const std::string& p_region = std::string());
		void cache_config_variables(void);
		std::optional<std::vector<byte>> patch_rom(void);

		std::string get_ips_path(void) const;
		std::string get_xml_path(void) const;
		std::string get_nes_path(void) const;
		std::string get_filepath(const std::string& p_ext, bool p_add_out = false) const;

		std::size_t get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_default_palette_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::string get_description(byte p_index, const std::map<byte, std::string>& p_map) const;
		std::string get_description(byte p_index, const std::vector<std::string>& p_vec) const;
		std::string get_sprite_label(std::size_t p_sprite_id) const;

		void draw_metatile_info(std::size_t p_sel_chunk, std::size_t p_sel_screen,
			std::size_t p_sel_x, std::size_t p_sel_y);

		void draw_control_window(SDL_Renderer* p_rnd);
		void draw_screen_tilemap_window(SDL_Renderer* p_rnd);
		void draw_metadata_window(SDL_Renderer* p_rnd);
		void draw_iscript_window(SDL_Renderer* p_rnd);
		void draw_filepicker_window(SDL_Renderer* p_rnd);
		void draw_gfx_window(SDL_Renderer* p_rnd);

		void show_output_messages(void) const;

		void add_message(const std::string& p_msg, int p_status = 0);

		std::optional<byte> show_screen_scroll_section(const std::string& p_direction,
			std::size_t p_screen_count, std::optional<byte> p_scroll_data);

		std::optional<std::pair<byte, byte>> show_position_slider(byte p_x, byte p_y);
		void show_sprite_screen(fe::Sprite_set& p_sprites, std::size_t& p_sel_sprite);
		void show_sprite_npc_bundle_screen(void);
		void show_stage_door_data(fe::Door& p_door);
		void show_stages_data(void);
		void show_screen_scroll_data(void);

		void show_mt_definition_tab(SDL_Renderer* p_rnd, fe::Chunk& p_chunk);

		void show_sprite_set_contents(std::size_t p_sprite_set);

		bool check_patched_size(const std::string& p_data_type, std::size_t p_patch_data_size, std::size_t p_max_data_size);

		std::string get_editmode_as_string(void) const;

		Size4 get_selection_dims(void) const;
		void clipboard_copy(void);
		void clipboard_paste(bool l_update_data = true);

		void scroll_button(std::string p_button_text, std::optional<byte> p_scroll_dest);
		void scroll_left_button(const fe::Screen& p_screen);
		void scroll_right_button(const fe::Screen& p_screen);
		void scroll_up_button(const fe::Screen& p_screen);
		void scroll_down_button(const fe::Screen& p_screen);
		void enter_door_button(const fe::Screen& p_screen);
		void transition_sw_button(const fe::Screen& p_screen);
		void transition_ow_button(const fe::Screen& p_screen);

		// screen element draw functions
		void draw_sprites(SDL_Renderer* p_rnd,
			const std::vector<fe::Sprite>& p_sprites,
			std::size_t p_sel_sprite_no) const;

		// helper functions for the bmp import
		void gen_read_only_chr_idx_non_building(std::size_t p_tileset_no,
			std::size_t p_world_no, std::set<std::size_t>& p_idxs) const;
		void gen_read_only_chr_idx_building(std::size_t p_tileset_no,
			std::size_t p_world_no, std::size_t p_screen_no,
			std::set<std::size_t>& p_idxs) const;
		void gen_building_metatile_usage(std::size_t p_screen_no,
			std::set<std::size_t>& p_idxs) const;
		void gen_fixed_building_metatiles(std::size_t p_tileset_no,
			std::set<std::size_t>& p_idxs) const;

	public:

		MainWindow(SDL_Renderer* p_rnd,
			const std::string& p_filepath = std::string(),
			const std::string& p_region = std::string());
		void generate_textures(SDL_Renderer* p_rnd);
		void generate_metatile_textures(SDL_Renderer* p_rnd);
		void draw(SDL_Renderer* p_rnd);
	};

}

#endif
