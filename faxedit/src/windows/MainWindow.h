#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include "./../fe/Game.h"
#include "gfx.h"

namespace fe {

	class MainWindow {

		int m_sel_chunk, m_sel_screen;
		fe::gfx m_gfx;

	public:

		MainWindow(void);
		void generate_textures(SDL_Renderer* p_rnd, const fe::Game& p_game);
		void draw(SDL_Renderer* p_rnd, const fe::Game& p_game);

		void draw_screen(SDL_Renderer* p_rnd, const fe::Game& p_game) const;
	};

}

#endif
