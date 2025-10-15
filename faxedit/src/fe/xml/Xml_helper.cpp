#include "Xml_helper.h"
#include "Xml_constants.h"
#include <stdexcept>

void fe::xml::save_xml(const std::string p_filepath, const fe::Game& p_game) {

	// create document object
	pugi::xml_document doc;

	// add comments about editor
	auto n_comments = doc.append_child(pugi::node_comment);
	n_comments.set_value(c::COMMENTS_ROOT);

	auto n_metadata = doc.append_child(c::TAG_ROOT);
	n_metadata.append_attribute(c::ATTR_ROOT_VERSION);
	n_metadata.attribute(c::ATTR_ROOT_VERSION).set_value(c::VAL_ROOT_VERSION);

	// game metadata

	// for each palette
	auto n_palettes{ n_metadata.append_child(c::TAG_PALETTES) };

	for (std::size_t i{ 0 }; i < p_game.m_palettes.size(); ++i) {
		auto n_palette{ n_palettes.append_child(c::TAG_PALETTE) };
		n_palette.append_attribute(c::ATTR_NO);
		n_palette.attribute(c::ATTR_NO).set_value(i);
		n_palette.append_attribute(c::ATTR_BYTES);
		n_palette.attribute(c::ATTR_BYTES).set_value(join_bytes(p_game.m_palettes[i], true).c_str());
	}

	// for each chunk
	auto n_chunks{ n_metadata.append_child(c::TAG_CHUNKS) };

	for (std::size_t i{ 0 }; i < p_game.m_chunks.size(); ++i) {
		const auto& lc_chunk{ p_game.m_chunks[i] };

		auto n_chunk{ n_chunks.append_child(c::TAG_CHUNK) };

		n_chunk.append_attribute(c::ATTR_NO);
		n_chunk.attribute(c::ATTR_NO).set_value(i);

		// chunk metadata
		n_chunk.append_attribute(c::ATTR_DEFAULT_PALETTE);
		n_chunk.attribute(c::ATTR_DEFAULT_PALETTE).set_value(byte_to_hex(lc_chunk.m_default_palette_no));

		// chunk door connections, if it exists
		const auto& lc_chunk_door_conns{ lc_chunk.m_door_connections };

		if (lc_chunk_door_conns.has_value()) {

			auto n_door_conns{ n_chunk.append_child(c::TAG_CHUNK_DOOR_CONN) };

			auto n_next_chunk{ n_door_conns.append_child(c::TAG_NEXT_CHUNK) };
			n_next_chunk.append_attribute(c::ATTR_CHUNK_ID);
			n_next_chunk.attribute(c::ATTR_CHUNK_ID).set_value(byte_to_hex(lc_chunk_door_conns.value().m_next_chunk));
			n_next_chunk.append_attribute(c::ATTR_SCREEN_ID);
			n_next_chunk.attribute(c::ATTR_SCREEN_ID).set_value(byte_to_hex(lc_chunk_door_conns.value().m_next_screen));
			n_next_chunk.append_attribute(c::ATTR_REQUIREMENT);
			n_next_chunk.attribute(c::ATTR_REQUIREMENT).set_value(byte_to_hex(lc_chunk_door_conns.value().m_next_door_req));

			auto n_prev_chunk{ n_door_conns.append_child(c::TAG_PREV_CHUNK) };
			n_prev_chunk.append_attribute(c::ATTR_CHUNK_ID);
			n_prev_chunk.attribute(c::ATTR_CHUNK_ID).set_value(byte_to_hex(lc_chunk_door_conns.value().m_prev_chunk));
			n_prev_chunk.append_attribute(c::ATTR_SCREEN_ID);
			n_prev_chunk.attribute(c::ATTR_SCREEN_ID).set_value(byte_to_hex(lc_chunk_door_conns.value().m_prev_screen));
			n_prev_chunk.append_attribute(c::ATTR_REQUIREMENT);
			n_prev_chunk.attribute(c::ATTR_REQUIREMENT).set_value(byte_to_hex(lc_chunk_door_conns.value().m_prev_door_req));
		}

		// for each metatile

		auto n_metatiles{ n_chunk.append_child(c::TAG_METATILES) };

		for (std::size_t mt{ 0 }; mt < lc_chunk.m_metatiles.size(); ++mt) {
			const auto& lc_metatile{ lc_chunk.m_metatiles[mt] };

			auto n_metatile{ n_metatiles.append_child(c::TAG_METATILE) };
			n_metatile.append_attribute(c::ATTR_NO);
			n_metatile.attribute(c::ATTR_NO).set_value(mt);

			n_metatile.append_attribute(c::ATTR_MT_PROPERTY);
			n_metatile.attribute(c::ATTR_MT_PROPERTY).set_value(byte_to_hex(lc_chunk.m_block_properties.at(i)));


			// the metatile tilemap is just 4 tiles, so we just flatten the tilemap and push the bytes
			const auto& lc_mt_tm{ lc_metatile.m_tilemap };

			n_metatile.append_attribute(c::TAG_TILEMAP);
			n_metatile.attribute(c::TAG_TILEMAP).set_value(
				join_bytes(
					{ lc_mt_tm.at(0).at(0),
					lc_mt_tm.at(0).at(1),
					lc_mt_tm.at(1).at(0),
					lc_mt_tm.at(1).at(1) }
					, true)
			);

			n_metatile.append_attribute(c::ATTR_MT_PAL_TL);
			n_metatile.attribute(c::ATTR_MT_PAL_TL).set_value(byte_to_hex(lc_metatile.m_attr_tl));
			n_metatile.append_attribute(c::ATTR_MT_PAL_TR);
			n_metatile.attribute(c::ATTR_MT_PAL_TR).set_value(byte_to_hex(lc_metatile.m_attr_tr));
			n_metatile.append_attribute(c::ATTR_MT_PAL_BL);
			n_metatile.attribute(c::ATTR_MT_PAL_BL).set_value(byte_to_hex(lc_metatile.m_attr_bl));
			n_metatile.append_attribute(c::ATTR_MT_PAL_BR);
			n_metatile.attribute(c::ATTR_MT_PAL_BR).set_value(byte_to_hex(lc_metatile.m_attr_br));
		}

		auto n_screens{ n_chunk.append_child(c::TAG_SCREENS) };

		// for each screen

		for (std::size_t s{ 0 }; s < lc_chunk.m_screens.size(); ++s) {

			const auto& lc_screen{ lc_chunk.m_screens[s] };

			auto n_screen{ n_screens.append_child(c::TAG_SCREEN) };
			n_screen.append_attribute(c::ATTR_NO);
			n_screen.attribute(c::ATTR_NO).set_value(s);

			// screen metadata

			// regular scrolling
			if (lc_screen.m_scroll_left.has_value()) {
				n_screen.append_attribute(c::ATTR_SCREEN_ID_LEFT);
				n_screen.attribute(c::ATTR_SCREEN_ID_LEFT).set_value(lc_screen.m_scroll_left.value());
			}
			if (lc_screen.m_scroll_right.has_value()) {
				n_screen.append_attribute(c::ATTR_SCREEN_ID_RIGHT);
				n_screen.attribute(c::ATTR_SCREEN_ID_RIGHT).set_value(lc_screen.m_scroll_right.value());
			}
			if (lc_screen.m_scroll_up.has_value()) {
				n_screen.append_attribute(c::ATTR_SCREEN_ID_UP);
				n_screen.attribute(c::ATTR_SCREEN_ID_UP).set_value(lc_screen.m_scroll_up.value());
			}
			if (lc_screen.m_scroll_down.has_value()) {
				n_screen.append_attribute(c::ATTR_SCREEN_ID_DOWN);
				n_screen.attribute(c::ATTR_SCREEN_ID_DOWN).set_value(lc_screen.m_scroll_down.value());
			}

			// inter-chunk scrolling
			if (lc_screen.m_interchunk_scroll.has_value()) {
				auto n_sinter_s{ n_screen.append_child(c::TAG_SCREEN_INTERCHUNK_TRANSTION) };

				n_sinter_s.append_attribute(c::ATTR_CHUNK_ID);
				n_sinter_s.attribute(c::ATTR_CHUNK_ID).set_value(lc_screen.m_interchunk_scroll.value().m_dest_chunk);
				n_sinter_s.append_attribute(c::ATTR_DEST_SCREEN_NO);
				n_sinter_s.attribute(c::ATTR_DEST_SCREEN_NO).set_value(lc_screen.m_interchunk_scroll.value().m_dest_screen);
				n_sinter_s.append_attribute(c::ATTR_DEST_X);
				n_sinter_s.attribute(c::ATTR_DEST_X).set_value(lc_screen.m_interchunk_scroll.value().m_dest_x);
				n_sinter_s.append_attribute(c::ATTR_DEST_Y);
				n_sinter_s.attribute(c::ATTR_DEST_Y).set_value(lc_screen.m_interchunk_scroll.value().m_dest_y);
				n_sinter_s.append_attribute(c::ATTR_DEST_PALETTE);
				n_sinter_s.attribute(c::ATTR_DEST_PALETTE).set_value(lc_screen.m_interchunk_scroll.value().m_palette_id);
			}

			// intra-chunk scrolling
			if (lc_screen.m_intrachunk_scroll.has_value()) {
				auto n_sinter_s{ n_screen.append_child(c::TAG_SCREEN_INTRACHUNK_TRANSTION) };

				n_sinter_s.append_attribute(c::ATTR_DEST_SCREEN_NO);
				n_sinter_s.attribute(c::ATTR_DEST_SCREEN_NO).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_screen);
				n_sinter_s.append_attribute(c::ATTR_DEST_X);
				n_sinter_s.attribute(c::ATTR_DEST_X).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_x);
				n_sinter_s.append_attribute(c::ATTR_DEST_Y);
				n_sinter_s.attribute(c::ATTR_DEST_Y).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_y);
				n_sinter_s.append_attribute(c::ATTR_DEST_PALETTE);
				n_sinter_s.attribute(c::ATTR_DEST_PALETTE).set_value(lc_screen.m_intrachunk_scroll.value().m_palette_id);
			}

			// tilemap
			auto n_screen_tilemap{ n_screen.append_child(c::TAG_TILEMAP) };

			for (std::size_t stc{ 0 }; stc < lc_screen.m_tilemap.size(); ++stc) {
				auto n_str{ n_screen_tilemap.append_child(c::TAG_COL) };

				n_str.append_attribute(c::ATTR_NO);
				n_str.attribute(c::ATTR_NO).set_value(stc);

				n_str.append_attribute(c::ATTR_BYTES);
				n_str.attribute(c::ATTR_BYTES).set_value(join_bytes(lc_screen.m_tilemap[stc], true));
			}

			// for each sprite, if any
			if (!p_game.m_chunks[i].m_screens[s].m_sprites.empty()) {

				auto n_screen_sprites{ n_screen.append_child(c::TAG_SPRITES) };

				for (std::size_t sp{ 0 }; sp < p_game.m_chunks[i].m_screens[s].m_sprites.size(); ++sp) {
					const auto& lc_sprite{ p_game.m_chunks[i].m_screens[s].m_sprites[sp] };

					auto n_sprite{ n_screen_sprites.append_child(c::TAG_SPRITE) };
					n_sprite.append_attribute(c::ATTR_NO);
					n_sprite.attribute(c::ATTR_NO).set_value(sp);

					n_sprite.append_attribute(c::ATTR_ID);
					n_sprite.attribute(c::ATTR_ID).set_value(byte_to_hex(lc_sprite.m_id));

					n_sprite.append_attribute(c::ATTR_X);
					n_sprite.attribute(c::ATTR_X).set_value(lc_sprite.m_x);

					n_sprite.append_attribute(c::ATTR_Y);
					n_sprite.attribute(c::ATTR_Y).set_value(lc_sprite.m_y);

					if (lc_sprite.m_text_id.has_value()) {
						n_sprite.append_attribute(c::ATTR_TEXT_ID);
						n_sprite.attribute(c::ATTR_TEXT_ID).set_value(byte_to_hex(lc_sprite.m_text_id.value()));
					}

				}
			}

			// for each door, if any
			if (!p_game.m_chunks[i].m_screens[s].m_doors.empty()) {
				auto n_screen_doors{ n_screen.append_child(c::TAG_DOORS) };

				for (std::size_t door{ 0 }; door < p_game.m_chunks[i].m_screens[s].m_doors.size(); ++door) {

					const auto& lc_door{ p_game.m_chunks[i].m_screens[s].m_doors[door] };

					auto n_door{ n_screen_doors.append_child(c::TAG_DOOR) };
					n_door.append_attribute(c::ATTR_NO);
					n_door.attribute(c::ATTR_NO).set_value(door);

					n_door.append_attribute(c::ATTR_TYPE);

					if (lc_door.m_door_type == fe::DoorType::SameWorld)
						n_door.attribute(c::ATTR_TYPE).set_value(c::VAL_DOOR_TYPE_INTERCHUNK);
					else if (lc_door.m_door_type == fe::DoorType::Building)
						n_door.attribute(c::ATTR_TYPE).set_value(c::VAL_DOOR_TYPE_BUILDING);
					else if (lc_door.m_door_type == fe::DoorType::PrevWorld)
						n_door.attribute(c::ATTR_TYPE).set_value(c::VAL_DOOR_TYPE_PREVCHUNK);
					else
						n_door.attribute(c::ATTR_TYPE).set_value(c::VAL_DOOR_TYPE_NEXTCHUNK);

					n_door.append_attribute(c::ATTR_X);
					n_door.attribute(c::ATTR_X).set_value(lc_door.m_coords.first);

					n_door.append_attribute(c::ATTR_Y);
					n_door.attribute(c::ATTR_Y).set_value(lc_door.m_coords.second);

					n_door.append_attribute(c::ATTR_DEST_X);
					n_door.attribute(c::ATTR_DEST_X).set_value(lc_door.m_dest_coords.first);

					n_door.append_attribute(c::ATTR_DEST_Y);
					n_door.attribute(c::ATTR_DEST_Y).set_value(lc_door.m_dest_coords.second);

					if (lc_door.m_door_type == fe::DoorType::SameWorld || lc_door.m_door_type == fe::DoorType::Building) {
						n_door.append_attribute(c::ATTR_DEST_SCREEN_NO);
						n_door.attribute(c::ATTR_DEST_SCREEN_NO).set_value(lc_door.m_dest_screen_id);

						if (lc_door.m_door_type == fe::DoorType::SameWorld) {
							n_door.append_attribute(c::ATTR_DEST_PALETTE);
							n_door.attribute(c::ATTR_DEST_PALETTE).set_value(byte_to_hex(lc_door.m_dest_palette_id));
						}
						else {
							n_door.append_attribute(c::ATTR_DEST_PARAM_ID);
							n_door.attribute(c::ATTR_DEST_PARAM_ID).set_value(byte_to_hex(lc_door.m_npc_bundle));
						}

						n_door.append_attribute(c::ATTR_UNKNOWN_BYTE);
						n_door.attribute(c::ATTR_UNKNOWN_BYTE).set_value(byte_to_hex(lc_door.m_unknown));
					}

				}
			}

		}
	}

	// save document to disk
	if (!doc.save_file(p_filepath.c_str()))
		throw std::runtime_error("Could not save " + p_filepath);
}

std::string fe::xml::join_bytes(const std::vector<byte>& p_bytes, bool p_hex) {
	if (p_bytes.empty())
		return std::string();
	else {

		std::string result = (p_hex ? byte_to_hex(p_bytes[0]) : std::to_string(p_bytes[0]));

		for (size_t i = 1; i < p_bytes.size(); ++i)
			result += "," + (p_hex ? byte_to_hex(p_bytes[i]) : std::to_string(p_bytes[i]));

		return result;

	}
}

std::string fe::xml::byte_to_hex(byte p_byte) {
	constexpr char hex_chars[] = "0123456789abcdef";
	std::string out = "0x";
	out += hex_chars[(p_byte >> 4) & 0xF];
	out += hex_chars[p_byte & 0xF];
	return out;
}
