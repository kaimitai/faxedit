#include "gfx.h"
#define STB_IMAGE_IMPLEMENTATION
#include "./../common/stb_image.h"
#include <stdexcept>

constexpr int TILEMAP_SCALE{ 1 };

fe::gfx::gfx(SDL_Renderer* p_rnd) :
	m_nes_palette{ SDL_CreatePalette(256) },
	m_atlas{ nullptr },
	m_metatile_gfx{ std::vector<SDL_Texture*>(256, nullptr) },
	HOT_PINK_TRANSPARENT{ 0xff69b400 }
{
	SDL_SetTextureBlendMode(m_screen_texture, SDL_BLENDMODE_NONE); // if no alpha blending
	SDL_SetTextureScaleMode(m_screen_texture, SDL_SCALEMODE_NEAREST);

	m_screen_texture = SDL_CreateTexture(p_rnd, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, TILEMAP_SCALE * 16 * 16, TILEMAP_SCALE * 13 * 16);

	// generate NES palette
	SDL_Color out_palette[256] = {};
	for (std::size_t i = 0; i < NES_PALETTE.size(); ++i) {
		out_palette[i] = { NES_PALETTE[i][0], NES_PALETTE[i][1], NES_PALETTE[i][2], 255 };
	}
	SDL_SetPaletteColors(m_nes_palette, out_palette, 0, 256);
}

fe::gfx::~gfx(void) {
	if (m_nes_palette != nullptr)
		SDL_DestroyPalette(m_nes_palette);

	delete_texture(m_atlas);
	delete_texture(m_screen_texture);
	for (auto& txt : m_metatile_gfx)
		delete_texture(txt);
	for (auto& kv : m_sprite_gfx)
		for (auto& txt : kv.second)
			delete_texture(txt);
	for (auto& txt : m_icon_overlays)
		delete_texture(txt);
}

void fe::gfx::delete_texture(SDL_Texture* p_txt) {
	if (p_txt != nullptr)
		SDL_DestroyTexture(p_txt);
}

void fe::gfx::generate_mt_texture(SDL_Renderer* p_rnd, const std::vector<std::vector<byte>>& p_mt_def,
	std::size_t p_idx, std::size_t p_sub_palette_no) {
	SDL_Texture* metatile = SDL_CreateTexture(
		p_rnd,
		SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET,
		16, 16);

	SDL_SetTextureBlendMode(metatile, SDL_BLENDMODE_BLEND);
	SDL_SetRenderTarget(p_rnd, metatile);
	SDL_SetRenderDrawColor(p_rnd, 0, 0, 0, 255); // Transparent background
	SDL_RenderClear(p_rnd);

	// Draw the 4 tiles from the atlas
	for (int col = 0; col < 2; ++col) {
		for (int row = 0; row < 2; ++row) {
			int tileIndex = p_mt_def[col][row];
			SDL_FRect src = {
				static_cast<float>(tileIndex * 8),
				static_cast<float>(p_sub_palette_no * 8),
				8.0f, 8.0f
			};
			SDL_FRect dst = {
				static_cast<float>(row * 8),
				static_cast<float>(col * 8),
				8.0f, 8.0f
			};
			SDL_RenderTexture(p_rnd, m_atlas, &src, &dst);
		}
	}

	SDL_SetRenderTarget(p_rnd, nullptr);

	// Store the texture
	delete_texture(m_metatile_gfx[p_idx]);
	m_metatile_gfx[p_idx] = metatile;
}

void fe::gfx::generate_icon_overlays(SDL_Renderer* p_rnd) {
	int width, height, channels;
	unsigned char* rgbaPixels = stbi_load("./assets/icon_overlays.png", &width, &height, &channels, 4);
	if (!rgbaPixels) {
		throw std::runtime_error("Could not make icon overlays");
	}

	unsigned char transp_r{ (HOT_PINK_TRANSPARENT >> 24) & 0xff };
	unsigned char transp_g{ (HOT_PINK_TRANSPARENT >> 16) & 0xff };
	unsigned char transp_b{ (HOT_PINK_TRANSPARENT >> 8) & 0xff };

	for (int i = 0; i < width * height; ++i) {
		for (int i = 0; i < width * height; ++i) {
			int idx = i * 4;
			unsigned char r = rgbaPixels[idx + 0];
			unsigned char g = rgbaPixels[idx + 1];
			unsigned char b = rgbaPixels[idx + 2];

			if (r == transp_r && g == transp_g && b == transp_b) {
				rgbaPixels[idx + 3] = 0; // make transparent
			}
		}
	}

	SDL_Surface* fullSurface = SDL_CreateSurfaceFrom(
		width, height,
		SDL_PIXELFORMAT_RGBA32, // correct first argument
		rgbaPixels,              // pixel data
		width * 4                  // pitch (bytes per row)
	);
	if (!fullSurface)
		throw std::runtime_error("bad boi!");

	for (int i = 0; i < width / 16; ++i) {
		SDL_Rect srcRect = { i * 16, 0, 16, 16 };
		SDL_Surface* iconSurface = SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32);
		SDL_BlitSurface(fullSurface, &srcRect, iconSurface, nullptr);

		SDL_Texture* iconTex = SDL_CreateTextureFromSurface(p_rnd, iconSurface);
		m_icon_overlays.push_back(iconTex);

		SDL_DestroySurface(iconSurface);
	}

	stbi_image_free(rgbaPixels);
	SDL_DestroySurface(fullSurface);
}

