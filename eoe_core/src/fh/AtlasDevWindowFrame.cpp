#include "HackManager.h"
#include "common/klib/Asm6502.h"
#include "common/lodepng.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

// the six border tiles every window draws (dialogue, shops, menus, status):
// 1 top left, 2 the top and bottom edge, 3 top right, 4 the sides, 5 bottom
// left, 6 bottom right. 96 bytes of 2bpp tiles at bank 10 $8150, the same in
// every region. the hack only rewrites these tiles: no code, no ram, and the
// window palette stays as it is.
namespace {
	using Asm = klib::Asm6502;
	using Tile = std::array<std::array<byte, 8>, 8>;
	using Frame = std::array<Tile, 6>;
	constexpr byte BANK{ 10 };
	constexpr word FRAME{ 0x8150 };
	constexpr std::size_t TILE_BYTES{ 16 }, FRAME_BYTES{ 6 * TILE_BYTES };
	constexpr unsigned ORIGINAL_CRC{ 0x27978569 };
	constexpr std::array<const char*, 6> TILE_PARAMS{ "tl", "top", "tr", "side", "bl", "br" };

	[[noreturn]] void fail(const std::string& p_message) {
		throw std::runtime_error("AtlasDevWindowFrame: " + p_message);
	}

	// eight rows of eight pixels, colors 0 to 3 ('.' is 0), rows optionally split by '/'
	Tile parse(std::string_view p_name, std::string_view p_text) {
		std::string pixels;
		for (char c : p_text)
			if (c != '/')
				pixels += c;
		if (pixels.size() != 64)
			fail(std::format("{} needs 64 pixels, eight rows of eight", p_name));
		Tile tile{};
		for (std::size_t i{ 0 }; i < 64; ++i) {
			const char c{ pixels[i] };
			if (c != '.' && (c < '0' || c > '3'))
				fail(std::format("{}: a pixel is 0 to 3, or '.' for 0", p_name));
			tile[i / 8][i % 8] = c == '.' ? 0 : static_cast<byte>(c - '0');
		}
		return tile;
	}

	Tile decode(const std::vector<byte>& p_rom, std::size_t p_off) {
		Tile tile{};
		for (std::size_t y{ 0 }; y < 8; ++y)
			for (std::size_t x{ 0 }; x < 8; ++x)
				tile[y][x] = static_cast<byte>(((p_rom[p_off + y] >> (7 - x)) & 1)
					| (((p_rom[p_off + 8 + y] >> (7 - x)) & 1) << 1));
		return tile;
	}

	void encode(std::vector<byte>& p_rom, std::size_t p_off, const Tile& p_tile) {
		for (std::size_t y{ 0 }; y < 8; ++y) {
			byte low{ 0 }, high{ 0 };
			for (std::size_t x{ 0 }; x < 8; ++x) {
				low |= static_cast<byte>((p_tile[y][x] & 1) << (7 - x));
				high |= static_cast<byte>(((p_tile[y][x] >> 1) & 1) << (7 - x));
			}
			p_rom[p_off + y] = low;
			p_rom[p_off + 8 + y] = high;
		}
	}

	Tile mirror(const Tile& p_tile) {
		Tile out{};
		for (std::size_t y{ 0 }; y < 8; ++y)
			for (std::size_t x{ 0 }; x < 8; ++x)
				out[y][x] = p_tile[y][7 - x];
		return out;
	}

	Tile flip(const Tile& p_tile) {
		Tile out{};
		for (std::size_t y{ 0 }; y < 8; ++y)
			out[y] = p_tile[7 - y];
		return out;
	}

	Tile transpose(const Tile& p_tile) {
		Tile out{};
		for (std::size_t y{ 0 }; y < 8; ++y)
			for (std::size_t x{ 0 }; x < 8; ++x)
				out[y][x] = p_tile[x][y];
		return out;
	}

	// a top-left corner and a top edge make all six: the corner mirrored and
	// flipped for the others, the edge turned for the sides
	Frame from_corner(const Tile& p_corner, const Tile& p_edge) {
		return { p_corner, p_edge, mirror(p_corner), transpose(p_edge), flip(p_corner), mirror(flip(p_corner)) };
	}

	// band: four colors from the outside in, mirrored to eight rows, so the one
	// tile the game uses for both the top and the bottom (and for both sides)
	// always meets the corners. the corner follows the band round its shape.
	std::array<byte, 8> band_rows(std::string_view p_band) {
		if (p_band.size() != 4)
			fail("band is four colors, from the outside in");
		std::array<byte, 8> rows{};
		for (std::size_t i{ 0 }; i < 4; ++i) {
			if (p_band[i] < '0' || p_band[i] > '3')
				fail("band: a color is 0 to 3");
			rows[i] = rows[7 - i] = static_cast<byte>(p_band[i] - '0');
		}
		return rows;
	}

