#include "Xml_helper.h"
#include "Xml_constants.h"
#include <stdexcept>

using byte = unsigned char;

fe::Game fe::xml::load_xml(const std::string p_filepath) {
	fe::Game l_game;

	pugi::xml_document l_doc;
	if (!l_doc.load_file(p_filepath.c_str()))
		throw std::runtime_error("Could not load xml file " + p_filepath);

	auto n_root{ l_doc.child(c::TAG_ROOT) };

	// Read version and be backward-compatible when the format changes

	// extract game-level metadata

	// stage definitions
	auto n_stages{ n_root.child(c::TAG_STAGES) };
	{
		byte l_start_scr{ parse_numeric_byte(n_stages.attribute(c::ATTR_SCREEN_ID).as_string()) };
		byte l_start_x{ parse_numeric_byte(n_stages.attribute(c::ATTR_X).as_string()) };
		byte l_start_y{ parse_numeric_byte(n_stages.attribute(c::ATTR_Y).as_string()) };
		byte l_start_hp{ parse_numeric_byte(n_stages.attribute(c::ATTR_HP).as_string()) };

		std::vector<fe::Stage> l_stages;
		for (auto n_stage{ n_stages.child(c::TAG_STAGE) }; n_stage;
			n_stage = n_stage.next_sibling(c::TAG_STAGE)) {
			l_stages.push_back(fe::Stage(
				parse_numeric_byte(n_stage.attribute(c::ATTR_CHUNK_ID).as_string()),
				parse_numeric_byte(n_stage.attribute(c::ATTR_PREV_STAGE).as_string()),
				parse_numeric_byte(n_stage.attribute(c::ATTR_NEXT_STAGE).as_string()),
				parse_numeric_byte(n_stage.attribute(c::ATTR_PREV_SCREEN).as_string()),
				parse_numeric_byte(n_stage.attribute(c::ATTR_NEXT_SCREEN).as_string()),
				parse_numeric_byte(n_stage.attribute(c::ATTR_PREV_REQ).as_string()),
				parse_numeric_byte(n_stage.attribute(c::ATTR_NEXT_REQ).as_string())
			));
		}

		l_game.m_stages = fe::StageManager(l_start_scr, l_start_x, l_start_y,
			l_start_hp, l_stages);
	}

	// extract palettes
	auto n_palettes = n_root.child(c::TAG_PALETTES);
	for (auto n_palette{ n_palettes.child(c::TAG_PALETTE) }; n_palette;
		n_palette = n_palette.next_sibling(c::TAG_PALETTE)) {
		l_game.m_palettes.push_back(parse_byte_list(n_palette.attribute(c::ATTR_BYTES).as_string()));
	}

	// extract building parameters
	auto n_bparams = n_root.child(c::TAG_NPC_BUNDLES);
	for (auto n_bparam{ n_bparams.child(c::TAG_NPC_BUNDLE) }; n_bparam;
		n_bparam = n_bparam.next_sibling(c::TAG_NPC_BUNDLE)) {

		fe::Sprite_set l_sprite_set;

		if (n_bparam.attribute(c::ATTR_SPRITE_COMM_BYTE))
			l_sprite_set.m_command_byte = parse_numeric_byte(n_bparam.attribute(c::ATTR_SPRITE_COMM_BYTE).as_string());

		for (auto n_bld_sprite{ n_bparam.child(c::TAG_SPRITE) }; n_bld_sprite;
			n_bld_sprite = n_bld_sprite.next_sibling(c::TAG_SPRITE)) {

			fe::Sprite l_sprite(
				parse_numeric_byte(n_bld_sprite.attribute(c::ATTR_ID).as_string()),
				parse_numeric_byte(n_bld_sprite.attribute(c::ATTR_X).as_string()),
				parse_numeric_byte(n_bld_sprite.attribute(c::ATTR_Y).as_string())
			);

			if (n_bld_sprite.attribute(c::ATTR_TEXT_ID))
				l_sprite.m_text_id = parse_numeric_byte(n_bld_sprite.attribute(c::ATTR_TEXT_ID).as_string());

			l_sprite_set.m_sprites.push_back(l_sprite);
		}

		l_game.m_npc_bundles.push_back(l_sprite_set);
	}

	// extract spawn points
	auto n_spawns = n_root.child(c::TAG_SPAWN_POINTS);

	// remain backward compatible and set building sprite set
	// value here if missing (beta-2 and earlier did not have this parameter)
	const std::vector<byte> lc_spawn_sprite_sets{
		0x02, 0x0b, 0x10, 0x1e, 0x23, 0x2b, 0x33, 0x3c
	};
	std::size_t spawnspritesetno{ 0 };

	for (auto n_spawn{ n_spawns.child(c::TAG_SPAWN_POINT) }; n_spawn;
		n_spawn = n_spawn.next_sibling(c::TAG_SPAWN_POINT)) {

		byte l_sprite_set{ n_spawn.attribute(c::TAG_NPC_BUNDLE) ?
			parse_numeric_byte(n_spawn.attribute(c::TAG_NPC_BUNDLE).as_string()) :
			lc_spawn_sprite_sets.at(spawnspritesetno)
		};

		l_game.m_spawn_locations.push_back(fe::Spawn_location(
			parse_numeric_byte(n_spawn.attribute(c::ATTR_CHUNK_ID).as_string()),
			parse_numeric_byte(n_spawn.attribute(c::ATTR_SCREEN_ID).as_string()),
			parse_numeric_byte(n_spawn.attribute(c::ATTR_STAGE_ID).as_string()),
			parse_numeric_byte(n_spawn.attribute(c::ATTR_X).as_string()),
			parse_numeric_byte(n_spawn.attribute(c::ATTR_Y).as_string()),
			l_sprite_set
		));
		++spawnspritesetno;
	}

	// extract push-block parameters
	auto n_push_block{ n_root.child(c::TAG_PUSH_BLOCK) };

	l_game.m_push_block = fe::Push_block_parameters(
		parse_numeric_byte(n_push_block.attribute(c::ATTR_STAGE_ID).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_SCREEN_ID).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_X).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_Y).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_BLOCK_COUNT).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_SOURCE_BLOCK0).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_SOURCE_BLOCK1).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_TARGET_BLOCK0).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_TARGET_BLOCK1).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_DELTA_X).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_DRAW_BLOCK).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_COVER_X).as_string()),
		parse_numeric_byte(n_push_block.attribute(c::ATTR_COVER_Y).as_string())
	);

	// check if attribute exists, to be backward compatible with beta-1
	// we make sure the default constructor populates the vector if not
	auto n_jump_on = n_root.child(c::TAG_JUMP_ON_ANIMATION);
	if (n_jump_on)
		l_game.m_jump_on_animation = parse_byte_list(
			n_jump_on.attribute(c::TAG_METATILES).as_string()
		);

	// if building scenes are not set (pre beta-5), extract them from ROM
	// after the import
	auto n_bscenes = n_root.child(c::TAG_BUILDING_SCENES);
	if (n_bscenes) {
		for (auto n_scene{ n_bscenes.child(c::TAG_SCENE) }; n_scene;
			n_scene = n_scene.next_sibling(c::TAG_SCENE)) {

			l_game.m_building_scenes.push_back(fe::Scene(
				parse_numeric(n_scene.attribute(c::ATTR_DEFAULT_PALETTE).as_string()),
				parse_numeric(n_scene.attribute(c::ATTR_TILESET).as_string()),
				parse_numeric(n_scene.attribute(c::ATTR_MUSIC).as_string()),
				parse_numeric_byte(n_scene.attribute(c::ATTR_X).as_string()),
				parse_numeric_byte(n_scene.attribute(c::ATTR_Y).as_string())
			));
		}
	}

	// extract chunks
	auto n_chunks = n_root.child(c::TAG_CHUNKS);
	for (auto n_chunk{ n_chunks.child(c::TAG_CHUNK) }; n_chunk;
		n_chunk = n_chunk.next_sibling(c::TAG_CHUNK)) {

		fe::Chunk l_chunk;

		// extract chunk-level metadata

		// chunk scene values
		// if any are missing (pre beta-5) they will be loaded from ROM after the xml load
		l_chunk.m_scene.m_palette = parse_numeric_byte(n_chunk.attribute(c::ATTR_DEFAULT_PALETTE).as_string());

		auto n_scene_tileset{ n_chunk.attribute(c::ATTR_TILESET) };
		if (n_scene_tileset)
			l_chunk.m_scene.m_tileset = parse_numeric_byte(n_chunk.attribute(c::ATTR_TILESET).as_string());

		auto n_scene_music{ n_chunk.attribute(c::ATTR_MUSIC) };
		if (n_scene_music)
			l_chunk.m_scene.m_music = parse_numeric_byte(n_chunk.attribute(c::ATTR_MUSIC).as_string());

		auto n_scene_x{ n_chunk.attribute(c::ATTR_X) };
		if (n_scene_x)
			l_chunk.m_scene.m_x = parse_numeric_byte(n_chunk.attribute(c::ATTR_X).as_string());

		auto n_scene_y{ n_chunk.attribute(c::ATTR_Y) };
		if (n_scene_y)
			l_chunk.m_scene.m_y = parse_numeric_byte(n_chunk.attribute(c::ATTR_Y).as_string());

		// mattock animation
		l_chunk.m_mattock_animation = parse_byte_list(n_chunk.attribute(c::ATTR_MATTOCK_ANIMATION).as_string());

		// chunk metatile definitions
		auto n_metatiles{ n_chunk.child(c::TAG_METATILES) };
		for (auto n_metatile{ n_metatiles.child(c::TAG_METATILE) }; n_metatile;
			n_metatile = n_metatile.next_sibling(c::TAG_METATILE)) {
			const auto l_tilemap_bytes{ parse_byte_list(n_metatile.attribute(c::TAG_TILEMAP).as_string()) };

			l_chunk.m_metatiles.push_back(fe::Metatile(
				l_tilemap_bytes.at(0),
				l_tilemap_bytes.at(1),
				l_tilemap_bytes.at(2),
				l_tilemap_bytes.at(3),
				parse_numeric_byte(n_metatile.attribute(c::ATTR_MT_PAL_TL).as_string()),
				parse_numeric_byte(n_metatile.attribute(c::ATTR_MT_PAL_TR).as_string()),
				parse_numeric_byte(n_metatile.attribute(c::ATTR_MT_PAL_BL).as_string()),
				parse_numeric_byte(n_metatile.attribute(c::ATTR_MT_PAL_BR).as_string()),
				parse_numeric_byte(n_metatile.attribute(c::ATTR_MT_PROPERTY).as_string())
			));
		}

		// parse screens

		auto n_screens{ n_chunk.child(c::TAG_SCREENS) };
		for (auto n_screen{ n_screens.child(c::TAG_SCREEN) }; n_screen; n_screen = n_screen.next_sibling(c::TAG_SCREEN)) {
			fe::Screen l_screen;

			// screen metadata

			// scrolling
			if (n_screen.attribute(c::ATTR_SCREEN_ID_LEFT))
				l_screen.m_scroll_left = parse_numeric_byte(n_screen.attribute(c::ATTR_SCREEN_ID_LEFT).as_string());
			if (n_screen.attribute(c::ATTR_SCREEN_ID_RIGHT))
				l_screen.m_scroll_right = parse_numeric_byte(n_screen.attribute(c::ATTR_SCREEN_ID_RIGHT).as_string());
			if (n_screen.attribute(c::ATTR_SCREEN_ID_UP))
				l_screen.m_scroll_up = parse_numeric_byte(n_screen.attribute(c::ATTR_SCREEN_ID_UP).as_string());
			if (n_screen.attribute(c::ATTR_SCREEN_ID_DOWN))
				l_screen.m_scroll_down = parse_numeric_byte(n_screen.attribute(c::ATTR_SCREEN_ID_DOWN).as_string());

			// optional command byte originating from the sprite data
			if (n_screen.attribute(c::ATTR_SPRITE_COMM_BYTE))
				l_screen.m_sprite_set.m_command_byte = parse_numeric_byte(n_screen.attribute(c::ATTR_SPRITE_COMM_BYTE).as_string());

			// optional sameworld-transition override
			auto n_scr_sw_trans{ n_screen.child(c::TAG_SCREEN_INTERCHUNK_TRANSTION) };
			if (n_scr_sw_trans)
				l_screen.m_interchunk_scroll = fe::InterChunkScroll(
					parse_numeric_byte(n_scr_sw_trans.attribute(c::ATTR_DEST_SCREEN_NO).as_string()),
					parse_numeric_byte(n_scr_sw_trans.attribute(c::ATTR_DEST_X).as_string()),
					parse_numeric_byte(n_scr_sw_trans.attribute(c::ATTR_DEST_Y).as_string()),
					parse_numeric_byte(n_scr_sw_trans.attribute(c::ATTR_DEST_PALETTE).as_string()));

			// optional otherworld-transition override
			auto n_scr_ow_trans{ n_screen.child(c::TAG_SCREEN_INTRACHUNK_TRANSTION) };
			if (n_scr_ow_trans)
				l_screen.m_intrachunk_scroll = fe::IntraChunkScroll(
					parse_numeric_byte(n_scr_ow_trans.attribute(c::ATTR_CHUNK_ID).as_string()),
					parse_numeric_byte(n_scr_ow_trans.attribute(c::ATTR_DEST_SCREEN_NO).as_string()),
					parse_numeric_byte(n_scr_ow_trans.attribute(c::ATTR_DEST_X).as_string()),
					parse_numeric_byte(n_scr_ow_trans.attribute(c::ATTR_DEST_Y).as_string()),
					parse_numeric_byte(n_scr_ow_trans.attribute(c::ATTR_DEST_PALETTE).as_string()));

			auto n_scr_tilemap{ n_screen.child(c::TAG_TILEMAP) };
			for (auto n_scr_tl_col{ n_scr_tilemap.child(c::TAG_COL) }; n_scr_tl_col; n_scr_tl_col = n_scr_tl_col.next_sibling(c::TAG_COL))
				l_screen.m_tilemap.push_back(parse_byte_list(n_scr_tl_col.attribute(c::ATTR_BYTES).as_string()));

			// screen sprites
			auto n_sprites{ n_screen.child(c::TAG_SPRITES) };
			for (auto n_sprite{ n_sprites.child(c::TAG_SPRITE) }; n_sprite; n_sprite = n_sprite.next_sibling(c::TAG_SPRITE)) {
				if (n_sprite.attribute(c::ATTR_TEXT_ID))
					l_screen.m_sprite_set.push_back(fe::Sprite(
						parse_numeric_byte(n_sprite.attribute(c::ATTR_ID).as_string()),
						parse_numeric_byte(n_sprite.attribute(c::ATTR_X).as_string()),
						parse_numeric_byte(n_sprite.attribute(c::ATTR_Y).as_string()),
						parse_numeric_byte(n_sprite.attribute(c::ATTR_TEXT_ID).as_string())));
				else
					l_screen.m_sprite_set.push_back(fe::Sprite(
						parse_numeric_byte(n_sprite.attribute(c::ATTR_ID).as_string()),
						parse_numeric_byte(n_sprite.attribute(c::ATTR_X).as_string()),
						parse_numeric_byte(n_sprite.attribute(c::ATTR_Y).as_string())
					));
			}

			// screen doors
			auto n_doors{ n_screen.child(c::TAG_DOORS) };
			for (auto n_door{ n_doors.child(c::TAG_DOOR) }; n_door; n_door = n_door.next_sibling(c::TAG_DOOR)) {
				fe::DoorType l_sc_door_type{ text_to_doortype(n_door.attribute(c::ATTR_TYPE).as_string()) };
				byte l_sc_door_x{ parse_numeric_byte(n_door.attribute(c::ATTR_X).as_string()) };
				byte l_sc_door_y{ parse_numeric_byte(n_door.attribute(c::ATTR_Y).as_string()) };
				byte l_sc_door_dest_x{ parse_numeric_byte(n_door.attribute(c::ATTR_DEST_X).as_string()) };
				byte l_sc_door_dest_y{ parse_numeric_byte(n_door.attribute(c::ATTR_DEST_Y).as_string()) };
				byte l_sc_door_dest_screen{ n_door.attribute(c::ATTR_DEST_SCREEN_NO) ? parse_numeric_byte(n_door.attribute(c::ATTR_DEST_SCREEN_NO).as_string()) : static_cast<byte>(0) };
				byte l_sc_door_dest_palette{ n_door.attribute(c::ATTR_DEST_PALETTE) ? parse_numeric_byte(n_door.attribute(c::ATTR_DEST_PALETTE).as_string()) : static_cast<byte>(0) };
				byte l_sc_door_npc_bundle_no{ n_door.attribute(c::ATTR_DEST_PARAM_ID) ? parse_numeric_byte(n_door.attribute(c::ATTR_DEST_PARAM_ID).as_string()) : static_cast<byte>(0) };
				byte l_sc_door_unk_byte{ n_door.attribute(c::ATTR_UNKNOWN_BYTE) ? parse_numeric_byte(n_door.attribute(c::ATTR_UNKNOWN_BYTE).as_string()) : static_cast<byte>(0) };
				byte l_sc_door_req{ n_door.attribute(c::ATTR_REQUIREMENT) ? parse_numeric_byte(n_door.attribute(c::ATTR_REQUIREMENT).as_string()) : static_cast<byte>(0) };

				l_screen.m_doors.push_back(fe::Door(l_sc_door_type, l_sc_door_x, l_sc_door_y, l_sc_door_dest_x, l_sc_door_dest_y, l_sc_door_req,
					l_sc_door_dest_palette, l_sc_door_npc_bundle_no, l_sc_door_dest_screen, l_sc_door_unk_byte));
			}

			l_chunk.m_screens.push_back(l_screen);
		}

		l_game.m_chunks.push_back(l_chunk);
	}

	return l_game;
}

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

	// for each stage
	auto n_stages = n_metadata.append_child(c::TAG_STAGES);

	n_stages.append_attribute(c::ATTR_SCREEN_ID);
	n_stages.attribute(c::ATTR_SCREEN_ID).set_value(p_game.m_stages.m_start_screen);
	n_stages.append_attribute(c::ATTR_X);
	n_stages.attribute(c::ATTR_X).set_value(p_game.m_stages.m_start_x);
	n_stages.append_attribute(c::ATTR_Y);
	n_stages.attribute(c::ATTR_Y).set_value(p_game.m_stages.m_start_y);
	n_stages.append_attribute(c::ATTR_HP);
	n_stages.attribute(c::ATTR_HP).set_value(p_game.m_stages.m_start_hp);

	for (std::size_t i{ 0 }; i < p_game.m_stages.m_stages.size(); ++i) {
		auto n_stage = n_stages.append_child(c::TAG_STAGE);
		n_stage.append_attribute(c::ATTR_NO);
		n_stage.attribute(c::ATTR_NO).set_value(i);
		n_stage.append_attribute(c::ATTR_CHUNK_ID);
		n_stage.attribute(c::ATTR_CHUNK_ID).set_value(p_game.m_stages.m_stages[i].m_world_id);
		n_stage.append_attribute(c::ATTR_PREV_STAGE);
		n_stage.attribute(c::ATTR_PREV_STAGE).set_value(p_game.m_stages.m_stages[i].m_prev_stage);
		n_stage.append_attribute(c::ATTR_PREV_SCREEN);
		n_stage.attribute(c::ATTR_PREV_SCREEN).set_value(p_game.m_stages.m_stages[i].m_prev_screen);
		n_stage.append_attribute(c::ATTR_PREV_REQ);
		n_stage.attribute(c::ATTR_PREV_REQ).set_value(byte_to_hex(p_game.m_stages.m_stages[i].m_prev_requirement));
		n_stage.append_attribute(c::ATTR_NEXT_STAGE);
		n_stage.attribute(c::ATTR_NEXT_STAGE).set_value(p_game.m_stages.m_stages[i].m_next_stage);
		n_stage.append_attribute(c::ATTR_NEXT_SCREEN);
		n_stage.attribute(c::ATTR_NEXT_SCREEN).set_value(p_game.m_stages.m_stages[i].m_next_screen);
		n_stage.append_attribute(c::ATTR_NEXT_REQ);
		n_stage.attribute(c::ATTR_NEXT_REQ).set_value(byte_to_hex(p_game.m_stages.m_stages[i].m_next_requirement));
	}

	// for each palette
	auto n_palettes{ n_metadata.append_child(c::TAG_PALETTES) };

	for (std::size_t i{ 0 }; i < p_game.m_palettes.size(); ++i) {
		auto n_palette{ n_palettes.append_child(c::TAG_PALETTE) };
		n_palette.append_attribute(c::ATTR_NO);
		n_palette.attribute(c::ATTR_NO).set_value(i);
		n_palette.append_attribute(c::ATTR_BYTES);
		n_palette.attribute(c::ATTR_BYTES).set_value(join_bytes(p_game.m_palettes[i], true));
	}

	// for each npc-bundle
	auto n_bundles{ n_metadata.append_child(c::TAG_NPC_BUNDLES) };

	for (std::size_t i{ 0 }; i < p_game.m_npc_bundles.size(); ++i) {
		auto n_bundle{ n_bundles.append_child(c::TAG_NPC_BUNDLE) };

		const auto& l_bundle{ p_game.m_npc_bundles[i] };

		n_bundle.append_attribute(c::ATTR_NO);
		n_bundle.attribute(c::ATTR_NO).set_value(i);

		if (l_bundle.m_command_byte.has_value()) {
			n_bundle.append_attribute(c::ATTR_SPRITE_COMM_BYTE);
			n_bundle.attribute(c::ATTR_SPRITE_COMM_BYTE).set_value(byte_to_hex(l_bundle.m_command_byte.value()));
		}

		for (std::size_t s{ 0 }; s < l_bundle.m_sprites.size(); ++s) {
			auto n_bld_sprite{ n_bundle.append_child(c::TAG_SPRITE) };

			n_bld_sprite.append_attribute(c::ATTR_NO);
			n_bld_sprite.attribute(c::ATTR_NO).set_value(s);

			n_bld_sprite.append_attribute(c::ATTR_ID);
			n_bld_sprite.attribute(c::ATTR_ID).set_value(l_bundle.m_sprites[s].m_id);

			n_bld_sprite.append_attribute(c::ATTR_X);
			n_bld_sprite.attribute(c::ATTR_X).set_value(l_bundle.m_sprites[s].m_x);

			n_bld_sprite.append_attribute(c::ATTR_Y);
			n_bld_sprite.attribute(c::ATTR_Y).set_value(l_bundle.m_sprites[s].m_y);

			if (l_bundle.m_sprites[s].m_text_id.has_value()) {
				n_bld_sprite.append_attribute(c::ATTR_TEXT_ID);
				n_bld_sprite.attribute(c::ATTR_TEXT_ID).set_value(byte_to_hex(l_bundle.m_sprites[s].m_text_id.value()));
			}
		}

	}

	// for each spawn point
	auto n_spawns{ n_metadata.append_child(c::TAG_SPAWN_POINTS) };

	for (std::size_t i{ 0 }; i < p_game.m_spawn_locations.size(); ++i) {
		const auto& l_sl{ p_game.m_spawn_locations[i] };

		auto n_spawn{ n_spawns.append_child(c::TAG_SPAWN_POINT) };

		n_spawn.append_attribute(c::ATTR_NO);
		n_spawn.attribute(c::ATTR_NO).set_value(i);

		n_spawn.append_attribute(c::ATTR_CHUNK_ID);
		n_spawn.attribute(c::ATTR_CHUNK_ID).set_value(l_sl.m_world);
		n_spawn.append_attribute(c::ATTR_STAGE_ID);
		n_spawn.attribute(c::ATTR_STAGE_ID).set_value(l_sl.m_stage);
		n_spawn.append_attribute(c::ATTR_SCREEN_ID);
		n_spawn.attribute(c::ATTR_SCREEN_ID).set_value(l_sl.m_screen);
		n_spawn.append_attribute(c::ATTR_X);
		n_spawn.attribute(c::ATTR_X).set_value(l_sl.m_x);
		n_spawn.append_attribute(c::ATTR_Y);
		n_spawn.attribute(c::ATTR_Y).set_value(l_sl.m_y);
		n_spawn.append_attribute(c::TAG_NPC_BUNDLE);
		n_spawn.attribute(c::TAG_NPC_BUNDLE).set_value(l_sl.m_sprite_set);
	}

	auto n_push_block{ n_metadata.append_child(c::TAG_PUSH_BLOCK) };

	n_push_block.append_attribute(c::ATTR_STAGE_ID);
	n_push_block.attribute(c::ATTR_STAGE_ID).set_value(p_game.m_push_block.m_stage);
	n_push_block.append_attribute(c::ATTR_SCREEN_ID);
	n_push_block.attribute(c::ATTR_SCREEN_ID).set_value(p_game.m_push_block.m_screen);
	n_push_block.append_attribute(c::ATTR_X);
	n_push_block.attribute(c::ATTR_X).set_value(p_game.m_push_block.m_x);
	n_push_block.append_attribute(c::ATTR_Y);
	n_push_block.attribute(c::ATTR_Y).set_value(p_game.m_push_block.m_y);
	n_push_block.append_attribute(c::ATTR_DRAW_BLOCK);
	n_push_block.attribute(c::ATTR_DRAW_BLOCK).set_value(byte_to_hex(p_game.m_push_block.m_draw_block));
	n_push_block.append_attribute(c::ATTR_DELTA_X);
	n_push_block.attribute(c::ATTR_DELTA_X).set_value(p_game.m_push_block.m_pos_delta);
	n_push_block.append_attribute(c::ATTR_BLOCK_COUNT);
	n_push_block.attribute(c::ATTR_BLOCK_COUNT).set_value(p_game.m_push_block.m_block_count);
	n_push_block.append_attribute(c::ATTR_SOURCE_BLOCK0);
	n_push_block.attribute(c::ATTR_SOURCE_BLOCK0).set_value(byte_to_hex(p_game.m_push_block.m_source_0));
	n_push_block.append_attribute(c::ATTR_SOURCE_BLOCK1);
	n_push_block.attribute(c::ATTR_SOURCE_BLOCK1).set_value(byte_to_hex(p_game.m_push_block.m_source_1));
	n_push_block.append_attribute(c::ATTR_TARGET_BLOCK0);
	n_push_block.attribute(c::ATTR_TARGET_BLOCK0).set_value(byte_to_hex(p_game.m_push_block.m_target_0));
	n_push_block.append_attribute(c::ATTR_TARGET_BLOCK1);
	n_push_block.attribute(c::ATTR_TARGET_BLOCK1).set_value(byte_to_hex(p_game.m_push_block.m_target_1));
	n_push_block.append_attribute(c::ATTR_COVER_X);
	n_push_block.attribute(c::ATTR_COVER_X).set_value(p_game.m_push_block.m_cover_x);
	n_push_block.append_attribute(c::ATTR_COVER_Y);
	n_push_block.attribute(c::ATTR_COVER_Y).set_value(p_game.m_push_block.m_cover_y);

	auto n_jump_on{ n_metadata.append_child(c::TAG_JUMP_ON_ANIMATION) };
	n_jump_on.append_attribute(c::TAG_METATILES);
	n_jump_on.attribute(c::TAG_METATILES).set_value(
		join_bytes(p_game.m_jump_on_animation, true)
	);

	auto n_bscenes{ n_metadata.append_child(c::TAG_BUILDING_SCENES) };
	for (std::size_t i{ 0 }; i < p_game.m_building_scenes.size(); ++i) {
		auto n_scene{ n_bscenes.append_child(c::TAG_SCENE) };

		n_scene.append_attribute(c::ATTR_NO);
		n_scene.attribute(c::ATTR_NO).set_value(i);

		n_scene.append_attribute(c::ATTR_DEFAULT_PALETTE);
		n_scene.attribute(c::ATTR_DEFAULT_PALETTE).set_value(
			byte_to_hex(static_cast<byte>(p_game.m_building_scenes[i].m_palette)));

		n_scene.append_attribute(c::ATTR_TILESET);
		n_scene.attribute(c::ATTR_TILESET).set_value(
			byte_to_hex(static_cast<byte>(p_game.m_building_scenes[i].m_tileset)));

		n_scene.append_attribute(c::ATTR_MUSIC);
		n_scene.attribute(c::ATTR_MUSIC).set_value(
			byte_to_hex(static_cast<byte>(p_game.m_building_scenes[i].m_music)));

		n_scene.append_attribute(c::ATTR_X);
		n_scene.attribute(c::ATTR_X).set_value(
			byte_to_hex(p_game.m_building_scenes[i].m_x));

		n_scene.append_attribute(c::ATTR_Y);
		n_scene.attribute(c::ATTR_Y).set_value(
			byte_to_hex(p_game.m_building_scenes[i].m_y));
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
		n_chunk.attribute(c::ATTR_DEFAULT_PALETTE).set_value(byte_to_hex(static_cast<byte>(lc_chunk.m_scene.m_palette)));

		n_chunk.append_attribute(c::ATTR_TILESET);
		n_chunk.attribute(c::ATTR_TILESET).set_value(byte_to_hex(static_cast<byte>(lc_chunk.m_scene.m_tileset)));

		n_chunk.append_attribute(c::ATTR_MUSIC);
		n_chunk.attribute(c::ATTR_MUSIC).set_value(byte_to_hex(static_cast<byte>(lc_chunk.m_scene.m_music)));

		n_chunk.append_attribute(c::ATTR_X);
		n_chunk.attribute(c::ATTR_X).set_value(byte_to_hex(lc_chunk.m_scene.m_x));

		n_chunk.append_attribute(c::ATTR_Y);
		n_chunk.attribute(c::ATTR_Y).set_value(byte_to_hex(lc_chunk.m_scene.m_y));

		// mattock animation
		n_chunk.append_attribute(c::ATTR_MATTOCK_ANIMATION);
		n_chunk.attribute(c::ATTR_MATTOCK_ANIMATION).set_value(join_bytes(lc_chunk.m_mattock_animation, true));

		// for each metatile
		auto n_metatiles{ n_chunk.append_child(c::TAG_METATILES) };

		for (std::size_t mt{ 0 }; mt < lc_chunk.m_metatiles.size(); ++mt) {
			const auto& lc_metatile{ lc_chunk.m_metatiles[mt] };

			auto n_metatile{ n_metatiles.append_child(c::TAG_METATILE) };
			n_metatile.append_attribute(c::ATTR_NO);
			n_metatile.attribute(c::ATTR_NO).set_value(mt);

			n_metatile.append_attribute(c::ATTR_MT_PROPERTY);
			n_metatile.attribute(c::ATTR_MT_PROPERTY).set_value(byte_to_hex(lc_metatile.m_block_property));


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

			if (lc_screen.m_sprite_set.m_command_byte.has_value()) {
				n_screen.append_attribute(c::ATTR_SPRITE_COMM_BYTE);
				n_screen.attribute(c::ATTR_SPRITE_COMM_BYTE).set_value(byte_to_hex(lc_screen.m_sprite_set.m_command_byte.value()));
			}

			// intra-chunk scrolling
			if (lc_screen.m_intrachunk_scroll.has_value()) {
				auto n_sinter_s{ n_screen.append_child(c::TAG_SCREEN_INTRACHUNK_TRANSTION) };

				n_sinter_s.append_attribute(c::ATTR_CHUNK_ID);
				n_sinter_s.attribute(c::ATTR_CHUNK_ID).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_chunk);
				n_sinter_s.append_attribute(c::ATTR_DEST_SCREEN_NO);
				n_sinter_s.attribute(c::ATTR_DEST_SCREEN_NO).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_screen);
				n_sinter_s.append_attribute(c::ATTR_DEST_X);
				n_sinter_s.attribute(c::ATTR_DEST_X).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_x);
				n_sinter_s.append_attribute(c::ATTR_DEST_Y);
				n_sinter_s.attribute(c::ATTR_DEST_Y).set_value(lc_screen.m_intrachunk_scroll.value().m_dest_y);
				n_sinter_s.append_attribute(c::ATTR_DEST_PALETTE);
				n_sinter_s.attribute(c::ATTR_DEST_PALETTE).set_value(byte_to_hex(lc_screen.m_intrachunk_scroll.value().m_palette_id));
			}

			// inter-chunk scrolling
			if (lc_screen.m_interchunk_scroll.has_value()) {
				auto n_sinter_s{ n_screen.append_child(c::TAG_SCREEN_INTERCHUNK_TRANSTION) };

				n_sinter_s.append_attribute(c::ATTR_DEST_SCREEN_NO);
				n_sinter_s.attribute(c::ATTR_DEST_SCREEN_NO).set_value(lc_screen.m_interchunk_scroll.value().m_dest_screen);
				n_sinter_s.append_attribute(c::ATTR_DEST_X);
				n_sinter_s.attribute(c::ATTR_DEST_X).set_value(lc_screen.m_interchunk_scroll.value().m_dest_x);
				n_sinter_s.append_attribute(c::ATTR_DEST_Y);
				n_sinter_s.attribute(c::ATTR_DEST_Y).set_value(lc_screen.m_interchunk_scroll.value().m_dest_y);
				n_sinter_s.append_attribute(c::ATTR_DEST_PALETTE);
				n_sinter_s.attribute(c::ATTR_DEST_PALETTE).set_value(lc_screen.m_interchunk_scroll.value().m_palette_id);
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
			if (!p_game.m_chunks[i].m_screens[s].m_sprite_set.empty()) {

				auto n_screen_sprites{ n_screen.append_child(c::TAG_SPRITES) };

				for (std::size_t sp{ 0 }; sp < p_game.m_chunks[i].m_screens[s].m_sprite_set.size(); ++sp) {
					const auto& lc_sprite{ p_game.m_chunks[i].m_screens[s].m_sprite_set.m_sprites[sp] };

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

						n_door.append_attribute(c::ATTR_REQUIREMENT);
						n_door.attribute(c::ATTR_REQUIREMENT).set_value(lc_door.m_requirement);

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


// eoe config helpers

void fe::xml::load_configuration(const std::string& p_config_xml,
	const std::string& p_region_name,
	std::map<std::string, std::size_t>& p_constants,
	std::map<std::string, std::pair<std::size_t, std::size_t>>& p_pointers,
	std::map<std::string, std::vector<byte>>& p_sets,
	std::map<std::string, std::map<byte, std::string>>& p_byte_maps) {
	pugi::xml_document l_doc;
	if (!l_doc.load_file(p_config_xml.c_str()))
		throw std::runtime_error("Could not load xml file " + p_config_xml);

	auto n_root{ l_doc.child(c::TAG_CONFIG_ROOT) };

	// constants
	auto n_consts{ n_root.child(c::TAG_CONSTANTS) };
	for (auto n_const{ n_consts.child(c::TAG_CONSTANT) }; n_const;
		n_const = n_const.next_sibling(c::TAG_CONSTANT)) {

		if (!n_const.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_const.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_const.attribute(c::ATTR_NAME).as_string() };

			if (p_constants.find(l_name) == end(p_constants))
				p_constants.insert(std::make_pair(l_name,
					parse_numeric(n_const.attribute(c::ATTR_VALUE).as_string())
				));
		}
	}

	// pointers
	auto n_ptrs{ n_root.child(c::TAG_POINTERS) };
	for (auto n_ptr{ n_ptrs.child(c::TAG_POINTER) }; n_ptr;
		n_ptr = n_ptr.next_sibling(c::TAG_POINTER)) {

		if (!n_ptr.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_ptr.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_ptr.attribute(c::ATTR_NAME).as_string() };

			if (p_pointers.find(l_name) == end(p_pointers))
				p_pointers.insert(std::make_pair(l_name,
					std::make_pair(
						parse_numeric(n_ptr.attribute(c::ATTR_VALUE).as_string()),
						parse_numeric(n_ptr.attribute(c::ATTR_ZERO_ADDR).as_string())
					)
				));
		}
	}

	// sets
	auto n_sets{ n_root.child(c::TAG_SETS) };
	for (auto n_set{ n_sets.child(c::TAG_SET) }; n_set;
		n_set = n_set.next_sibling(c::TAG_SET)) {

		if (!n_set.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_set.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_set.attribute(c::ATTR_NAME).as_string() };

			if (p_sets.find(l_name) == end(p_sets))
				p_sets.insert(std::make_pair(l_name,
					parse_byte_list(n_set.attribute(c::ATTR_VALUES).as_string())
				));
		}
	}

	// maps byte -> string (labels, char maps etc)
	auto n_bmaps{ n_root.child(c::TAG_BYTE_TO_STR_MAPS) };
	for (auto n_bmap{ n_bmaps.child(c::TAG_BYTE_TO_STR_MAP) }; n_bmap;
		n_bmap = n_bmap.next_sibling(c::TAG_BYTE_TO_STR_MAP)) {

		if (!n_bmap.attribute(c::TAG_REGION) ||
			region_match(p_region_name, n_bmap.attribute(c::TAG_REGION).as_string())) {

			std::string l_name{ n_bmap.attribute(c::ATTR_NAME).as_string() };

			if (p_sets.find(l_name) == end(p_sets)) {
				std::map<byte, std::string> l_tmp_bmap;

				for (auto n_entry{ n_bmap.child(c::TAG_ENTRY) }; n_entry;
					n_entry = n_entry.next_sibling(c::TAG_ENTRY)) {
					l_tmp_bmap.insert(std::make_pair(
						parse_numeric_byte(n_entry.attribute(c::ATTR_BYTE).as_string()),
						n_entry.attribute(c::ATTR_STRING).as_string()
					));
				}

				p_byte_maps.insert(std::make_pair(l_name, l_tmp_bmap));
			}
		}
	}
}

