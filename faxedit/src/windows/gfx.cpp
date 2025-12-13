#include "gfx.h"
#define STB_IMAGE_IMPLEMENTATION
#include "./../common/stb_image.h"
#include "./../common/klib/Kfile.h"
#include <array>
#include <format>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

constexpr int TILEMAP_SCALE{ 1 };

// embed the png directly in the code
// currently just my own placeholder "art"
constexpr unsigned char OVERLAY_ICONS_PNG[]{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x10, 0x08, 0x06, 0x00, 0x00, 0x00, 0xde, 0x7d, 0x20, 0xa1, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xae, 0xce, 0x1c, 0xe9, 0x00, 0x00, 0x00, 0x04, 0x67, 0x41, 0x4d, 0x41, 0x00, 0x00, 0xb1, 0x8f, 0x0b, 0xfc, 0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0e, 0xc2, 0x00, 0x00, 0x0e, 0xc2, 0x01, 0x15, 0x28, 0x4a, 0x80, 0x00, 0x00, 0x04, 0x25, 0x49, 0x44, 0x41, 0x54, 0x78, 0x5e, 0xed, 0x9a, 0xbd, 0xae, 0xd4, 0x30, 0x10, 0x85, 0x6f, 0xe8, 0xa1, 0xa1, 0x02, 0x41, 0x83, 0x90, 0xe8, 0x68, 0x2f, 0x2d, 0xb4, 0x20, 0x21, 0xf1, 0x16, 0x50, 0x20, 0xde, 0x00, 0xf1, 0x06, 0x88, 0x02, 0xde, 0x02, 0x09, 0xe9, 0xd2, 0x42, 0xcb, 0x6d, 0xe9, 0x90, 0x10, 0x0d, 0x08, 0x2a, 0x1a, 0x78, 0x80, 0x90, 0x63, 0x72, 0x96, 0xd9, 0xd9, 0xb1, 0x3d, 0xb6, 0xc3, 0x66, 0xef, 0x6e, 0x3e,
	0x69, 0xb4, 0xf1, 0x5f, 0x32, 0xb6, 0x67, 0xcf, 0x4e, 0xac, 0xed, 0xfa, 0x87, 0x27, 0xfd, 0xd1, 0xc2, 0xc2, 0xc2, 0x42, 0x05, 0x8b, 0x80, 0xec, 0x01, 0xdd, 0xab, 0x7b, 0xe3, 0x55, 0x19, 0xc3, 0xde, 0x87, 0xcf, 0xd6, 0xf1, 0x73, 0x73, 0xe8, 0xfe, 0xcf, 0x39, 0xff, 0x45, 0x40, 0xf6, 0x00, 0x04, 0x50, 0x7f, 0x7a, 0x61, 0x2c, 0xf9, 0xe8, 0x8e, 0x7f, 0xad, 0x05, 0xe0, 0x93, 0x17, 0xef, 0xc2, 0xb5, 0x97, 0xe7, 0x8f, 0xef, 0xec, 0xd4, 0x17, 0xf0, 0xed, 0xf7, 0xab, 0x63, 0xc9, 0xc7, 0xdd, 0xcb, 0x5f, 0x77, 0xca, 0xff, 0xd6, 0xfd, 0xeb, 0x5f, 0x9f, 0x86, 0x6b, 0x2f, 0xdd, 0x83, 0xe3, 0x49, 0xe6, 0x7f, 0x6e, 0xfc, 0x9c, 0x04, 0x4c, 0xc4, 0x63, 0xbb, 0x8a, 0xe5, 0x2b, 0xcc, 0x4b, 0xcb, 0x58, 0x49, 0xeb, 0x7d, 0x10, 0x5c, 0xb0, 0x5a, 0x20, 0x0e, 0xb0, 0xb3, 0x0a, 0xc4, 0x01, 0xd6, 0x4a, 0xe9, 0xba, 0x6b, 0x6a, 0xc7, 0xb7, 0xee, 0x1f, 0xc4, 0x01, 0xb6, 0x0d, 0xa2, 0x02, 0x22, 0x83, 0x57, 0x5a, 0x8e, 0x9f, 0x57, 0xae, 0xaf, 0x4c, 0x97, 0x59, 0x17, 0x23, 0xf5, 0x9c, 0x54, 0x9b, 0x85,
	0xec, 0xef, 0x1d, 0x03, 0xfa, 0x61, 0xdf, 0xa4, 0x95, 0xf2, 0xe1, 0xfd, 0x8f, 0x95, 0xd5, 0x00, 0x5f, 0x3f, 0xfe, 0x7e, 0x19, 0xae, 0xe5, 0xa7, 0x77, 0x0e, 0xe1, 0x97, 0x69, 0xf8, 0x35, 0x83, 0xd5, 0x04, 0x21, 0x84, 0x03, 0xd9, 0x08, 0xac, 0x45, 0x44, 0x2c, 0x7f, 0x73, 0x73, 0x40, 0x3b, 0x4d, 0x93, 0x6a, 0x93, 0x40, 0x38, 0x90, 0x8d, 0xc0, 0x5a, 0x44, 0x04, 0xcf, 0x41, 0xbc, 0xe6, 0x9e, 0x17, 0x03, 0xe3, 0x10, 0x3f, 0xa5, 0xe3, 0x5b, 0xf7, 0x2f, 0x64, 0x16, 0x43, 0x36, 0x02, 0x6b, 0x11, 0x11, 0xf8, 0xad, 0xcd, 0x62, 0x43, 0x40, 0xd8, 0xb9, 0xef, 0x7b, 0xd3, 0x52, 0x37, 0x03, 0x17, 0xbf, 0x7d, 0x5e, 0x99, 0x2e, 0xb3, 0x2e, 0xc5, 0xb5, 0x37, 0x7f, 0xef, 0xcd, 0xe7, 0xc8, 0xe7, 0xb1, 0x2d, 0x07, 0xfa, 0xf7, 0x4f, 0x6f, 0xac, 0x19, 0xef, 0xe1, 0xa1, 0x1b, 0xb2, 0x49, 0x5a, 0x0d, 0xb7, 0x6e, 0x5f, 0x0a, 0x76, 0xa8, 0xa4, 0xd6, 0x3a, 0xb7, 0x0f, 0x14, 0x6d, 0xf4, 0x93, 0x06, 0x6a, 0x04, 0xbd, 0x06, 0x3c, 0x8f, 0x3f, 0x76, 0x35, 0x22, 0x82, 0xfe, 0xf4, 0xb5, 0x46, 0x44, 0xe6,
	0x86, 0xfe, 0x6b, 0xb3, 0xe6, 0xb1, 0x26, 0x20, 0x61, 0xe0, 0x28, 0x14, 0x31, 0xa4, 0x90, 0x58, 0xc8, 0x4c, 0x83, 0xd7, 0xb2, 0xec, 0x01, 0x42, 0x61, 0xd9, 0x36, 0xc1, 0xfb, 0x21, 0xad, 0x94, 0x96, 0x0c, 0xa4, 0x95, 0xd2, 0x77, 0x69, 0x4d, 0xe9, 0x59, 0x88, 0x86, 0xc1, 0x17, 0x23, 0x16, 0x88, 0x12, 0x19, 0xb4, 0xd2, 0x3c, 0x94, 0x9e, 0x85, 0x68, 0xe0, 0x9b, 0x8e, 0x53, 0x94, 0x73, 0x3e, 0x13, 0x6b, 0xfe, 0x28, 0x7b, 0xc7, 0xb7, 0xee, 0x5f, 0xe9, 0x59, 0x88, 0xc6, 0xf2, 0x9f, 0x58, 0xf3, 0x58, 0x09, 0x48, 0x18, 0x98, 0x10, 0x0e, 0x4d, 0x4c, 0x44, 0x64, 0xa6, 0xa1, 0x33, 0x0f, 0x7e, 0xe6, 0xf8, 0x72, 0xff, 0xc4, 0x34, 0x0f, 0x61, 0x1e, 0x43, 0xc6, 0xa1, 0x29, 0xce, 0x42, 0x86, 0xbe, 0x25, 0xfd, 0x25, 0x32, 0x03, 0xa9, 0xbd, 0x8f, 0x7e, 0x8d, 0x01, 0xde, 0xfb, 0x20, 0xf5, 0x65, 0x2a, 0x5c, 0x03, 0xcf, 0x40, 0x28, 0x26, 0x2c, 0xe7, 0x5e, 0x69, 0xe0, 0x9f, 0x0c, 0x3e, 0xce, 0x9d, 0x46, 0x72, 0x5f, 0x28, 0x99, 0x01, 0x4a, 0xf3, 0xc2, 0x33, 0x90, 0x52, 0x31, 0xa1, 0x4f, 0x3a,
	0x66, 0x79, 0x9d, 0xf2, 0x19, 0xb0, 0x5d, 0xfa, 0xbb, 0x76, 0x9d, 0x19, 0x4f, 0x5a, 0xf7, 0x8f, 0x67, 0x20, 0xc5, 0x07, 0xab, 0x83, 0x7f, 0xdc, 0x3f, 0xfa, 0x2d, 0x0d, 0xe8, 0xbd, 0x9b, 0xf4, 0x10, 0x15, 0x30, 0xe3, 0xa0, 0xe9, 0x3a, 0x2f, 0x32, 0x03, 0xa8, 0xc9, 0x02, 0xba, 0x67, 0x9f, 0x82, 0xd5, 0x80, 0x45, 0x92, 0x5f, 0x84, 0x52, 0xe4, 0x39, 0x08, 0xcd, 0x1b, 0x3c, 0x31, 0xa4, 0x90, 0x78, 0x60, 0xf0, 0x31, 0x18, 0x69, 0x5e, 0x74, 0x26, 0x92, 0xcb, 0x4c, 0x64, 0xf0, 0x01, 0xae, 0xa1, 0x36, 0x82, 0xeb, 0xd4, 0x9a, 0xb4, 0xee, 0xbf, 0x14, 0x0f, 0x0a, 0x4a, 0x8e, 0xd8, 0xf3, 0x62, 0xf5, 0x9a, 0x58, 0xbf, 0x58, 0x7d, 0x0a, 0x29, 0x1e, 0x35, 0xfb, 0x27, 0xc5, 0x83, 0x82, 0xe2, 0x3d, 0x13, 0x81, 0x58, 0xd0, 0x57, 0xfa, 0x81, 0x32, 0x45, 0x44, 0xb2, 0x12, 0x90, 0xd0, 0xa1, 0xeb, 0xc6, 0x52, 0x1e, 0xf4, 0xb5, 0x16, 0x84, 0x8a, 0x2d, 0x95, 0x5b, 0x97, 0x3d, 0x20, 0xb8, 0xa4, 0x6d, 0x13, 0xa9, 0xb8, 0x53, 0xe1, 0x15, 0x11, 0xac, 0xe9, 0xcd, 0xf3, 0x8f, 0xc6, 0xd2, 0x3f, 0xac,
	0x3a, 0x0f, 0x08, 0x00, 0x5a, 0x0b, 0xb9, 0xec, 0x43, 0x07, 0x18, 0xd7, 0x50, 0x1b, 0xc1, 0xb5, 0x15, 0x3f, 0x64, 0xea, 0xfd, 0x6f, 0x3d, 0x54, 0x9d, 0x9b, 0xd6, 0x3d, 0xe4, 0xc1, 0x6a, 0x29, 0x39, 0xd1, 0x5a, 0xcb, 0x40, 0x28, 0x22, 0x29, 0x21, 0x61, 0x7b, 0x6c, 0xf3, 0xad, 0xac, 0x43, 0xd6, 0x79, 0xb0, 0xce, 0x3f, 0x60, 0x25, 0xf0, 0xf0, 0xb4, 0x16, 0xcc, 0x2f, 0x15, 0xe0, 0x73, 0xe0, 0xf5, 0xc7, 0x0a, 0x34, 0x04, 0x82, 0x37, 0x00, 0xad, 0x6c, 0xc3, 0x73, 0x36, 0x12, 0xe2, 0x47, 0x3c, 0x82, 0x6b, 0x48, 0x23, 0x39, 0xf1, 0xd0, 0x59, 0x0b, 0xcd, 0x8b, 0xf5, 0xea, 0x52, 0xf3, 0x4a, 0x33, 0x17, 0xb1, 0xfd, 0xf3, 0x62, 0x09, 0x45, 0x4d, 0xf6, 0x01, 0xe4, 0xba, 0xa3, 0x5e, 0xee, 0x2f, 0xd8, 0x78, 0x85, 0xe1, 0x66, 0x53, 0x28, 0xb4, 0xb1, 0x3d, 0x86, 0x95, 0x75, 0xc8, 0x3a, 0x0f, 0xd6, 0xf9, 0x07, 0xec, 0x90, 0x40, 0xc6, 0x21, 0xad, 0x04, 0x1d, 0x6c, 0x25, 0xe2, 0x01, 0xac, 0x6c, 0x23, 0x97, 0x81, 0x10, 0x2b, 0xc8, 0x24, 0x39, 0xf1, 0x00, 0xe8, 0x63, 0x99, 0x17, 0x2b, 0xd3,
	0x38, 0x4b, 0x19, 0x88, 0x25, 0x16, 0x25, 0xfb, 0x67, 0x89, 0x85, 0x27, 0xfb, 0xb0, 0xbe, 0xdb, 0x7a, 0xdd, 0x75, 0x9f, 0x9d, 0xfa, 0x27, 0xaa, 0x4c, 0x55, 0x37, 0x26, 0x92, 0x68, 0x93, 0xe4, 0xd2, 0x5d, 0xef, 0xd8, 0x54, 0xbf, 0x18, 0x9e, 0x54, 0xbb, 0xe6, 0xbe, 0x39, 0xf0, 0xdc, 0x54, 0x80, 0x59, 0x02, 0x12, 0xea, 0x46, 0x5f, 0x30, 0xde, 0x93, 0x61, 0x48, 0x20, 0x28, 0xb9, 0xb5, 0xd4, 0xed, 0x56, 0x9d, 0x44, 0xae, 0x9f, 0x35, 0x96, 0x58, 0x6d, 0xa9, 0xec, 0x42, 0x0a, 0x07, 0xfb, 0xa1, 0xae, 0xd6, 0x17, 0x0f, 0x25, 0xe3, 0xd1, 0x37, 0xb7, 0x7f, 0x84, 0xfd, 0xf4, 0xfe, 0xa5, 0x04, 0x02, 0x82, 0xa2, 0xdb, 0x43, 0x9d, 0xd3, 0x2f, 0xf9, 0x2c, 0xcd, 0xf2, 0x57, 0xf6, 0x3d, 0x20, 0x17, 0x80, 0x16, 0xff, 0x5b, 0x40, 0xb6, 0x09, 0xfc, 0x2f, 0x7d, 0x3d, 0xc9, 0x09, 0xc8, 0x36, 0x99, 0x62, 0xff, 0x4a, 0xcf, 0x37, 0xbc, 0x02, 0x42, 0x62, 0x7d, 0x17, 0x01, 0xd9, 0x03, 0xe4, 0x46, 0x97, 0x20, 0x03, 0xb0, 0x86, 0x5c, 0x00, 0x6e, 0x8b, 0x43, 0xf7, 0x7f, 0xce, 0xf9, 0x2f, 0x02, 0xb2,
	0xb0, 0xb0, 0x50, 0xc9, 0xd1, 0xd1, 0x1f, 0xdb, 0x2e, 0x83, 0x73, 0x06, 0xa1, 0x47, 0xe2, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };

