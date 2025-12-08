#include <fstream>
#include "SDL_WindowConfig.h"

klib::WindowConfig::WindowConfig(void) :
	x{ SDL_WINDOWPOS_CENTERED },
	y{ SDL_WINDOWPOS_CENTERED },
	w{ 1280 },
	h{ 720 },
	maximized{ false }
{
}

void klib::WindowConfig::loadConfig(const std::string& p_filepath) {
	std::ifstream in(p_filepath);
	if (in) {
		in >> x >> y >> w >> h >> maximized;
		in.close();
	}
}

void klib::WindowConfig::saveWindowConfig(SDL_Window* window,
	const std::string& p_filepath) {

	SDL_GetWindowPosition(window, &x, &y);
	SDL_GetWindowSize(window, &w, &h);
	maximized = SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED;

	std::ofstream out(p_filepath);

	if (out) {
		out << x << " " << y << " "
			<< w << " " << h << " "
			<< maximized << "\n";

		out.close();
	}
}