std::vector<fe::RegionDefinition> fe::xml::load_region_defs(const std::string& p_config_xml_file) {
	pugi::xml_document l_doc;
	if (!l_doc.load_file(p_config_xml_file.c_str()))
		throw std::runtime_error("Could not load xml file " + p_config_xml_file);

	std::vector<fe::RegionDefinition> result;

	auto n_root{ l_doc.child(c::TAG_CONFIG_ROOT) };
	auto n_regions{ n_root.child(c::TAG_REGIONS) };


	for (auto n_region{ n_regions.child(c::TAG_REGION) }; n_region;
		n_region = n_region.next_sibling(c::TAG_REGION)) {
		fe::RegionDefinition l_region;

		l_region.m_name = n_region.attribute(c::ATTR_NAME).as_string();
		if (n_region.attribute(c::ATTR_FILE_SIZE))
			l_region.m_filesize = parse_numeric(n_region.attribute(c::ATTR_FILE_SIZE).as_string());

		for (auto n_sig{ n_region.child(c::TAG_SIGNATURE) }; n_sig;
			n_sig = n_sig.next_sibling(c::TAG_SIGNATURE)) {
			l_region.m_defs.push_back(std::make_pair(
				parse_numeric(n_sig.attribute(c::ATTR_OFFSET).as_string()),
				parse_byte_list(n_sig.attribute(c::ATTR_VALUES).as_string())
			));
		}

		result.push_back(l_region);
	}

	return result;
}