fe::gfx::gfx(SDL_Renderer* p_rnd) :
	m_nes_palette{ SDL_CreatePalette(256) },
	m_atlas{ nullptr },
	m_metatile_gfx{ std::vector<SDL_Texture*>(256, nullptr) },
	m_hot_pink{ SDL_Color(0xff, 0x69, 0xbf, 0x00) }
{
	SDL_SetTextureBlendMode(m_screen_texture, SDL_BLENDMODE_NONE); // if no alpha blending
	SDL_SetTextureScaleMode(m_screen_texture, SDL_SCALEMODE_NEAREST);

	m_screen_texture = SDL_CreateTexture(p_rnd, SDL_PIXELFORMAT_ABGR8888,
		SDL_TEXTUREACCESS_TARGET, TILEMAP_SCALE * 16 * 16, TILEMAP_SCALE * 13 * 16);
}

SDL_Color fe::gfx::uint24_to_SDL_Color(std::size_t p_col) const {
	return SDL_Color{
		static_cast<byte>((p_col >> 16) & 0xff),
		static_cast<byte>((p_col >> 8) & 0xff),
		static_cast<byte>((p_col) & 0xff),
		static_cast<byte>(0xff)
	};
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
	for (auto& txt : m_door_req_gfx)
		delete_texture(txt);
	for (auto& kv : m_tilemap_gfx)
		delete_texture(kv.second);
}

