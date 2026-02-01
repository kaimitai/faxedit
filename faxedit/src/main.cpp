#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "./common/imgui/imgui.h"
#include "./common/imgui/imgui_impl_sdl3.h"
#include "./common/imgui/imgui_impl_sdlrenderer3.h"
#include "./common/klib/Kfile.h"
#include "./windows/MainWindow.h"
#include "./fe/fe_app_constants.h"
#include "./windows/gfx.h"
#include "./common/klib/SDL_WindowConfig.h"

constexpr char SDL_WIN_FILENAME[]{ "eoe_os_window.cfg" };
constexpr char IMGUI_WIN_FILENAME[]{ "eoe_windows.ini" };

int main(int argc, char** argv) try {

	SDL_Window* l_window{ nullptr };
	SDL_Renderer* l_rnd{ nullptr };
	bool l_exit{ false };

	if (!SDL_Init(SDL_INIT_VIDEO))
		throw std::runtime_error(SDL_GetError());
	else {
		SDL_Event e;

		klib::WindowConfig l_wconf;
		l_wconf.loadConfig(SDL_WIN_FILENAME);

		if (!l_wconf.isVisibleOnAnyDisplay())
			l_wconf.set_defaults();

		l_window = SDL_CreateWindow("Echoes of Eolis", l_wconf.w, l_wconf.h, SDL_WINDOW_RESIZABLE);
		if (l_window == nullptr)
			throw std::runtime_error(SDL_GetError());
		else {
			SDL_SetWindowPosition(l_window, l_wconf.x, l_wconf.y);
			if (l_wconf.maximized)
				SDL_MaximizeWindow(l_window);

			l_rnd = SDL_CreateRenderer(l_window, nullptr);

			if (l_rnd == nullptr)
				throw std::runtime_error(SDL_GetError());
			else {
				//Initialize renderer color
				SDL_SetRenderDrawColor(l_rnd, 0x00, 0x00, 0x00, 0x00);
			}

			// Setup Dear ImGui context
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();

			// Setup Dear ImGui style
			ImGui::StyleColorsDark();

			// Setup Platform/Renderer backends
			ImGui_ImplSDL3_InitForSDLRenderer(l_window, l_rnd);
			ImGui_ImplSDLRenderer3_Init(l_rnd);
			ImGui::GetIO().IniFilename = IMGUI_WIN_FILENAME;
			ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

			fe::gfx::set_app_icon(l_window, fe::c::APP_ICON);

			fe::MainWindow l_main_window(l_rnd, argc > 1 ? argv[1] : "");

			uint64_t last_logic_time = SDL_GetTicks() - 1;
			uint64_t last_draw_time = SDL_GetTicks() - 17;

			uint64_t delta = 1;
			uint64_t deltaDraw = 17;

			int l_w{ 1024 }, l_h{ 768 };

			while (!l_main_window.is_exit_app_granted()) {

				uint64_t tick_time = SDL_GetTicks();	// current time

				deltaDraw = tick_time - last_draw_time;
				delta = tick_time - last_logic_time;
				int32_t mw_y{ 0 };

				SDL_PumpEvents();

				if (SDL_PollEvent(&e) != 0) {
					ImGui_ImplSDL3_ProcessEvent(&e);

					if (e.type == SDL_EVENT_QUIT) {
						l_wconf.saveWindowConfig(l_window, SDL_WIN_FILENAME);
						l_main_window.request_exit_app();
					}
				}

				if (delta != 0) {
					uint64_t realDelta = std::min(delta, static_cast<uint64_t>(5));
					SDL_GetWindowSize(l_window, &l_w, &l_h);

					last_logic_time = tick_time;
				}

				if (deltaDraw >= 25) { // capped frame rate of ~40 is ok
					l_main_window.draw(l_rnd);
					last_draw_time = SDL_GetTicks();

					//Update screen
					SDL_RenderPresent(l_rnd);
				}

				SDL_Delay(1);

			}
		}


	}

	// Destroy window
	SDL_DestroyWindow(l_window);

	// Quit SDL subsystems
	SDL_Quit();

	return 0;

}
catch (const std::runtime_error& p_ex) {
	std::cerr << p_ex.what();
	return 1;
}
catch (const std::exception& p_ex) {
	std::cerr << p_ex.what();
	return 1;
}
catch (...) {
	std::cerr << "Unknown runtime error";
	return 1;
}