void fe::gfx::generate_atlas(SDL_Renderer* p_rnd,
	const std::vector<klib::NES_tile>& p_tiles, const std::vector<byte>& p_palette) {

	auto l_srf{ create_sdl_surface(8 * static_cast<int>(p_tiles.size()),
		8 * static_cast<int>(p_palette.size() / 4)) }; // each palette is 4 bytes long, but each NES tile is 8 pixels high

	// draw all tiles onto the surface, once for each sub-palette
	for (int p{ 0 }; p < static_cast<int>(p_palette.size()); p += 4)
		for (int t{ 0 }; t < static_cast<int>(p_tiles.size()); ++t) {
			// draw pixels

			const auto& tile = p_tiles[t];

			for (int y = 0; y < 8; ++y) {
				for (int x = 0; x < 8; ++x) {
					byte palette_index = tile.get_color(x, y); // 0–3
					byte nes_color_index = p_palette[static_cast<byte>(p) + palette_index]; // NES color index from sub-palette

					int draw_x = t * 8 + x;
					int draw_y = (p / 4) * 8 + y;

					put_nes_pixel(l_srf, draw_x, draw_y, nes_color_index);
				}
			}
		}

	if (m_atlas != nullptr)
		SDL_DestroyTexture(m_atlas);

	m_atlas = surface_to_texture(p_rnd, l_srf);
}

SDL_Texture* fe::gfx::get_atlas(void) const {
	return m_atlas;
}

SDL_Texture* fe::gfx::get_screen_texture(void) const {
	return m_screen_texture;
}

SDL_Texture* fe::gfx::get_metatile_texture(std::size_t p_mt_no) const {
	return m_metatile_gfx.at(p_mt_no);
}

std::size_t fe::gfx::get_anim_frame_count(std::size_t p_sprite_no) const {
	auto iter{ m_sprite_gfx.find(p_sprite_no) };
	if (iter == end(m_sprite_gfx))
		return 0;
	else
		return iter->second.size();
}

void fe::gfx::draw_nes_tile_on_surface(SDL_Surface* p_srf, int dst_x, int dst_y,
	const klib::NES_tile& tile, const std::vector<byte>& p_palette,
	bool p_transparent, bool h_flip, bool v_flip) const {

	for (int y = 0; y < 8; ++y) {
		for (int x = 0; x < 8; ++x) {
			int src_x = h_flip ? 7 - x : x;
			int src_y = v_flip ? 7 - y : y;

			byte color = tile.get_color(src_x, src_y);
			byte palette_index = p_palette.at(color);

			put_nes_pixel(p_srf, dst_x + x, dst_y + y, palette_index,
				p_transparent && (color == 0));
		}
	}

}

SDL_Surface* fe::gfx::create_sdl_surface(int p_w, int p_h, bool p_transparent) const {
	SDL_Surface* l_bmp = SDL_CreateSurface(p_w, p_h, SDL_PIXELFORMAT_ABGR8888);

	if (p_transparent) {
		SDL_FillSurfaceRect(l_bmp, nullptr, HOT_PINK_TRANSPARENT);
		SDL_SetSurfaceColorKey(l_bmp, true, HOT_PINK_TRANSPARENT);
	}

	return l_bmp;
}

void fe::gfx::put_nes_pixel(SDL_Surface* srf, int x, int y, byte p_palette_index,
	bool p_transparent) const {
	SDL_Color l_col{ m_nes_palette->colors[p_palette_index] };

	if (!p_transparent)
		SDL_WriteSurfacePixel(srf, x, y, l_col.r, l_col.g, l_col.b, l_col.a);
	else
		SDL_WriteSurfacePixel(srf, x, y,
			(HOT_PINK_TRANSPARENT >> 24) & 0xff,
			(HOT_PINK_TRANSPARENT >> 16) & 0xff,
			(HOT_PINK_TRANSPARENT >> 8) & 0xff,
			HOT_PINK_TRANSPARENT & 0xff
		);
}

