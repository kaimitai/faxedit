#include <SDL3/SDL.h>
#include <string>

namespace klib {

	struct WindowConfig {
		int x, y, w, h;
		bool maximized;

		WindowConfig(void);
		void saveWindowConfig(SDL_Window* window, const std::string& p_filepath);
		void loadConfig(const std::string& p_filepath);
	};

}