// utility functions
bool fe::xml::region_match(const std::string& current_region, const std::string& region_list) {
	size_t start = 0;
	size_t end;

	while ((end = region_list.find(',', start)) != std::string::npos) {
		std::string token = region_list.substr(start, end - start);
		token = trim_whitespace(token);
		if (token == current_region) return true;
		start = end + 1;
	}

	// Handle final token
	std::string token = region_list.substr(start);
	token = trim_whitespace(token);
	return token == current_region;
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

std::string  fe::xml::trim_whitespace(const std::string& p_value) {
	std::size_t start = 0;
	while (start < p_value.size() && std::isspace(static_cast<unsigned char>(p_value[start]))) ++start;

	std::size_t end = p_value.size();
	while (end > start && std::isspace(static_cast<unsigned char>(p_value[end - 1])))
		--end;

	return p_value.substr(start, end - start);
}

byte fe::xml::parse_numeric_byte(const std::string& p_token) {
	size_t result = parse_numeric(p_token);
	if (result > 0xff)
		throw std::runtime_error("Value exceeds byte range: " + p_token);
	return static_cast<byte>(result);
}

size_t fe::xml::parse_numeric(const std::string& p_token) {
	std::string value = trim_whitespace(p_token);
	if (value.empty())
		throw std::runtime_error("Empty value");

	int base = 10;
	std::string number = value;

	// Hex formats: 0xNN or $NN
	if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		base = 16;
		number = value.substr(2);
	}
	else if (value[0] == '$') {
		base = 16;
		number = value.substr(1);
	}

	size_t result = 0;
	for (char c : number) {
		if (!std::isxdigit(static_cast<unsigned char>(c))) {
			throw std::runtime_error("Invalid digit in token: " + p_token);
		}

		int digit = std::isdigit(static_cast<unsigned char>(c))
			? c - '0'
			: std::toupper(static_cast<unsigned char>(c)) - 'A' + 10;
		result = result * base + digit;
	}

	return result;
}

