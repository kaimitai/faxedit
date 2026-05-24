#include "Chunk.h"
#include "./../common/klib/Bitreader.h"
#include "./../common/klib/Kutil.h"

#include <format>
#include <map>
#include <stdexcept>

void fe::Chunk::decompress_and_add_screen(const std::vector<byte>& p_rom,
	std::size_t p_offset) {
	m_screens.push_back(fe::Screen(p_rom, p_offset));
}

std::vector<byte> fe::Chunk::extract_bytes(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_length) const {
	std::vector<byte> l_result;

	l_result.insert(end(l_result), begin(p_rom) + p_offset,
		begin(p_rom) + p_offset + p_length);

	return l_result;
}

void fe::Chunk::set_screen_scroll_properties(const std::vector<byte>& p_rom,
	std::size_t p_offset) {
	for (std::size_t i{ 0 }; i < m_screens.size(); ++i)
		m_screens[i].set_scroll_properties(p_rom, p_offset + 4 * i);
}

void fe::Chunk::add_metatiles(const std::vector<byte>& p_rom, std::size_t p_metatile_count,
	std::size_t p_tl_offset, std::size_t p_tr_offset, std::size_t p_bl_offset, std::size_t p_br_offset,
	std::size_t p_attributes_offset, std::size_t p_properties_offset) {

	for (std::size_t i{ 0 }; i < p_metatile_count; ++i) {
		m_metatiles.push_back(fe::Metatile(p_rom.at(p_tl_offset + i),
			p_rom.at(p_tr_offset + i),
			p_rom.at(p_bl_offset + i),
			p_rom.at(p_br_offset + i),
			p_rom.at(p_attributes_offset + i),
			p_rom.at(p_properties_offset + i)
		)
		);
	}
}

void fe::Chunk::set_screen_doors(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_door_param_offset,
	byte p_param_offset) {

	for (std::size_t i{ p_offset }; p_rom.at(i) != 0xff; i += 4) {

		std::size_t l_screen_id{ p_rom.at(i) };
		if (l_screen_id >= m_screens.size())
			return;

		m_screens.at(l_screen_id).m_doors.push_back(
			fe::Door(
				p_rom.at(i + 1),
				p_rom.at(i + 2),
				p_rom.at(i + 3),
				p_rom,
				p_door_param_offset,
				p_param_offset
			)
		);
	}
}

void fe::Chunk::add_screen_sprite(std::size_t p_screen_no, byte p_id, byte p_x,
	byte p_y) {
	m_screens.at(p_screen_no).add_sprite(p_id, p_x, p_y);
}

void fe::Chunk::set_screen_sprite_text(std::size_t p_screen_no,
	std::size_t p_sprite_no, byte p_text_id) {
	m_screens.at(p_screen_no).set_sprite_text(p_sprite_no, p_text_id);
}

std::vector<byte> fe::Chunk::get_block_property_bytes(void) const {
	std::vector<byte> l_result;

	for (const auto& mt : m_metatiles)
		l_result.push_back(mt.m_block_property);

	return l_result;
}

// helper
std::vector<byte> fe::Chunk::get_metatile_quadrant_bytes(std::size_t p_x, std::size_t p_y) const {
	std::vector<byte> l_result;

	for (const auto& mt : m_metatiles)
		l_result.push_back(mt.m_tilemap.at(p_y).at(p_x));

	return l_result;
}

std::vector<byte> fe::Chunk::get_screen_scroll_bytes(void) const {
	std::vector<byte> l_result;

	for (const auto& scr : m_screens) {
		l_result.push_back(scr.m_scroll_left.has_value() ? scr.m_scroll_left.value() : 0xff);
		l_result.push_back(scr.m_scroll_right.has_value() ? scr.m_scroll_right.value() : 0xff);
		l_result.push_back(scr.m_scroll_up.has_value() ? scr.m_scroll_up.value() : 0xff);
		l_result.push_back(scr.m_scroll_down.has_value() ? scr.m_scroll_down.value() : 0xff);
	}

	return l_result;
}

std::vector<byte> fe::Chunk::get_palette_attribute_bytes(void) const {
	std::vector<byte> l_result;

	for (const auto& mt : m_metatiles)
		l_result.push_back(
			(mt.m_attr_tl) +
			(mt.m_attr_tr << 2) +
			(mt.m_attr_bl << 4) +
			(mt.m_attr_br << 6)
		);

	return l_result;
}

std::vector<byte> fe::Chunk::get_metatile_top_left_bytes(void) const {
	return get_metatile_quadrant_bytes(0, 0);
}

std::vector<byte> fe::Chunk::get_metatile_top_right_bytes(void) const {
	return get_metatile_quadrant_bytes(1, 0);
}

std::vector<byte> fe::Chunk::get_metatile_bottom_left_bytes(void) const {
	return get_metatile_quadrant_bytes(0, 1);
}

std::vector<byte> fe::Chunk::get_metatile_bottom_right_bytes(void) const {
	return get_metatile_quadrant_bytes(1, 1);
}

