#include "NES_tile.h"
#include <stdexcept>

klib::NES_tile::NES_tile(const std::vector<byte>& p_rom_data, std::size_t p_offset)
	: m_tile_data{ std::vector<std::vector<byte>>(8, std::vector<byte>(8, 0)) }
{
	for (int y = 0; y < 8; ++y) {
		// bitplanes are 8 bytes apart
		byte b0 = p_rom_data[p_offset + y];        // plane 0
		byte b1 = p_rom_data[p_offset + 8 + y];    // plane 1

		for (int x = 0; x < 8; ++x) {
			int bit = 7 - x;
			byte low = (b0 >> bit) & 1;
			byte high = (b1 >> bit) & 1;
			m_tile_data[y][x] = (high << 1) | low;        // palette index 0–3
		}
	}

}

klib::NES_tile::NES_tile(void) :
	m_tile_data{ std::vector<std::vector<byte>>(8, std::vector<byte>(8, 0)) }
{
}

void klib::NES_tile::flip_h(void) {
	const std::size_t W = w();
	const std::size_t H = h();
	if (W == 0 || H == 0) return;

	for (std::size_t y = 0; y < H; ++y) {
		for (std::size_t x = 0; x < W / 2; ++x) {
			std::swap(m_tile_data[y][x], m_tile_data[y][W - 1 - x]);
		}
	}
}

void klib::NES_tile::flip_v(void) {
	const std::size_t H = h();
	if (h() == 0) return;

	for (std::size_t y = 0; y < H / 2; ++y) {
		std::swap(m_tile_data[y], m_tile_data[H - 1 - y]);
	}
}

void klib::NES_tile::flip(bool h, bool v) {
	if (h)
		flip_h();
	if (v)
		flip_v();
}

bool klib::NES_tile::is_empty(void) const {
	for (const auto& row : m_tile_data)
		for (byte b : row)
			if (b != 0)
				return false;
	return true;
}

bool klib::NES_tile::operator<(const klib::NES_tile& rhs) const {
	return m_tile_data < rhs.m_tile_data;
}

bool klib::NES_tile::operator==(const klib::NES_tile& rhs) const {
	return m_tile_data == rhs.m_tile_data;
}

std::size_t klib::NES_tile::w(void) const {
	return m_tile_data.empty() ? 0 : m_tile_data[0].size();
}

std::size_t klib::NES_tile::h(void) const {
	return m_tile_data.size();
}

byte klib::NES_tile::get_color(std::size_t p_x, std::size_t p_y) const {
	return m_tile_data.at(p_y).at(p_x);
}

void klib::NES_tile::set_color(std::size_t p_x, std::size_t p_y, byte p_pal_idx) {
	m_tile_data.at(p_y).at(p_x) = p_pal_idx;
}

std::vector<byte> klib::NES_tile::to_bytes() const {
	if (m_tile_data.size() != 8 || m_tile_data[0].size() != 8) {
		throw std::runtime_error("NES_tile must be 8x8");
	}

	std::vector<byte> out(16, 0);

	for (int row = 0; row < 8; ++row) {
		byte plane0 = 0;
		byte plane1 = 0;

		for (int col = 0; col < 8; ++col) {
			byte val = m_tile_data[row][col] & 0x03; // ensure 0..3
			byte bit = 1 << (7 - col);          // NES packs MSB leftmost

			if (val & 0x01) plane0 |= bit;      // low bit
			if (val & 0x02) plane1 |= bit;      // high bit
		}

		out[static_cast<std::size_t>(row)] = plane0; // first 8 bytes = plane 0
		out[static_cast<std::size_t>(row + 8)] = plane1; // next 8 bytes = plane 1
	}

	return out;
}

klib::CanonChoice klib::NES_tile::canonicalize(void) {
	CanonChoice bestChoice{ false, false };
	NES_tile bestTile = *this;

	const struct { bool h, v; } candidates[4] = {
		{false,false},
		{true,false},
		{false,true},
		{true,true}
	};

	for (auto c : candidates) {
		NES_tile candidate = *this;
		candidate.flip(c.h, c.v);

		// STRICT comparison.
		// If equal, do NOT replace -> earlier candidate wins (no-flip preferred).
		if (candidate < bestTile) {
			bestTile = std::move(candidate);
			bestChoice.h = c.h;
			bestChoice.v = c.v;
		}
	}

	*this = std::move(bestTile);
	return bestChoice;
}
