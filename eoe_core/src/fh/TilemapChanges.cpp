#include "TilemapChanges.h"
#include <cassert>
#include <format>
#include <stdexcept>

fh::TilemapChanges::TilemapChanges(byte p_init_max_world_idx) {
	resize_data_if_needed(p_init_max_world_idx);
}

bool fh::TilemapChanges::empty(void) const {
	for (const auto& world : data)
		if (!world.empty())
			return false;

	return true;
}

std::vector<byte> fh::TilemapChanges::to_bytes(word cpu_addr) const {
	std::vector<byte> result;

	const std::size_t world_ptr_table_size = data.size() * sizeof(word);
	word next_cpu_addr = static_cast<word>(cpu_addr + world_ptr_table_size + EOE_TILEMAP_CHANGE_HEADER.size());

	std::vector<word> world_table_addrs;

	for (const auto& world : data) {
		world_table_addrs.push_back(next_cpu_addr);
		next_cpu_addr += static_cast<word>(world.size() * DESCRIPTOR_SIZE + 1);
	}

	std::vector<std::map<byte, word>> block_addrs(data.size());

	for (std::size_t world{ 0 }; world < data.size(); ++world) {
		for (const auto& [screen, change] : data[world]) {
			block_addrs[world][screen] = next_cpu_addr;
			next_cpu_addr += static_cast<word>(1 + change.changes.size() * 2); // count byte + 2 bytes per tile change
		}
	}

	const word empty_table_addr = next_cpu_addr;

	// emit header with version info for the sake of disassembly
	result.insert(result.end(), begin(EOE_TILEMAP_CHANGE_HEADER), end(EOE_TILEMAP_CHANGE_HEADER));

	// emit world pointers
	for (std::size_t world = 0; world < data.size(); ++world) {
		word addr = data[world].empty() ? empty_table_addr : world_table_addrs[world];

		result.push_back(addr & 0xff);
		result.push_back(addr >> 8);
	}

	for (std::size_t world = 0; world < data.size(); ++world) {
		for (const auto& [screen, change] : data[world]) {
			const word addr = block_addrs[world].at(screen);

			result.push_back(screen);
			result.push_back(change.flag);
			result.push_back(addr & 0xff);
			result.push_back(addr >> 8);
		}

		result.push_back(0xff);
	}

	for (std::size_t world{ 0 }; world < data.size(); ++world) {
		for (const auto& [screen, change] : data[world]) {
			if (change.changes.empty())
				throw std::runtime_error(
					std::format(
						"Screen {} in world {} has no tile changes",
						screen, world));

			result.push_back(static_cast<byte>(change.changes.size()));

			for (const auto& tile : change.changes) {
				result.push_back(static_cast<byte>((tile.y << 4) | tile.x));
				result.push_back(tile.id);
			}
		}
	}

	// shared 0xff for all worlds with no screen tilemap changes
	result.push_back(0xff);
	++next_cpu_addr;

	assert(result.size() == static_cast<std::size_t>(next_cpu_addr - cpu_addr));

	return result;
}

void fh::TilemapChanges::resize_data_if_needed(byte p_world) {
	if (p_world > MAX_WORLD)
		throw std::runtime_error(std::format("World {} exceeds the maximum supported world index ({})", p_world, MAX_WORLD));

	if (data.size() <= p_world)
		data.resize(p_world + 1);
}

void fh::TilemapChanges::add_screen(byte p_world, byte p_screen, byte p_flag) {
	resize_data_if_needed(p_world);

	if (p_screen == 0xff)
		throw std::runtime_error("Screen $ff is reserved as the descriptor terminator");
	if (data[p_world].size() >= MAX_SCREENS_PER_WORLD)
		throw std::runtime_error(std::format("World {} exceeds the maximum of {} tilemap change screens", p_world, MAX_SCREENS_PER_WORLD));

	auto [it, inserted] = data[p_world].emplace(
		p_screen,
		ScreenTilemapChange{ .flag = p_flag });

	if (!inserted)
		throw std::runtime_error(
			std::format("Duplicate screen {} in world {}", p_screen, p_world));
}

void fh::TilemapChanges::add_change(byte p_world, byte p_screen, byte p_x, byte p_y, byte p_id) {
	if (p_world >= data.size())
		throw std::runtime_error(std::format("World {} not initialized", p_world));

	auto it = data[p_world].find(p_screen);

	if (it == end(data[p_world]))
		throw std::runtime_error(std::format("Screen {} in world {} has not been initialized with a flag number", p_screen, p_world));

	if (p_x >= 16 || p_y >= 13)
		throw std::runtime_error(std::format("Tile coordinate ({}, {}) is outside the supported range (0-15 by 0-12)", p_x, p_y));

	if (it->second.changes.size() >= MAX_TILE_CHANGES)
		throw std::runtime_error(std::format("Screen {} in world {} exceeds the maximum of {} tile changes", p_screen, p_world, MAX_TILE_CHANGES));

	it->second.changes.push_back(TileChange{
		.x = p_x,
		.y = p_y,
		.id = p_id
		});
}
