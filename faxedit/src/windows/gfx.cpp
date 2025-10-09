#include "gfx.h"

fe::gfx::gfx(void) :
	m_nes_palette{ SDL_CreatePalette(256) }
{
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
}

void fe::gfx::generate_textures(SDL_Renderer* p_rnd,
	const std::vector<klib::NES_tile>& p_tiles) {

	std::vector<byte> l_palette{ 0x0f, 0x05, 0x17, 0x26 };

	for (const auto& tile : p_tiles) {

		auto l_srf = create_sdl_surface(8, 8);

		for (int y{ 0 }; y < 8; ++y)
			for (int x{ 0 }; x < 8; ++x) {
				put_nes_pixel(l_srf, x, y, l_palette[tile.get_color(x, y)]);
			}

		m_textures.push_back(surface_to_texture(p_rnd, l_srf));

	}
}

SDL_Texture* fe::gfx::get_texture(std::size_t p_txt_no) const {
	return m_textures.at(p_txt_no);
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