void fe::gfx::delete_texture(SDL_Texture* p_txt) {
	if (p_txt != nullptr)
		SDL_DestroyTexture(p_txt);
}

void fe::gfx::set_nes_palette(const std::vector<std::size_t>& p_palette) {
	SDL_Color out_palette[256] = {};
	for (std::size_t i{ 0 }; i < p_palette.size(); ++i) {
		out_palette[i] = uint24_to_SDL_Color(p_palette[i]);
	}

	SDL_SetPaletteColors(m_nes_palette, out_palette, 0, 256);
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
	unsigned char* rgbaPixels = stbi_load_from_memory(OVERLAY_ICONS_PNG,
		sizeof(OVERLAY_ICONS_PNG),
		&width, &height, &channels, 4); // stbi_load("./assets/icon_overlays.png", &width, &height, &channels, 4);
	if (!rgbaPixels) {
		throw std::runtime_error("Could not make icon overlays");
	}

	for (int i = 0; i < width * height; ++i) {
		for (int i = 0; i < width * height; ++i) {
			int idx = i * 4;
			unsigned char r = rgbaPixels[idx + 0];
			unsigned char g = rgbaPixels[idx + 1];
			unsigned char b = rgbaPixels[idx + 2];

			if (r == m_hot_pink.r && g == m_hot_pink.g && b == m_hot_pink.b) {
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

SDL_Surface* fe::gfx::create_sdl_surface(int p_w, int p_h,
	bool p_transparent, bool p_set_no_colorkey) const {
	SDL_Surface* l_bmp = SDL_CreateSurface(p_w, p_h, SDL_PIXELFORMAT_ABGR8888);

	Uint32 l_hotpink_32 = SDL_MapRGBA(SDL_GetPixelFormatDetails(l_bmp->format),
		nullptr,
		m_hot_pink.r, m_hot_pink.g, m_hot_pink.b, m_hot_pink.a);

	if (p_transparent) {
		SDL_FillSurfaceRect(l_bmp, nullptr, l_hotpink_32);
		if (!p_set_no_colorkey)
			SDL_SetSurfaceColorKey(l_bmp, true, l_hotpink_32);
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
			m_hot_pink.r,
			m_hot_pink.g,
			m_hot_pink.b,
			m_hot_pink.a
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
	if (static_cast<std::size_t>(block_property) > m_icon_overlays.size())
		return;

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

void fe::gfx::draw_door_req(SDL_Renderer* p_rnd, int x, int y, byte p_req) const {
	if (p_req >= m_door_req_gfx.size() ||
		m_door_req_gfx[p_req] == nullptr)
		return;
	else {
		float w, h;
		SDL_GetTextureSize(m_door_req_gfx[p_req], &w, &h);
		SDL_FRect dst_rect = { 16.0f * static_cast<float>(x),
			16.0f * static_cast<float>(y),
		w, h };

		SDL_SetRenderTarget(p_rnd, m_screen_texture);
		SDL_RenderTexture(p_rnd, m_door_req_gfx[p_req],
			nullptr,
			&dst_rect);
		SDL_SetRenderTarget(p_rnd, nullptr);
	}
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

void fe::gfx::draw_gridlines_on_screen(SDL_Renderer* p_rnd) const {
	// Set render target to your texture
	SDL_SetRenderTarget(p_rnd, m_screen_texture);

	// Choose grid line color (semi-transparent gray)
	SDL_SetRenderDrawColor(p_rnd, 200, 200, 200, 128);

	// Vertical lines (skip x=0 and x=texW-1)
	for (int x = 16; x < 16 * 16 - 1; x += 16) {
		SDL_RenderLine(p_rnd, static_cast<float>(x),
			0.0f,
			static_cast<float>(x),
			16.0f * 13.0f - 1.0f);
	}

	// Horizontal lines (skip y=0 and y=texH-1)
	for (int y = 16; y < 16 * 13 - 1; y += 16) {
		SDL_RenderLine(p_rnd, 0.0f,
			static_cast<float>(y),
			16.0f * 16.0f - 1.0f,
			static_cast<float>(y));
	}

	// Reset render target back to default (the window)
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
			m_sprite_gfx[kv.first].push_back(
				anim_frame_to_texture(p_rnd, frame, tiles, l_sprpal)
			);
		}
	}
}

void fe::gfx::gen_door_req_gfx(SDL_Renderer* p_rnd,
	const fe::Sprite_gfx_definiton& p_def) {
	const auto& l_sprpal{ p_def.m_sprite_palette };
	const auto& tiles{ p_def.m_nes_tiles };

	m_door_req_gfx.push_back(nullptr); // req "None"
	for (std::size_t i{ 0 }; i < p_def.m_frames.size(); ++i)
		m_door_req_gfx.push_back(
			anim_frame_to_texture(p_rnd, p_def.m_frames[i],
				p_def.m_nes_tiles, p_def.m_sprite_palette)
		);
}

SDL_Texture* fe::gfx::anim_frame_to_texture(SDL_Renderer* p_rnd,
	const fe::AnimationFrame& p_frame,
	const std::vector<klib::NES_tile>& p_tiles,
	const std::vector<std::vector<byte>>& p_palette) {

	std::size_t w{ static_cast<std::size_t>(p_frame.m_w) };
	std::size_t h{ static_cast<std::size_t>(p_frame.m_h) };

	auto srf{ create_sdl_surface(static_cast<int>(8 * w),
		static_cast<int>(8 * h), true) };

	for (int y{ 0 }; y < p_frame.m_tilemap.size(); ++y)
		for (int x{ 0 }; x < p_frame.m_tilemap.at(y).size(); ++x) {
			const auto& opttile{ p_frame.m_tilemap.at(y).at(x) };

			if (opttile.has_value() &&
				opttile.value().first < p_tiles.size()) {
				byte tile_ctrl{ opttile.value().second };

				draw_nes_tile_on_surface(
					srf, static_cast<int>(8 * x),
					static_cast<int>(8 * y),
					p_tiles[opttile.value().first],
					p_palette.at(tile_ctrl & 0b11),
					true,
					tile_ctrl & 0x40,
					tile_ctrl & 0x80);
			}
		}

	return surface_to_texture(p_rnd, srf);
}

SDL_Texture* fe::gfx::get_tileset_txt(std::size_t p_key) const {
	auto iter = m_tilemap_gfx.find(p_key);
	if (iter == end(m_tilemap_gfx))
		return nullptr;
	else
		return iter->second;
}

void fe::gfx::save_surface_as_bmp(SDL_Surface* srf,
	const std::string& p_path,
	const std::string& p_filename,
	bool p_destroy_surface) const {

	if (!srf)
		throw std::runtime_error("Could not create surface");

	klib::file::create_directories(p_path);
	std::string l_filepath{ p_path + "/" + p_filename };

	// convert to 24-bit bmp as we want to preserve the hot pink on disk
	SDL_Surface* out_bmp{ SDL_ConvertSurface(srf, SDL_PIXELFORMAT_RGB24) };
	if (!out_bmp)
		throw std::runtime_error("Could not create 24-bit surface");

	bool l_result{ SDL_SaveBMP(out_bmp, l_filepath.c_str()) };

	SDL_DestroySurface(out_bmp);

	if (p_destroy_surface)
		SDL_DestroySurface(srf);

	if (!l_result)
		throw std::runtime_error("Could not save " + l_filepath);
}

SDL_Surface* fe::gfx::load_bmp(const std::string& p_path,
	const std::string& p_filename) const {
	std::string l_filepath{ std::format("{}/{}", p_path, p_filename) };

	SDL_Surface* srf = SDL_LoadBMP(l_filepath.c_str());

	if (!srf)
		throw std::runtime_error("Could not load " + l_filepath);

	return srf;
}

klib::NES_tile fe::gfx::surface_region_to_nes_tile(SDL_Surface* srf,
	const std::vector<byte>& p_palette,
	int x, int y) const {
	klib::NES_tile result;

	for (int yy = 0; yy < 8; ++yy) {
		for (int xx = 0; xx < 8; ++xx) {
			Uint8 r, g, b, a;
			if (!SDL_ReadSurfacePixel(srf, x + xx, y + yy, &r, &g, &b, &a)) {
				continue; // skip if out of bounds
			}

			// Find closest of the 4 palette colors
			int bestIdx = 0;
			int bestDist = INT_MAX;
			for (int i = 0; i < 4; ++i) {
				const SDL_Color& palCol = m_nes_palette->colors[p_palette[i]];
				int dr = int(r) - int(palCol.r);
				int dg = int(g) - int(palCol.g);
				int db = int(b) - int(palCol.b);
				int dist = dr * dr + dg * dg + db * db;
				if (dist < bestDist) {
					bestDist = dist;
					bestIdx = i; // 0..3
				}
			}

			result.set_color(static_cast<std::size_t>(xx),
				static_cast<std::size_t>(yy),
				static_cast<Uint8>(bestIdx)
			);
		}
	}

	return result;
}

int fe::gfx::get_duplicate_count(const klib::NES_tile& p_q0, const klib::NES_tile& p_q1,
	const klib::NES_tile& p_q2, const klib::NES_tile& p_q3,
	const std::set<std::size_t>& p_reserved_idxs,
	const std::map<klib::NES_tile, std::vector<std::pair<std::size_t, std::size_t>>>& p_generated,
	const std::vector<klib::NES_tile>& p_tiles) const {

	int result{ 0 };

	if (p_generated.count(p_q0))
		++result;
	if (p_generated.count(p_q1))
		++result;
	if (p_generated.count(p_q2))
		++result;
	if (p_generated.count(p_q3))
		++result;

	for (std::size_t idx : p_reserved_idxs) {
		if (p_tiles.at(idx) == p_q0)
			++result;
		if (p_tiles.at(idx) == p_q1)
			++result;
		if (p_tiles.at(idx) == p_q2)
			++result;
		if (p_tiles.at(idx) == p_q3)
			++result;
	}

	return result;
}

// gfx import result queries
bool fe::gfx::has_tilemap_import_result(std::size_t p_key) const {
	return m_tilemap_import_results.contains(p_key);
}

fe::ChrTilemap fe::gfx::get_tilemap_import_result(std::size_t p_key) const {
	return m_tilemap_import_results.at(p_key);
}

void fe::gfx::clear_tilemap_import_result(std::size_t p_key) {
	m_tilemap_import_results.erase(p_key);
}

void fe::gfx::clear_all_tilemap_import_results(void) {
	m_tilemap_import_results.clear();
}

// gfx import functions
void fe::gfx::import_tilemap_bmp(SDL_Renderer* p_rnd,
	std::vector<ChrGfxTile>& p_tiles,
	const std::vector<std::vector<byte>>& p_palette,
	ChrDedupMode p_dedupmode,
	const std::string& p_path,
	const std::string& p_filename,
	std::size_t p_key) {

	SDL_Surface* srf{ load_bmp(p_path, p_filename) };

	// enforce multiples of 16
	if ((srf->w % 16) != 0 || (srf->h % 16) != 0) {
		SDL_DestroySurface(srf);
		throw std::runtime_error("BMP dimensions must be multiples of 16");
	}

	std::size_t mt_w{ static_cast<std::size_t>(srf->w / 16) };
	std::size_t mt_h{ static_cast<std::size_t>(srf->h / 16) };

	// the tilemap, containing concrete chr tiles
	std::vector<std::vector<std::optional<ChrMetaTile>>> l_final_tilemap(
		mt_h, std::vector<std::optional<ChrMetaTile>>(mt_w));

	// the candidate tilemap, also containing concrete chr tiles
	// but we don't know if we can use all of them before we execute
	std::vector<std::vector<std::optional<MetaTileCandidate>>> l_tilemap(
		mt_h, std::vector<std::optional<MetaTileCandidate>>(mt_w));

	for (std::size_t j{ 0 }; j < mt_h; ++j)
		for (std::size_t i{ 0 }; i < mt_w; ++i) {
			if (is_optional_bmp_region(srf, i, j))
				l_tilemap[j][i] = std::nullopt;
			else {
				std::vector<fe::MetaTileCandidate> l_cands;
				for (std::size_t pal{ 0 }; pal < 4; ++pal)
					l_cands.push_back(slice_and_quantize(
						srf, i, j, p_palette, pal, p_dedupmode, p_tiles
					));

				l_tilemap[j][i] = collapse_candidates(l_cands);
			}
		}

	// we now have the full tilemap with concrete chr tiles
	// and pre-calculated rgb-errors vs the bmp
	// we now emit chr tiles to our generated tilemap
	std::map<klib::NES_tile, std::vector<std::size_t>> tileToIndices;

	// fill out all read-only and unusable tiles
	for (std::size_t i{ 0 }; i < p_tiles.size(); ++i)
		if (!p_tiles[i].m_allowed || p_tiles[i].m_readonly)
			tileToIndices[p_tiles[i].m_tile].push_back(i);

	for (std::size_t j{ 0 }; j < mt_h; ++j)
		for (std::size_t i{ 0 }; i < mt_w; ++i)
			if (l_tilemap[j][i].has_value()) {
				l_final_tilemap[j][i] = fe::ChrMetaTile();
				l_final_tilemap[j][i]->m_palette = l_tilemap[j][i]->paletteIndex;

				for (std::size_t t{ 0 }; t < 4; ++t) {
					std::size_t idx{ allocate_or_reuse_chr(l_tilemap[j][i]->m_tiles[t],
						p_tiles, tileToIndices,
						p_palette[l_tilemap[j][i]->paletteIndex],
						p_dedupmode) };

					if (idx < 256)
						l_final_tilemap[j][i]->m_idxs.push_back(idx);
					else {
						auto pxpos{ mt_to_pixels(i, j, t) };

						l_final_tilemap[j][i]->m_idxs.push_back(
							best_substitute_chr_index(srf,
								pxpos.first, pxpos.second, p_palette.at(l_final_tilemap[j][i]->m_palette),
								tileToIndices, p_tiles)
						);
					}
				}
			}
			else
				l_final_tilemap[j][i] = std::nullopt;

	SDL_DestroySurface(srf);

	// if we have leftover chr space, set it to the empty tile
	std::vector<std::size_t> spareindices;

	for (std::size_t i{ 0 }; i < 256; ++i) {
		bool l_found{ false };
		for (const auto& kv : tileToIndices)
			for (std::size_t tidx : kv.second)
				if (i == tidx)
					l_found = true;

		if (!l_found)
			spareindices.push_back(i);
	}

	if (!spareindices.empty()) {
		klib::NES_tile l_empty;
		auto iter{ tileToIndices.find(l_empty) };

		if (iter != end(tileToIndices)) {
			for (std::size_t i : spareindices)
				iter->second.push_back(i);
		}
		else {
			tileToIndices.insert(std::make_pair(l_empty, spareindices));
		}
	}

	// finally done ... update result map and texture
	fe::ChrTilemap result{ fe::ChrTilemap(l_final_tilemap,
		chrtiletoindex_map_to_vector(tileToIndices),
		p_palette) };

	// add result to staging
	m_tilemap_import_results[p_key] = result;
	// render it to the outside
	gen_tilemap_texture(p_rnd, result, p_key);
}

std::pair<int, int> fe::gfx::mt_to_pixels(std::size_t mt_x, std::size_t mt_y,
	std::size_t quadrant) const {
	int px{ 16 * static_cast<int>(mt_x) };
	int py{ 16 * static_cast<int>(mt_y) };

	if (quadrant == 1 || quadrant == 3)
		px += 8;

	if (quadrant == 2 || quadrant == 3)
		py += 8;

	return std::make_pair(px, py);
}

fe::MetaTileCandidate fe::gfx::slice_and_quantize(
	SDL_Surface* p_srf,
	std::size_t mt_x, std::size_t mt_y,
	const std::vector<std::vector<byte>>& p_palette,
	std::size_t p_sub_pal_idx,
	fe::ChrDedupMode p_dedupmode,
	const std::vector<fe::ChrGfxTile>& p_tiles) const {

	fe::MetaTileCandidate result;
	result.paletteIndex = p_sub_pal_idx;
	result.rgbError = 0;

	int px{ 16 * static_cast<int>(mt_x) };
	int py{ 16 * static_cast<int>(mt_y) };

	result.m_tiles.push_back(
		surface_region_to_nes_tile(p_srf, p_palette[p_sub_pal_idx], px, py)
	);
	result.m_tiles.push_back(
		surface_region_to_nes_tile(p_srf, p_palette[p_sub_pal_idx], px + 8, py)
	);
	result.m_tiles.push_back(
		surface_region_to_nes_tile(p_srf, p_palette[p_sub_pal_idx], px, py + 8)
	);
	result.m_tiles.push_back(
		surface_region_to_nes_tile(p_srf, p_palette[p_sub_pal_idx], px + 8, py + 8)
	);

	for (std::size_t i{ 0 }; i < 4; ++i) {
		const auto& tile = result.m_tiles[i];

		// compute per-tile error
		int err = rgb_space_diff(tile,
			p_palette[p_sub_pal_idx],
			p_srf,
			px + static_cast<int>(i % 2) * 8,
			py + static_cast<int>(i / 2) * 8);

		result.m_quad_errors.push_back(err);
		result.rgbError += err;
	}

	auto l_unique_tiles{ gen_unique_tiles(result.m_tiles,
		p_palette[p_sub_pal_idx], p_dedupmode) };

	for (const auto& tile : l_unique_tiles)
		for (const auto& globaltile : p_tiles)
			if (globaltile.m_allowed &&
				chr_tile_equivalence(p_dedupmode,
					globaltile.m_tile, tile,
					p_palette[p_sub_pal_idx])) {
				result.reuseCount++;
				break;
			}

	return result;
}

std::vector<klib::NES_tile> fe::gfx::gen_unique_tiles(
	const std::vector<klib::NES_tile>& p_tiles,
	const std::vector<byte>& p_palette,
	fe::ChrDedupMode p_dedupmode
) const {
	std::vector<klib::NES_tile> result;

	for (const auto& tile : p_tiles) {
		bool l_unique{ true };
		for(const auto& utile : result)
			if (chr_tile_equivalence(p_dedupmode, tile, utile, p_palette)) {
				l_unique = false;
				break;
			}
		if (l_unique)
			result.push_back(tile);
	}

	return result;
}

int fe::gfx::rgb_space_diff(const klib::NES_tile& p_tile,
	const std::vector<byte>& p_palette,
	SDL_Surface* srf, int x, int y) const {
	int result{ 0 };

	for (int j{ 0 }; j < 8; ++j)
		for (int i{ 0 }; i < 8; ++i) {

			Uint8 r, g, b, a;
			if (!SDL_ReadSurfacePixel(srf, x + i, y + j, &r, &g, &b, &a))
				throw std::runtime_error("pixel out of bounds");

			byte palIndex = p_palette.at(p_tile.get_color(i, j));
			SDL_Color tile_pixel_col{ m_nes_palette->colors[palIndex] };

			int rr{ static_cast<int>(r) - static_cast<int>(tile_pixel_col.r) };
			int gg{ static_cast<int>(g) - static_cast<int>(tile_pixel_col.g) };
			int bb{ static_cast<int>(b) - static_cast<int>(tile_pixel_col.b) };

			result += rr * rr + gg * gg + bb * bb;
		}

	return result;
}

fe::MetaTileCandidate fe::gfx::collapse_candidates(
	const std::vector<fe::MetaTileCandidate>& cands) const {
	fe::MetaTileCandidate best;
	bool first = true;

	for (const auto& cand : cands) {
		if (first ||
			cand.rgbError < best.rgbError ||
			(cand.rgbError == best.rgbError && cand.reuseCount > best.reuseCount) ||
			(cand.rgbError == best.rgbError && cand.reuseCount == best.reuseCount &&
				cand.paletteIndex < best.paletteIndex)) {
			best = cand;
			first = false;
		}
	}

	return best;
}

std::size_t fe::gfx::allocate_or_reuse_chr(const klib::NES_tile& tile,
	std::vector<ChrGfxTile>& p_tiles,
	std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
	const std::vector<byte> p_palette,
	fe::ChrDedupMode p_dedupmode) const {

	// Deduplication: if tile already exists, reuse any allowed index
	for (auto& [candidateTile, indices] : tileToIndices) {
		if (chr_tile_equivalence(p_dedupmode, tile, candidateTile,
			p_palette)) {
			for (std::size_t idx : indices) {
				if (p_tiles[idx].m_allowed) {
					return idx;
				}
			}
		}
	}

	// Pass 1: search p_tiles for a mutable exact match (allowed && !readonly && slot holds tile)
	// We DON'T pre-seed mutable slots in tileToIndices, so we must scan p_tiles here.
	for (std::size_t idx{ 0 }; idx < p_tiles.size(); ++idx) {
		if (p_tiles[idx].m_allowed && !p_tiles[idx].m_readonly &&
			chr_tile_equivalence(p_dedupmode,
				p_tiles[idx].m_tile, tile,
				p_palette)) {
			p_tiles[idx].m_readonly = true;            // claim and lock
			tileToIndices[tile].push_back(idx);        // record now that it's claimed
			return idx;
		}
	}

	// Allocation: find a writable slot (allowed && !readOnly)
	for (std::size_t idx{ 0 }; idx < p_tiles.size(); ++idx) {
		if (p_tiles[idx].m_allowed && !p_tiles[idx].m_readonly) {
			p_tiles[idx].m_readonly = true;
			tileToIndices[tile].push_back(idx);
			return idx;
		}
	}

	// No usable slot available
	return 256;
}

std::size_t fe::gfx::best_substitute_chr_index(
	SDL_Surface* srf,
	int px, int py,
	const std::vector<byte>& subPalette,
	const std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
	const std::vector<ChrGfxTile>& p_tiles) const {

	std::size_t bestChrIdx = 0;
	int bestErr = std::numeric_limits<int>::max();

	// Iterate over committed tiles (authoritative content)
	for (const auto& [candidateTile, indices] : tileToIndices) {
		// Compute error IN CONTEXT for the candidate’s CHR content
		int err = rgb_space_diff(candidateTile, subPalette, srf, px, py);
		if (err >= bestErr) continue;

		// Among its recorded indices, choose an allowed one to emit
		// Prefer the smallest allowed index for determinism.
		std::size_t chosenIdx = std::numeric_limits<std::size_t>::max();
		for (std::size_t idx : indices) {
			if (idx < p_tiles.size() && p_tiles[idx].m_allowed) {
				chosenIdx = std::min(chosenIdx, idx);
			}
		}
		if (chosenIdx == std::numeric_limits<std::size_t>::max()) {
			// This map entry has no allowed indices (all disallowed) -> skip
			continue;
		}

		// Better match found -> record
		bestErr = err;
		bestChrIdx = chosenIdx;
	}

	return bestChrIdx; // Guaranteed to be one of the committed indices

}

std::vector<klib::NES_tile> fe::gfx::chrtiletoindex_map_to_vector(
	const std::map<klib::NES_tile, std::vector<std::size_t>>& tileToIndices,
	std::size_t chr_count
) const {
	std::vector<klib::NES_tile> result(chr_count, klib::NES_tile());

	for (const auto& kv : tileToIndices)
		for (std::size_t idx : kv.second)
			if (idx >= chr_count)
				throw std::runtime_error("invalid chr tile index");
			else
				result[idx] = kv.first;

	return result;
}

bool fe::gfx::chr_tile_equivalence(fe::ChrDedupMode p_dedupmode,
	const klib::NES_tile& p_tile_a,
	const klib::NES_tile& p_tile_b,
	const std::vector<byte>& p_palette) const {

	if (p_dedupmode == fe::ChrDedupMode::PalIndex_Eq)
		return p_tile_a == p_tile_b;
	else {

		for (std::size_t j{ 0 }; j < 8; ++j)
			for (std::size_t i{ 0 }; i < 8; ++i) {
				std::size_t inda{ p_palette[p_tile_a.get_color(i, j)] };
				std::size_t indb{ p_palette[p_tile_b.get_color(i, j)] };

				if (p_dedupmode == fe::ChrDedupMode::NESPalIndex_Eq) {
					if (inda != indb)
						return false;
				}
				else if (!rgb_equivalence(
					m_nes_palette->colors[inda],
					m_nes_palette->colors[indb])
					)
					return false;
			}
	}

	return true;
}

bool fe::gfx::rgb_equivalence(const SDL_Color col_a, const SDL_Color col_b) const {
	return (col_a.r == col_b.r) && (col_a.g == col_b.g) && (col_a.b == col_b.b);
}

bool fe::gfx::is_optional_bmp_region(SDL_Surface* srf,
	std::size_t mt_x, std::size_t mt_y) const {
	auto l_pxpos{ mt_to_pixels(mt_x, mt_y, 0) };

	for (int j{ 0 }; j < 16; ++j)
		for (int i{ 0 }; i < 16; ++i) {
			Uint8 r, g, b, a;
			if (!SDL_ReadSurfacePixel(srf, l_pxpos.first + i,
				l_pxpos.second + j, &r, &g, &b, &a))
				throw std::runtime_error("pixel out of bounds");

			if (r != m_hot_pink.r ||
				g != m_hot_pink.g ||
				b != m_hot_pink.b)
				return false;
		}

	return true;
}

// functions for bmp export
SDL_Surface* fe::gfx::gen_tilemap_surface(const fe::ChrTilemap& p_tilemap) const {
	const auto& tilemap{ p_tilemap.m_tilemap };
	const auto& chrtiles{ p_tilemap.m_tiles };
	const auto& pals{ p_tilemap.m_palette };

	int width = static_cast<int>(tilemap.empty() ? 0 : 16 * tilemap[0].size());
	int height = 16 * static_cast<int>(tilemap.size());

	auto srf{ create_sdl_surface(width, height, true, true) };

	for (std::size_t j{ 0 }; j < tilemap.size(); ++j)
		for (std::size_t i{ 0 }; i < tilemap[j].size(); ++i) {
			if (tilemap[j][i].has_value()) {

				for (std::size_t q{ 0 }; q < 4; ++q) {
					auto pxpos{ mt_to_pixels(i, j, q) };
					draw_nes_tile_on_surface(srf,
						pxpos.first, pxpos.second,
						chrtiles.at(tilemap[j][i]->m_idxs[q]),
						pals.at(tilemap[j][i]->m_palette));
				}
			}
		}

	return srf;
}

void fe::gfx::save_tilemap_bmp(const fe::ChrTilemap& p_tilemap,
	const std::string& p_path,
	const std::string& p_filename) const {

	save_surface_as_bmp(gen_tilemap_surface(p_tilemap), p_path, p_filename);
}

void fe::gfx::gen_tilemap_texture(SDL_Renderer* p_rnd, const fe::ChrTilemap& p_tilemap,
	std::size_t p_key) {

	auto srf{ gen_tilemap_surface(p_tilemap) };

	delete_texture(m_tilemap_gfx[p_key]);

	m_tilemap_gfx[p_key] = surface_to_texture(p_rnd, srf);
}

const SDL_Palette* fe::gfx::get_nes_palette(void) const {
	return m_nes_palette;
}