// a little complicated - we need to build the door entry and destination
// tables simultaneously. if normalization is enabled for this world,
// we dynamically reduce the same-world range and later patch the asm
// subtract immediate accordingly
fe::DoorEncodeResult fe::Chunk::get_door_bytes(std::size_t p_world_no,
	bool p_normalization_enabled) const {

	constexpr bool DEDUP_DOOR_DEST_DATA{ true };

	std::vector<byte> l_door_bytes;
	std::vector<std::vector<byte>> sw_dests, bld_dests;

	// offset into l_door_bytes + corresponding same-world slot
	std::vector<std::pair<std::size_t, std::size_t>> sw_param_fixups;

	const auto intern_dest = [](std::vector<std::vector<byte>>& dests,
		const std::vector<byte>& dest) -> std::size_t
		{
			// linear search — totally fine for small data like this
			if constexpr (DEDUP_DOOR_DEST_DATA) {
				for (std::size_t i = 0; i < dests.size(); ++i) {
					if (dests[i] == dest)
						return i;
				}
			}

			// not found - append and return index
			dests.push_back(dest);
			return dests.size() - 1;
		};

	for (std::size_t scr{ 0 }; scr < m_screens.size(); ++scr) {
		for (std::size_t d{ 0 }; d < m_screens[scr].m_doors.size(); ++d) {

			const auto& l_door{ m_screens[scr].m_doors[d] };

			// first byte of door data is screen id
			l_door_bytes.push_back(static_cast<byte>(scr));

			// second byte is location (y*16 + x)
			l_door_bytes.push_back(
				l_door.m_coords.first + 16 * l_door.m_coords.second);

			// third byte depends on door type
			if (l_door.m_door_type == fe::DoorType::NextWorld)
				l_door_bytes.push_back(0xff);

			else if (l_door.m_door_type == fe::DoorType::PrevWorld)
				l_door_bytes.push_back(0xfe);

			else if (l_door.m_door_type == fe::DoorType::SameWorld) {

				// generate the same-world destination
				std::vector<byte> sw_dest{
					l_door.m_dest_screen_id,
					l_door.m_dest_palette_id,
					l_door.m_requirement,
					l_door.m_unknown };

				std::size_t sw_slot{
					intern_dest(sw_dests, sw_dest) };

				// reserve param byte for later patching
				sw_param_fixups.emplace_back(
					l_door_bytes.size(), sw_slot);

				l_door_bytes.push_back(0);
			}

			// we necessarily have a door to building
			else {

				// generate the other-world destination
				std::vector<byte> bld_dest{
					l_door.m_npc_bundle,
					l_door.m_dest_screen_id,
					l_door.m_requirement,
					l_door.m_unknown };

				std::size_t bld_slot{
					intern_dest(bld_dests, bld_dest) };

				l_door_bytes.push_back(
					static_cast<byte>(bld_slot + 0x20));
			}

			// fourth byte is destination location (y*16 + x)
			l_door_bytes.push_back(
				l_door.m_dest_coords.first
				+ 16 * l_door.m_dest_coords.second);
		}
	}

	std::size_t sub{ 0 };

	if (p_normalization_enabled)
		sub = 0x20 - sw_dests.size();

	std::size_t sw_capacity{ 0x20 - sub };
	std::size_t bld_capacity{ 0x40 - sw_capacity };

	if (sw_dests.size() > sw_capacity)
		throw std::runtime_error(std::format(
			"World {} exceeds limit of {} unique same-world door destinations",
			p_world_no, sw_capacity));

	if (bld_dests.size() > bld_capacity)
		throw std::runtime_error(std::format(
			"World {} exceeds limit of {} unique door-to-building destinations",
			p_world_no, bld_capacity));

	// now that final sub is known, patch the reserved param bytes
	for (const auto& fixup : sw_param_fixups)
		l_door_bytes.at(fixup.first) =
		static_cast<byte>(fixup.second + sub);

	std::vector<byte> dest_bytes;

	for (const auto& sw_dest_bytes : sw_dests)
		dest_bytes.insert(
			end(dest_bytes),
			begin(sw_dest_bytes),
			end(sw_dest_bytes));

	// pad with 0xff until we hit the lowest door-to-building index
	if (!bld_dests.empty())
		while (dest_bytes.size() < 4 * sw_capacity)
			dest_bytes.push_back(0xff);

	// append the destination entries for doors to buildings
	for (const auto& bld_dest_bytes : bld_dests)
		dest_bytes.insert(
			end(dest_bytes),
			begin(bld_dest_bytes),
			end(bld_dest_bytes));

	// 0xff delimiter for the door data
	l_door_bytes.push_back(0xff);

	return {
		std::move(l_door_bytes),
		std::move(dest_bytes),
		p_normalization_enabled ?
		std::optional<byte>{ static_cast<byte>(sub) } :
		std::nullopt
	};
}

std::vector<byte> fe::Chunk::get_sameworld_transition_bytes(void) const {
	std::vector<byte> l_result;

	for (std::size_t i{ 0 }; i < m_screens.size(); ++i) {
		if (m_screens[i].m_interchunk_scroll.has_value()) {
			const auto& l_val{ m_screens[i].m_interchunk_scroll.value() };

			l_result.push_back(static_cast<byte>(i));
			l_result.push_back(l_val.m_dest_screen);
			l_result.push_back(l_val.m_dest_y * 16 + l_val.m_dest_x);
			l_result.push_back(l_val.m_palette_id);
		}
	}

	l_result.push_back(0xff);
	return l_result;
}

// need to know the chunk remapping to generate this data
std::vector<byte> fe::Chunk::get_otherworld_transition_bytes(void) const {
	std::vector<byte> l_result;

	for (std::size_t i{ 0 }; i < m_screens.size(); ++i) {
		if (m_screens[i].m_intrachunk_scroll.has_value()) {
			const auto& l_val{ m_screens[i].m_intrachunk_scroll.value() };

			l_result.push_back(static_cast<byte>(i));
			l_result.push_back(static_cast<byte>(l_val.m_dest_chunk));
			l_result.push_back(l_val.m_dest_screen);
			l_result.push_back(l_val.m_dest_y * 16 + l_val.m_dest_x);
			l_result.push_back(l_val.m_palette_id);
		}
	}

	l_result.push_back(0xff);
	return l_result;
}