std::vector<std::string> fe::xml::split_bytes(const std::string& p_values) {
	std::vector<std::string> l_result;
	std::size_t start = 0;

	while (start < p_values.size()) {
		std::size_t end = p_values.find(',', start);
		if (end == std::string::npos) end = p_values.size();

		l_result.push_back(trim_whitespace(p_values.substr(start, end - start)));
		start = end + 1;
	}

	return l_result;
}

std::string fe::xml::byte_to_hex(byte p_byte) {
	constexpr char hex_chars[] = "0123456789abcdef";
	std::string out = "0x";
	out += hex_chars[(p_byte >> 4) & 0xF];
	out += hex_chars[p_byte & 0xF];
	return out;
}

// parse comma - separated string (takes decimals, hex, prefixed by 0x, 0X or $, trims whitespace) into vector of bytes
std::vector<byte> fe::xml::parse_byte_list(const std::string& input) {
	std::vector<byte> result;
	for (const auto& token : split_bytes(input)) {
		result.push_back(parse_numeric_byte(token));
	}
	return result;
}

fe::DoorType fe::xml::text_to_doortype(const std::string& p_str) {
	if (p_str == c::VAL_DOOR_TYPE_BUILDING)
		return fe::DoorType::Building;
	else if (p_str == c::VAL_DOOR_TYPE_INTERCHUNK)
		return fe::DoorType::SameWorld;
	else if (p_str == c::VAL_DOOR_TYPE_NEXTCHUNK)
		return fe::DoorType::NextWorld;
	else if (p_str == c::VAL_DOOR_TYPE_PREVCHUNK)
		return fe::DoorType::PrevWorld;
	else
		throw std::runtime_error("Invalid door-type given in xml");
}
