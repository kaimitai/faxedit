#include <fstream>
#include "SDL_WindowConfig.h"

klib::WindowConfig::WindowConfig(void) {
	set_defaults();
}

void klib::WindowConfig::set_defaults(void) {
	x = SDL_WINDOWPOS_CENTERED;
	y = SDL_WINDOWPOS_CENTERED;
	w = 1280;
	h = 720;
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

bool klib::WindowConfig::isVisibleOnAnyDisplay(void) const {
	int count = 0;
	SDL_DisplayID* displays = SDL_GetDisplays(&count);
	if (!displays)
		return false;

	// Compute window center
	const int cx = x + w / 2;
	const int cy = y + h / 2;

	bool visible = false;

	for (int i = 0; i < count; ++i) {
		SDL_Rect bounds;
		if (SDL_GetDisplayBounds(displays[i], &bounds)) {

			const bool inside =
				cx >= bounds.x &&
				cx < bounds.x + bounds.w &&
				cy >= bounds.y &&
				cy < bounds.y + bounds.h;

			if (inside) {
				visible = true;
				break;
			}
		}
	}

	SDL_free(displays);
	return visible;
}
