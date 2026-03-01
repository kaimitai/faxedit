#include "SpriteAnimationFrame.h"
#include <stdexcept>

fe::SpriteAnimationFrame::SpriteAnimationFrame(const std::vector<byte>& p_rom, std::size_t p_offset) :
	offset_x{ static_cast<int>(static_cast<char>(p_rom.at(p_offset + 1))) },
	offset_y{ static_cast<int>(static_cast<char>(p_rom.at(p_offset + 2))) },
	pivot_x{ static_cast<int>(static_cast<char>(p_rom.at(p_offset + 3))) }
{
	std::size_t w{ static_cast<std::size_t>(p_rom.at(p_offset)) % 16 + 1 };
	std::size_t h{ static_cast<std::size_t>(p_rom.at(p_offset)) / 16 + 1 };

	p_offset += 4;

	for (std::size_t y{ 0 }; y < h; ++y) {

		std::vector<std::optional<SpriteFrameTile>> l_row;

		for (std::size_t x{ 0 }; x < w; ++x) {
			byte tile_no{ p_rom.at(p_offset++) };

			if (tile_no == 0xff) {
				l_row.push_back(std::nullopt);
			}
			else {
				byte l_attr{ p_rom.at(p_offset++) };

				l_row.push_back(
					fe::SpriteFrameTile(tile_no,
						l_attr & 0b11, // low 2 bits: palette idx
						(l_attr & 0x80) != 0, // v-flip, most significant bit
						(l_attr & 0x40) != 0) // h-flip: 2nd most significant bit
				);
			}
		}

		tilemap.push_back(l_row);
	}
}

// warning about this constructor: it leaves an empty frame, which cannot be byte-encoded
fe::SpriteAnimationFrame::SpriteAnimationFrame(void) :
	offset_x{ 0 }, offset_y{ 0 }, pivot_x{ 0 }
{
}

std::size_t fe::SpriteAnimationFrame::w(void) const {
	return tilemap.empty() ? 0 : tilemap[0].size();
}

std::size_t fe::SpriteAnimationFrame::h(void) const {
	return tilemap.size();
}

std::vector<byte> fe::SpriteAnimationFrame::to_bytes(const std::map<byte, byte>& remap) const {
	std::vector<byte> result;

	if (tilemap.empty() || tilemap[0].empty())
		throw std::runtime_error("Empty animation frames not allowed");

	std::size_t dims_byte{ (tilemap.size() - 1) * 16 + tilemap[0].size() - 1 };
	result.push_back(static_cast<byte>(dims_byte));
	result.push_back(static_cast<byte>(static_cast<char>(offset_x)));
	result.push_back(static_cast<byte>(static_cast<char>(offset_y)));
	result.push_back(static_cast<byte>(static_cast<char>(pivot_x)));

	for (const auto& row : tilemap)
		for (const auto& tile : row) {
			if (tile) {
				auto tilebytes{ tile->to_bytes(remap) };
				result.insert(end(result), begin(tilebytes), end(tilebytes));
			}
			else
				result.push_back(0xff);
		}

	return result;
}

std::map<byte, int> fe::SpriteAnimationFrame::get_tile_usage(void) const {
	std::map<byte, int> result;

	for (const auto& row : tilemap)
		for (const auto& tile : row)
			if (tile)
				++result[tile->index];

	return result;
}

int fe::SpriteAnimationFrame::get_empty_tile_count(void) const {
	int result{ 0 };

	for (const auto& row : tilemap)
		for (const auto& tile : row)
			if (!tile)
				++result;

	return result;
}

std::vector<byte> fe::SpriteFrameTile::to_bytes(const std::map<byte, byte>& remap) const {
	std::vector<byte> result;

	if (remap.contains(index))
		result.push_back(remap.at(index));
	else
		result.push_back(index);

	byte attr = 0;
	// low 2 bits: sub-palette
	attr |= (sub_palette & 0b11);
	// bit 6: h-flip
	if (h_flip) attr |= 0x40;
	// bit 7: v-flip
	if (v_flip) attr |= 0x80;
	result.push_back(attr);

	return result;
}