	Frame from_band(const std::array<byte, 8>& p_rows, std::string_view p_shape, byte p_diamond) {
		Tile edge{}, corner{};
		for (int y{ 0 }; y < 8; ++y)
			for (int x{ 0 }; x < 8; ++x) {
				edge[y][x] = p_rows[y];
				// which of the band's rows reaches this pixel of the corner
				double depth{ static_cast<double>(std::min(y, x)) };
				if (p_shape == "rounded" && y < 7 && x < 7)
					depth = 7.0 - std::sqrt(static_cast<double>((7 - y) * (7 - y) + (7 - x) * (7 - x)));
				else if (p_shape == "cut")
					depth = std::min(depth, static_cast<double>(y + x - 3));
				const int row{ static_cast<int>(std::floor(depth + 0.5)) };
				corner[y][x] = row < 0 ? 0 : p_rows[std::min(row, 7)];
				if (p_shape == "diamond" && std::abs(y - 3.5) + std::abs(x - 3.5) == 3.0)
					corner[y][x] = p_diamond;
			}
		return from_corner(corner, edge);
	}
}

word fh::HackManager::install_AtlasDevWindowFrame(const fe::Config&, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) const {
	// the parameters first, so a typo is reported before anything about the rom.
	// the hack holds no graphics of its own: the frame is the original, a band and
	// corner (a style is a name for one), or the modder's tiles
	struct Style { const char* name; const char* band; const char* corner; byte diamond; };
	constexpr std::array<Style, 8> STYLES{ {
		{ "thin", "0030", "square", 2 }, { "bevel", "0123", "square", 2 },
		{ "inset", "3210", "square", 2 }, { "rounded", "0003", "rounded", 2 },
		{ "double", "0203", "square", 2 }, { "chamfer", "0203", "cut", 2 },
		{ "soft", "0102", "rounded", 2 }, { "ornate", "0003", "diamond", 1 } } };
	const Style* style{ nullptr };
	if (p_hack.has_param("style")) {
		const std::string name{ p_hack.get_string("style") };
		for (const auto& candidate : STYLES)
			if (name == candidate.name)
				style = &candidate;
		if (!style)
			fail("style must be thin, bevel, inset, rounded, double, chamfer, soft or ornate");
		if (p_hack.has_param("band") || p_hack.has_param("corner") || p_hack.has_param("diamond"))
			fail("a style is a band and a corner: use style, or band with corner and diamond, not both");
	}
	const std::string shape{ p_hack.string_or("corner", "square") };
	if (shape != "square" && shape != "rounded" && shape != "cut" && shape != "diamond")
		fail("corner must be square, rounded, cut or diamond");
	if ((p_hack.has_param("corner") || p_hack.has_param("diamond")) && !p_hack.has_param("band"))
		fail("corner and diamond shape a band");
	if (p_hack.has_param("diamond") && shape != "diamond")
		fail("diamond is the color of corner=diamond");
	const std::string diamond{ p_hack.string_or("diamond", "2") };
	if (diamond.size() != 1 || diamond[0] < '1' || diamond[0] > '3')
		fail("diamond is a color, 1 to 3");
	std::optional<Frame> drawn;
	if (p_hack.has_param("band"))
		drawn = from_band(band_rows(p_hack.get_string("band")), shape, static_cast<byte>(diamond[0] - '0'));
	else if (style)
		drawn = from_band(band_rows(style->band), style->corner, style->diamond);
	std::array<std::optional<Tile>, 6> own{};
	for (std::size_t i{ 0 }; i < own.size(); ++i)
		if (p_hack.has_param(TILE_PARAMS[i]))
			own[i] = parse(TILE_PARAMS[i], p_hack.get_string(TILE_PARAMS[i]));

	const auto off{ Asm::get_file_offset(BANK, FRAME) };
	if (p_rom.size() < off + FRAME_BYTES)
		fail("the rom has no bank 10");
	if (lodepng_crc32(p_rom.data() + off, FRAME_BYTES) != ORIGINAL_CRC)
		fail("the frame tiles at bank 10 $8150 are not the original ones");

	Frame frame{};
	for (std::size_t i{ 0 }; i < frame.size(); ++i)
		frame[i] = decode(p_rom, off + i * TILE_BYTES);
	if (drawn)
		frame = *drawn;
	for (std::size_t i{ 0 }; i < frame.size(); ++i)
		if (own[i])
			frame[i] = *own[i];

	for (std::size_t i{ 0 }; i < frame.size(); ++i)
		encode(p_rom, off + i * TILE_BYTES, frame[i]);
	return cpu_addr;
}