SDL_Texture* fe::gfx::surface_to_texture(SDL_Renderer* p_rnd, SDL_Surface* p_srf, bool p_destroy_surface) {
	SDL_Texture* result = SDL_CreateTextureFromSurface(p_rnd, p_srf);

	if (p_destroy_surface)
		SDL_DestroySurface(p_srf);

	return(result);
}

// blitting
void fe::gfx::blit(SDL_Renderer* p_rnd, SDL_Texture* p_texture, int p_x, int p_y) const {

	float w, h;
	SDL_GetTextureSize(p_texture, &w, &h);

	SDL_FRect dst_rect{ static_cast<float>(p_x), static_cast<float>(p_y), w, h };

	SDL_RenderTexture(p_rnd, p_texture, nullptr, &dst_rect);
}

void fe::gfx::blit_to_screen(SDL_Renderer* renderer, int tile_no, int sub_palette_no, int x, int y) const {
	if (!m_atlas || !m_screen_texture) return;

	SDL_FRect src_rect = {
		static_cast<float>(tile_no * 8),
		static_cast<float>(sub_palette_no * 8),
		8.0f,
		8.0f
	};

	SDL_FRect dst_rect = {
		static_cast<float>(TILEMAP_SCALE * x * 8),
		static_cast<float>(TILEMAP_SCALE * y * 8),
		TILEMAP_SCALE * 8.0f,
		TILEMAP_SCALE * 8.0f
	};

	SDL_SetRenderTarget(renderer, m_screen_texture);
	SDL_RenderTexture(renderer, m_atlas, &src_rect, &dst_rect);
	SDL_SetRenderTarget(renderer, nullptr);
}

void fe::gfx::draw_sprite_on_screen(SDL_Renderer* p_rnd, std::size_t p_sprite_no,
	std::size_t p_frame_no,
	int x, int y) const {
	auto iter{ m_sprite_gfx.find(p_sprite_no) };
	if (iter == end(m_sprite_gfx))
		return;
	else {
		float w, h;
		SDL_GetTextureSize(iter->second[p_frame_no], &w, &h);
		SDL_FRect dst_rect = { static_cast<float>(x), static_cast<float>(y),
			w, h };

		SDL_SetRenderTarget(p_rnd, m_screen_texture);
		SDL_RenderTexture(p_rnd, iter->second[p_frame_no], nullptr, &dst_rect);
		SDL_SetRenderTarget(p_rnd, nullptr);
	}
}

void fe::gfx::draw_icon_overlay(SDL_Renderer* p_rnd, int x, int y, byte block_property) const {

	float w, h;
	SDL_GetTextureSize(m_icon_overlays[block_property], &w, &h);
	SDL_FRect dst_rect = { 16.0f * static_cast<float>(x),
		16.0f * static_cast<float>(y),
	w, h };

	SDL_SetRenderTarget(p_rnd, m_screen_texture);
	SDL_RenderTexture(p_rnd, m_icon_overlays[block_property],
		nullptr,
		&dst_rect);
	SDL_SetRenderTarget(p_rnd, nullptr);
}

void fe::gfx::draw_pixel_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color, int pixel_x, int pixel_y, int pixel_w, int pixel_h) const {
	SDL_SetRenderTarget(p_rnd, m_screen_texture);
	SDL_SetRenderDrawColor(p_rnd, p_color.r, p_color.g, p_color.b, p_color.a);

	SDL_FRect l_rect(static_cast<float>(TILEMAP_SCALE * pixel_x),
		static_cast<float>(TILEMAP_SCALE * pixel_y),
		static_cast<float>(pixel_w * TILEMAP_SCALE),
		static_cast<float>(pixel_h * TILEMAP_SCALE));

	SDL_RenderRect(p_rnd, &l_rect);
	SDL_SetRenderTarget(p_rnd, nullptr);
}

void fe::gfx::draw_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color,
	int p_x, int p_y, int p_w, int p_h) const {
	draw_pixel_rect_on_screen(p_rnd, p_color, p_x * 16, p_y * 16, p_w * 16, p_h * 16);
}

