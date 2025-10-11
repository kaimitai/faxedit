#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include <string>
#include "./../fe/Game.h"
#include "gfx.h"

namespace fe {

	class MainWindow {

		int m_sel_chunk, m_sel_screen;
		fe::gfx m_gfx;
		SDL_Texture* m_screen_txt;

		std::size_t get_tileset(int p_chunk_no, int p_screen_no) const;

		void imgui_text(const std::string& p_str);

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
