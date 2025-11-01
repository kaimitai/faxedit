#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include <deque>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include "gfx.h"
#include "./../fe/Game.h"
#include "./../fe/ROM_Manager.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

namespace fe {

	enum EditMode { Tilemap, Sprites, Doors, Scrolling, Transitions, Other };

	struct Size4 {
		std::size_t x, y, w, h;
	};

	struct Message {
		std::string text;
		int status; // 0=neutral, 1=good, 2=bad
	};

	class MainWindow {

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
			m_sel_stage;

		// clipboard maps from chunk id -> rectangle with data
		std::map<std::size_t, std::vector<std::vector<byte>>> m_clipboard;

		fe::EditMode m_emode;
		std::size_t m_atlas_tileset_no, m_atlas_palette_no,
			m_atlas_new_tileset_no, m_atlas_new_palette_no;
		std::optional<fe::Game> m_game;
		fe::gfx m_gfx;
		std::deque<fe::Message> m_messages;
		fe::ROM_Manager m_rom_manager;

		// oscillating color for selected object
		SDL_Color m_pulse_color;
		float m_pulse_time;

		void imgui_text(const std::string& p_str);
		void regenerate_atlas_if_needed(SDL_Renderer* p_rnd);
		void load_rom(const std::string& p_filepath);
		std::optional<std::vector<byte>> patch_rom(void);

		std::string get_ips_path(void) const;
		std::string get_xml_path(void) const;
		std::string get_nes_path(void) const;
		std::string get_filepath(const std::string& p_ext, bool p_add_out = false) const;

		std::size_t get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_default_palette_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::string get_description(byte p_index, const std::map<byte, std::string>& p_map) const;
		std::string get_description(byte p_index, const std::vector<std::string>& p_vec) const;

		void draw_metatile_info(std::size_t p_sel_chunk, std::size_t p_sel_screen,
			std::size_t p_sel_x, std::size_t p_sel_y);

		void draw_control_window(SDL_Renderer* p_rnd);
		void draw_screen_tilemap_window(SDL_Renderer* p_rnd);

		void draw_metadata_window(SDL_Renderer* p_rnd);

		void draw_filepicker_window(SDL_Renderer* p_rnd);
		void show_output_messages(void) const;

		void add_message(const std::string& p_msg, int p_status = 0);

		std::optional<byte> show_screen_scroll_section(const std::string& p_direction,
			std::size_t p_screen_count, std::optional<byte> p_scroll_data);

		std::optional<std::pair<byte, byte>> show_position_slider(byte p_x, byte p_y);
		void show_sprite_screen(fe::Sprite_set& p_sprites, std::size_t& p_sel_sprite);
		void show_stage_door_data(fe::Door& p_door);
		void show_stages_data(void);
		void show_screen_scroll_data(void);

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

	public:

		MainWindow(SDL_Renderer* p_rnd, const std::string& p_filepath = std::string());
		void generate_textures(SDL_Renderer* p_rnd);
		void generate_metatile_textures(SDL_Renderer* p_rnd);
		void draw(SDL_Renderer* p_rnd);
	};

}

#endif