void fe::gfx::set_app_icon(SDL_Window* p_window, const unsigned char* p_pixels) {
	constexpr uint32_t lc_palette[4] = {
		0x00000000, // transparent
		0xFFB0B0B0, // lighter medium gray
		0xFF006400, // dark green
		0xFF800080  // purple
	};

	SDL_Surface* l_icon{ SDL_CreateSurface(16, 16, SDL_PIXELFORMAT_RGBA32) };
	if (!l_icon)
		return;
	if (!SDL_LockSurface(l_icon)) {
		SDL_DestroySurface(l_icon);
		return;
	}

	uint32_t* pixels = static_cast<uint32_t*>(l_icon->pixels);

	for (int i = 0; i < 64; ++i) {
		unsigned char byte = p_pixels[i];
		for (int j = 0; j < 4; ++j) {
			int pixel_index = i * 4 + j;
			int x = pixel_index % 16;
			int y = pixel_index / 16;

			uint8_t index = (byte >> ((3 - j) * 2)) & 0x03;
			pixels[y * 16 + x] = lc_palette[index];
		}
	}

	SDL_UnlockSurface(l_icon);
	SDL_SetWindowIcon(p_window, l_icon);
	SDL_DestroySurface(l_icon);
}

void fe::gfx::gen_sprites(SDL_Renderer* p_rnd,
	const std::map<std::size_t, fe::Sprite_gfx_definiton>& p_defs) {
	for (auto& kv : m_sprite_gfx)
		for (auto& txt : kv.second)
			delete_texture(txt);
	m_sprite_gfx.clear();

	for (const auto& kv : p_defs) {
		const auto& l_sprpal{ kv.second.m_sprite_palette };
		const auto& tiles{ kv.second.m_nes_tiles };

		for (const auto& frame : kv.second.m_frames) {
			if (frame.m_disabled)
				continue;

			std::size_t w{ static_cast<std::size_t>(frame.m_w) };
			std::size_t h{ static_cast<std::size_t>(frame.m_h) };

			auto srf{ create_sdl_surface(static_cast<int>(8 * w),
				static_cast<int>(8 * h), true) };

			for (int y{ 0 }; y < frame.m_tilemap.size(); ++y)
				for (int x{ 0 }; x < frame.m_tilemap.at(y).size(); ++x) {
					const auto& opttile{ frame.m_tilemap.at(y).at(x) };

					if (opttile.has_value() &&
						opttile.value().first < tiles.size()) {
						byte tile_ctrl{ opttile.value().second };

						draw_nes_tile_on_surface(
							srf, static_cast<int>(8 * x),
							static_cast<int>(8 * y),
							tiles[opttile.value().first],
							l_sprpal.at(tile_ctrl & 0b11),
							true,
							tile_ctrl & 0x40,
							tile_ctrl & 0x80);
					}
				}

			m_sprite_gfx[kv.first].push_back(surface_to_texture(p_rnd, srf));
		}
	}
}

const std::vector<std::vector<byte>> fe::gfx::NES_PALETTE = {
	{84, 84, 84},   {0, 30, 116},   {8, 16, 144},  {48, 0, 136},
	{68, 0, 100},   {92, 0, 48},    {84, 4, 0},    {60, 24, 0},
	{32, 42, 0},    {8, 58, 0},     {0, 64, 0},    {0, 60, 0},
	{0, 50, 60},    {0, 0, 0},      {0, 0, 0},     {0, 0, 0},

	{152, 150, 152}, {8, 76, 196},  {48, 50, 236}, {92, 30, 228},
	{136, 20, 176},  {160, 20, 100},{152, 34, 32}, {120, 60, 0},
	{84, 90, 0},     {40, 114, 0},  {8, 124, 0},   {0, 118, 40},
	{0, 102, 120},   {0, 0, 0},     {0, 0, 0},     {0, 0, 0},

	{236, 238, 236}, {76, 154, 236}, {120, 124, 236}, {176, 98, 236},
	{228, 84, 236},  {236, 88, 180}, {236, 106, 100}, {212, 136, 32},
	{160, 170, 0},   {116, 196, 0},  {76, 208, 32},   {56, 204, 108},
	{56, 180, 204},  {60, 60, 60},   {0, 0, 0},      {0, 0, 0},

	{236, 238, 236}, {168, 204, 236}, {188, 188, 236}, {212, 178, 236},
	{236, 174, 236}, {236, 174, 212}, {236, 180, 176}, {228, 196, 144},
	{204, 210, 120}, {180, 222, 120}, {168, 226, 144}, {152, 226, 180},
	{160, 214, 228}, {160, 162, 160}, {0, 0, 0},       {0, 0, 0}
};
