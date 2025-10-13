#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include <string>
#include "./../fe/Game.h"
#include "gfx.h"

namespace fe {

	class MainWindow {

		int m_sel_chunk, m_sel_screen;
		std::size_t m_atlas_tileset_no, m_atlas_palette_no,
			m_atlas_new_tileset_no, m_atlas_new_palette_no;
		fe::gfx m_gfx;

		void imgui_text(const std::string& p_str);
		void regenerate_atlas_if_needed(SDL_Renderer* p_rnd,
			const fe::Game& p_game);

		std::size_t get_default_tileset_no(std::size_t p_chunk_no, std::size_t p_screen_no) const;
		std::size_t get_default_palette_no(const fe::Game& p_game,
			std::size_t p_chunk_no, std::size_t p_screen_no) const;

	public:

		MainWindow(SDL_Renderer* p_rnd);
		void generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game);
		void draw(SDL_Renderer* p_rnd, const fe::Game& p_game);

		void draw_screen_window(SDL_Renderer* p_rnd, const fe::Game& p_game,
			int& p_hover_x, int& p_hover_y, bool& p_clicked) const;

		void draw_metatile_info(const fe::Game& p_game,
			std::size_t p_sel_chunk, std::size_t p_sel_screen,
			std::size_t p_sel_x, std::size_t p_sel_y);
	};

}

#endif
