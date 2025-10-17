#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <stdexcept>
#include "./common/imgui/imgui.h"
#include "./common/imgui/imgui_impl_sdl3.h"
#include "./common/imgui/imgui_impl_sdlrenderer3.h"
#include "./common/klib/Kfile.h"
#include "./fe/Game.h"
#include "./windows/MainWindow.h"

int main(int argc, char** argv) try {

	SDL_Window* l_window{ nullptr };
	SDL_Renderer* l_rnd{ nullptr };
	bool l_exit{ false };

	if (!SDL_Init(SDL_INIT_VIDEO))
		throw std::runtime_error(SDL_GetError());
	else {
		// create game object
		fe::Game l_game(klib::file::read_file_as_bytes("c:/temp/faxanadu-out.nes"));
		// fe::Game l_game(klib::file::read_file_as_bytes("c:/temp/Faxanadu (USA) (Rev A).nes"));

		// Event handler
		SDL_Event e;

		l_window = SDL_CreateWindow("Echoes of Eolis", 1024, 768, SDL_WINDOW_RESIZABLE);
		if (l_window == nullptr)
			throw std::runtime_error(SDL_GetError());
		else {
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
			//ImGui::StyleColorsLight();

			// Setup Platform/Renderer backends
			ImGui_ImplSDL3_InitForSDLRenderer(l_window, l_rnd);
			ImGui_ImplSDLRenderer3_Init(l_rnd);
			std::string l_ini_filename{ "faxedit_windows.ini" };
			ImGui::GetIO().IniFilename = l_ini_filename.c_str();
			ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

			fe::MainWindow l_main_window(l_rnd);
			l_main_window.generate_textures(l_rnd, l_game);
			// main_window.set_application_icon(l_window);

			// input handler
			// klib::User_input input;
			int mouse_wheel_y{ 0 };
			bool mw_used{ false };

			uint64_t last_logic_time = SDL_GetTicks() - 1;
			uint64_t last_draw_time = SDL_GetTicks() - 17;

			uint64_t delta = 1;
			uint64_t deltaDraw = 17;

			int l_w{ 1024 }, l_h{ 768 };

			while (!l_exit) {

				uint64_t tick_time = SDL_GetTicks();	// current time

				deltaDraw = tick_time - last_draw_time;
				delta = tick_time - last_logic_time;
				int32_t mw_y{ 0 };

				mw_used = false;
				SDL_PumpEvents();

				if (SDL_PollEvent(&e) != 0) {
					ImGui_ImplSDL3_ProcessEvent(&e);

					if (e.type == SDL_EVENT_QUIT)
						l_exit = true;
					else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
						mw_used = true;
						mouse_wheel_y = static_cast<int>(e.wheel.y);
					}
				}

				if (delta != 0) {
					uint64_t realDelta = std::min(delta, static_cast<uint64_t>(5));
					SDL_GetWindowSize(l_window, &l_w, &l_h);

					// input.move(realDelta, mw_used ? mouse_wheel_y : 0);
					//main_window.move(realDelta, input, config, l_w, l_h);
					// main_window.move(realDelta, input, l_config, l_h);

					last_logic_time = tick_time;
				}

				if (deltaDraw >= 25) { // capped frame rate of ~40 is ok
					l_main_window.draw(l_rnd, l_game);
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
catch (...) {
	return 1;
}
