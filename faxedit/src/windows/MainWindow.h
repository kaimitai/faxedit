#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include <deque>
#include <map>
#include <string>
#include "gfx.h"
#include "./../fe/Game.h"
#include "./../fe/ROM_Manager.h"

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
			m_sel_metatile, m_sel_tilemap_sub_palette;

		// clipboard maps from chunk id -> rectangle with data
		std::map<std::size_t, std::vector<std::vector<byte>>> m_clipboard;

		fe::EditMode m_emode;
		std::size_t m_atlas_tileset_no, m_atlas_palette_no,
			m_atlas_new_tileset_no, m_atlas_new_palette_no;
		fe::gfx m_gfx;
		std::deque<fe::Message> m_messages;
		fe::ROM_Manager m_rom_manager;

		void imgui_text(const std::string& p_str);
		void regenerate_atlas_if_needed(SDL_Renderer* p_rnd,
			const fe::Game& p_game);

		std::size_t get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_default_palette_no(const fe::Game& p_game,
			std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::string get_description(byte p_index, const std::map<byte, std::string>& p_map) const;
		std::string get_description(byte p_index, const std::vector<std::string>& p_vec) const;

		void draw_metatile_info(const fe::Game& p_game,
			std::size_t p_sel_chunk, std::size_t p_sel_screen,
			std::size_t p_sel_x, std::size_t p_sel_y);

		void draw_control_window(SDL_Renderer* p_rnd, fe::Game& p_game);
		void draw_screen_tilemap_window(SDL_Renderer* p_rnd, fe::Game& p_game);

		void draw_chunk_window(SDL_Renderer* p_rnd, fe::Game& p_game);

		void add_message(const std::string& p_msg, int p_status = 0);

		std::optional<byte> show_screen_scroll_section(const std::string& p_direction,
			std::size_t p_screen_count, std::optional<byte> p_scroll_data);

		std::optional<std::pair<byte, byte>> show_position_slider(byte p_x, byte p_y);
		std::string get_editmode_as_string(void) const;

		Size4 get_selection_dims(void) const;
		void clipboard_copy(const fe::Game& p_game);
		void clipboard_paste(fe::Game& p_game);

	public:

		MainWindow(SDL_Renderer* p_rnd);
		void generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game);
		void generate_metatile_textures(SDL_Renderer* p_rnd, const fe::Game& p_game);
		void draw(SDL_Renderer* p_rnd, fe::Game& p_game);
	};

}

#endif
