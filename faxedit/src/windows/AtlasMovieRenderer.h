#ifndef FE_ATLASMOVIERENDERER_H
#define FE_ATLASMOVIERENDERER_H

#include "fe/AtlasMovieAssets.h"
#include "fe/AtlasMoviePreview.h"
#include <SDL3/SDL.h>

namespace fe::atlas_movie {

	SDL_Texture* render_frame_texture(SDL_Renderer* p_renderer,
		const SDL_Palette* p_nes_palette, const SpriteAnimationFrame& p_frame,
		const std::vector<klib::NES_tile>& p_tiles,
		const std::vector<byte>& p_palette);
	SDL_Texture* render_room_texture(SDL_Renderer* p_renderer,
		const SDL_Palette* p_nes_palette, const ImportedRoom& p_room);
	SDL_Texture* render_movie_texture(SDL_Renderer* p_renderer,
		const SDL_Palette* p_nes_palette, const std::vector<byte>& p_rom,
		const AtlasMovie& p_movie, const PreviewState& p_state,
		const std::vector<SpriteAnimationFrame>& p_frames);

}

#endif
