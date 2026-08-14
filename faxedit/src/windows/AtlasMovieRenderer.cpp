#include "AtlasMovieRenderer.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace fe::atlas_movie {

	namespace {

		void write_nes_pixel(SDL_Surface* p_surface,
			const SDL_Palette* p_nes_palette, int p_x, int p_y, byte p_index) {
			const auto color{ p_nes_palette->colors[p_index & 0x3f] };
			SDL_WriteSurfacePixel(p_surface, p_x, p_y,
				color.r, color.g, color.b, color.a);
		}

		void draw_tile(SDL_Surface* p_surface, const SDL_Palette* p_nes_palette,
			const klib::NES_tile& p_tile, const std::array<byte, 4>& p_palette,
			int p_x, int p_y, bool p_transparent = false,
			bool p_hflip = false, bool p_vflip = false) {
			for (int y{ 0 }; y < 8; ++y)
				for (int x{ 0 }; x < 8; ++x) {
					const byte value{ p_tile.get_color(
						p_hflip ? 7 - x : x, p_vflip ? 7 - y : y) };
					if (p_transparent && value == 0) continue;
					if (p_x + x >= 0 && p_x + x < p_surface->w
						&& p_y + y >= 0 && p_y + y < p_surface->h)
						write_nes_pixel(p_surface, p_nes_palette,
							p_x + x, p_y + y, p_palette[value]);
				}
		}

		SDL_Texture* surface_texture(SDL_Renderer* p_renderer,
			SDL_Surface* p_surface) {
			auto* result{ SDL_CreateTextureFromSurface(p_renderer, p_surface) };
			SDL_DestroySurface(p_surface);
			if (result) SDL_SetTextureScaleMode(result, SDL_SCALEMODE_NEAREST);
			return result;
		}

	}

	SDL_Texture* render_frame_texture(SDL_Renderer* p_renderer,
		const SDL_Palette* p_nes_palette, const SpriteAnimationFrame& p_frame,
		const std::vector<klib::NES_tile>& p_tiles,
		const std::vector<byte>& p_palette) {
		const int width{ std::max(8, static_cast<int>(p_frame.w() * 8)) };
		const int height{ std::max(8, static_cast<int>(p_frame.h() * 8)) };
		auto* surface{ SDL_CreateSurface(width, height, SDL_PIXELFORMAT_ABGR8888) };
		SDL_FillSurfaceRect(surface, nullptr, SDL_MapRGBA(
			SDL_GetPixelFormatDetails(surface->format), nullptr, 0, 0, 0, 0));
		for (std::size_t y{ 0 }; y < p_frame.h(); ++y)
			for (std::size_t x{ 0 }; x < p_frame.w(); ++x) {
				const auto& cell{ p_frame.tilemap[y][x] };
				if (!cell || cell->index >= p_tiles.size()) continue;
				std::array<byte, 4> palette{};
				for (std::size_t c{ 0 }; c < 4; ++c)
					palette[c] = p_palette[cell->sub_palette * 4 + c];
				draw_tile(surface, p_nes_palette, p_tiles[cell->index], palette,
					static_cast<int>(x * 8), static_cast<int>(y * 8), true,
					cell->h_flip, cell->v_flip);
			}
		auto* result{ surface_texture(p_renderer, surface) };
		if (result) SDL_SetTextureBlendMode(result, SDL_BLENDMODE_BLEND);
		return result;
	}

	SDL_Texture* render_room_texture(SDL_Renderer* p_renderer,
		const SDL_Palette* p_nes_palette, const ImportedRoom& p_room) {
		const auto tiles{ decode_chr(p_room.chr) };
		auto* surface{ SDL_CreateSurface(256, 240, SDL_PIXELFORMAT_ABGR8888) };
		SDL_FillSurfaceRect(surface, nullptr, SDL_MapRGB(
			SDL_GetPixelFormatDetails(surface->format), nullptr, 0, 0, 0));
		for (std::size_t index{ 0 }; index < 960; ++index) {
			const std::size_t row{ index / 32 }, column{ index % 32 };
			const byte raw{ p_room.nametable[index] };
			const std::size_t tile{ static_cast<std::size_t>(
				raw >= 0x80 ? raw - 0x80 : raw) };
			if (tile >= tiles.size()) continue;
			const byte attribute{ p_room.nametable[
				960 + (row / 4) * 8 + column / 4] };
			const byte quadrant{ static_cast<byte>(
				((row % 4) / 2) * 2 + ((column % 4) / 2)) };
			const byte subpalette{ static_cast<byte>(
				(attribute >> (quadrant * 2)) & 3) };
			std::array<byte, 4> colors{};
			for (std::size_t c{ 0 }; c < 4; ++c)
				colors[c] = p_room.palette[subpalette * 4 + c];
			colors[0] = p_room.palette[0];
			draw_tile(surface, p_nes_palette, tiles[tile], colors,
				static_cast<int>(column * 8), static_cast<int>(row * 8));
		}
		return surface_texture(p_renderer, surface);
	}

	SDL_Texture* render_movie_texture(SDL_Renderer* p_renderer,
		const SDL_Palette* p_nes_palette, const std::vector<byte>& p_rom,
		const AtlasMovie& p_movie, const PreviewState& p_state,
		const std::vector<SpriteAnimationFrame>& p_frames) {
		const auto nametable{ resolved_asset_bytes(
			p_rom, p_movie, AtlasMovieAssetKind::Nametable) };
		auto palette{ resolved_asset_bytes(
			p_rom, p_movie, AtlasMovieAssetKind::Palette) };
		const auto background_tiles{ decode_chr(resolved_asset_bytes(
			p_rom, p_movie, AtlasMovieAssetKind::BackgroundChr)) };
		const auto sprite_tiles{ movie_sprite_tiles(p_rom, p_movie) };
		if (nametable.size() < 1024 || palette.size() < 32)
			throw std::runtime_error(
				"Real preview requires a 1024-byte nametable and 32-byte palette");

		if (p_state.phase < p_movie.phases.size()) {
			const auto& phase{ p_movie.phases[p_state.phase] };
			if (phase.effect == AtlasMovieEffect::PaletteFade
				&& phase.effect_period) {
				const std::size_t steps{ p_state.effect_calls / phase.effect_period };
				for (auto& color : palette)
					for (std::size_t step{ 0 }; step < steps; ++step)
						color = color >= phase.effect_subtract
							? std::max<byte>(static_cast<byte>(
								color - phase.effect_subtract), phase.effect_floor)
							: phase.effect_floor;
			}
		}

		auto* surface{ SDL_CreateSurface(256, 240, SDL_PIXELFORMAT_ABGR8888) };
		for (std::size_t index{ 0 }; index < 960; ++index) {
			const std::size_t row{ index / 32 }, column{ index % 32 };
			const byte raw_tile{ nametable[index] };
			const auto tile{ movie_background_tile(p_movie, raw_tile) };
			if (!tile || *tile >= background_tiles.size()) continue;
			const byte attribute{ nametable[960 + (row / 4) * 8 + column / 4] };
			const std::size_t quadrant{ ((row % 4) / 2) * 2
				+ ((column % 4) / 2) };
			const byte subpalette{ static_cast<byte>(
				(attribute >> (quadrant * 2)) & 3) };
			std::array<byte, 4> colors{};
			for (std::size_t c{ 0 }; c < 4; ++c)
				colors[c] = palette[subpalette * 4 + c];
			colors[0] = palette[0];
			draw_tile(surface, p_nes_palette, background_tiles[*tile], colors,
				static_cast<int>(column * 8), static_cast<int>(row * 8));
		}

		const byte draw_mask{ static_cast<byte>(
			p_state.phase < p_movie.phases.size()
				? p_movie.phases[p_state.phase].draw_mask : 0) };
		for (std::size_t i{ 0 }; i < p_state.tracks.size(); ++i) {
			if (!(draw_mask & (1u << i)) || !p_state.tracks[i].visible) continue;
			const auto& actor{ p_state.tracks[i] };
			if (actor.frame >= p_frames.size()) continue;
			const auto& frame{ p_frames[actor.frame] };
			for (std::size_t y{ 0 }; y < frame.h(); ++y)
				for (std::size_t x{ 0 }; x < frame.w(); ++x) {
					const auto& cell{ frame.tilemap[y][x] };
					if (!cell || cell->index >= sprite_tiles.size()) continue;
					std::array<byte, 4> colors{};
					for (std::size_t c{ 0 }; c < 4; ++c)
						colors[c] = palette[16 + cell->sub_palette * 4 + c];
					draw_tile(surface, p_nes_palette, sprite_tiles[cell->index],
						colors, actor.x + frame.offset_x + static_cast<int>(x * 8),
						actor.y + frame.offset_y + static_cast<int>(y * 8), true,
						cell->h_flip, cell->v_flip);
				}
		}
		return surface_texture(p_renderer, surface);
	}

}
