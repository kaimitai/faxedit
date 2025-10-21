#include "gfx.h"

constexpr int TILEMAP_SCALE{ 1 };

fe::gfx::gfx(SDL_Renderer* p_rnd) :
	m_nes_palette{ SDL_CreatePalette(256) },
	m_atlas{ nullptr },
	m_metatile_gfx{ std::vector<SDL_Texture*>(256, nullptr) }
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

SDL_Texture* fe::gfx::get_screen_texture(void) const {
	return m_screen_texture;
}

SDL_Texture* fe::gfx::get_metatile_texture(std::size_t p_mt_no) const {
	return m_metatile_gfx.at(p_mt_no);
}

SDL_Surface* fe::gfx::create_sdl_surface(int p_w, int p_h) const {
	SDL_Surface* l_bmp = SDL_CreateSurface(p_w, p_h, SDL_PIXELFORMAT_ABGR8888);

	SDL_Palette* l_palette = SDL_CreatePalette(256);
	SDL_Color l_out_palette[256] = {};

	for (std::size_t i{ 0 }; i < NES_PALETTE.size(); ++i)
		l_out_palette[i] = { NES_PALETTE[i][0], NES_PALETTE[i][1], NES_PALETTE[i][2], 255 };

	SDL_SetPaletteColors(l_palette, l_out_palette, 0, 256);

	return l_bmp;
}

void fe::gfx::put_nes_pixel(SDL_Surface* srf, int x, int y, byte p_palette_index) {
	SDL_Color l_col{ m_nes_palette->colors[p_palette_index] };
	SDL_WriteSurfacePixel(srf, x, y, l_col.r, l_col.g, l_col.b, l_col.a);
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

void fe::gfx::draw_rect_on_screen(SDL_Renderer* p_rnd, SDL_Color p_color,
	int p_x, int p_y, int p_w, int p_h) const {
	SDL_SetRenderTarget(p_rnd, m_screen_texture);
	SDL_SetRenderDrawColor(p_rnd, p_color.r, p_color.g, p_color.b, p_color.a);

	SDL_FRect l_rect(static_cast<float>(TILEMAP_SCALE * 16 * p_x),
		static_cast<float>(TILEMAP_SCALE * 16 * p_y),
		static_cast<float>(p_w * TILEMAP_SCALE * 16),
		static_cast<float>(p_h * TILEMAP_SCALE * 16));

	SDL_RenderRect(p_rnd, &l_rect);

	SDL_SetRenderTarget(p_rnd, nullptr);
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
