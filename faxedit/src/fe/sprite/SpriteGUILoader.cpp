#include "SpriteGUILoader.h"

void fe::SpriteGUILoader::load_sprites_for_gui(const fe::SpriteGfxManager& p_mgr,
	const std::vector<byte>& p_rom, std::size_t p_spr_cat_offset) {

	// prepare some overrides - ideally this should come from config
	// we have little hope of deducing this from the rom directly

	// map from npc id -> frames to disable
	std::map<std::size_t, std::set<std::size_t>> disabled_frames{
		{32, {2, 3}}, // remove scantily clad woman from the maskman frames
		{3, {0, 1, 2}}, // for zorugeriru rock only render the rock frames
		{49, {3, 4, 5}}, // for zorugeriru do not render the rock frames
	};
	// map from npc id -> chr-bank override
	std::map<std::size_t, std::size_t> bank_remaps{
		{3, 49} // render zorugeriru rock under the zorugeriru bank
	};
	// map from npc id -> extra x-offset values to add to all frames for the sprite
	// the following are all springs, which add 8 to their offset in bScript code
	std::map<std::size_t, int> extra_x_offsets{
		{0x52, 8},
		{0x61, 8},
		{0x62, 8},
		{0x63, 8}
	};
	// npcs to apply the snake transform to
	std::set<std::size_t> snake_stacks{ 18 };

	const auto& npcs{ p_mgr.c_npcs };
	const std::size_t SPRITE_COUNT{ p_mgr.npc_start_frames.size() };

	// extract sprite categories
	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i)
		sprite_cats.push_back(fe::SpriteGUICategory(p_rom.at(p_spr_cat_offset + i)));

	// copy all banks from the manager
	for (const auto& bank : npcs.banks)
		banks.push_back(bank);

	// loop over all npcs, copy their frames and apply any transform if needed
	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i) {

		// set bank mapping: override > static ppu > individual sprite chr bank
		if (bank_remaps.contains(i))
			npc_to_bank_idx.push_back(bank_remaps.at(i));
		else if (p_mgr.npc_using_common_gfx.contains(i))
			npc_to_bank_idx.push_back(npcs.banks.size() - 1);
		else
			npc_to_bank_idx.push_back(i);

		// copy all frames while weeding out unused frames
		std::vector<fe::SpriteAnimationFrame> l_frames;
		for (std::size_t j{ 0 }; j < p_mgr.npc_frame_counts.at(i); ++j) {
			if (disabled_frames.contains(i) && disabled_frames.at(i).contains(j))
				continue;
			l_frames.push_back(npcs.frames.at(p_mgr.npc_start_frames.at(i) + j).frame);
		}

		if (extra_x_offsets.contains(i))
			add_offsets(l_frames, extra_x_offsets.at(i));
		if (snake_stacks.contains(i))
			stack_snake(l_frames);

		animations.push_back(l_frames);
	}
}

std::vector<fe::SpriteAnimationGUIData> fe::SpriteGUILoader::get_animation_dimension_data(void) const {
	std::vector<fe::SpriteAnimationGUIData> result;

	for (std::size_t i{ 0 }; i < animations.size(); ++i) {
		const auto& animation{ animations[i] };
		auto bounding_rect{ get_bounding_rect(animation) };

		std::vector<std::pair<int, int>> frame_offsets;
		for (const auto& frame : animation)
			frame_offsets.push_back(std::make_pair(frame.offset_x, frame.offset_y));

		result.push_back(fe::SpriteAnimationGUIData{
			.w = bounding_rect.first,
			.h = bounding_rect.second,
			.offsets = frame_offsets,
			.category = sprite_cats.at(i)
			});
	}

	return result;
}

std::pair<int, int> fe::SpriteGUILoader::get_bounding_rect(const FrameAnimation& frames) const {
	int w{ 0 }, h{ 0 };

	for (const auto& frame : frames) {
		w = std::max(static_cast<int>(8 * static_cast<int>(frame.w()) + frame.offset_x), w);
		h = std::max(static_cast<int>(8 * static_cast<int>(frame.h()) + frame.offset_y), h);
	}

	return std::make_pair(w, h);
}

void fe::SpriteGUILoader::add_offsets(FrameAnimation& frames, int p_x, int p_y) const {
	for (auto& frame : frames) {
		frame.offset_x += p_x;
		frame.offset_y += p_y;
	}
}

void fe::SpriteGUILoader::stack_snake(FrameAnimation& frames) const {
	if (frames.size() < 2)
		return;

	const auto& body{ frames.back() };

	for (std::size_t i{ 0 }; i < frames.size() - 1; ++i) {
		frames[i].offset_y = -48;

		for (std::size_t j{ 0 }; j < 3; ++j)
			for (const auto& row : body.tilemap)
				frames[i].tilemap.push_back(row);
	}
	frames.pop_back();
}

std::string fe::SpriteGUILoader::SpriteCatToString(fe::SpriteGUICategory category) {
	switch (category) {
	case SpriteGUICategory::Enemy: return "Enemy";
	case SpriteGUICategory::DroppedItem: return "Dropped Item";
	case SpriteGUICategory::NPC: return "NPC";
	case SpriteGUICategory::SpecialEffect: return "Special Effect";
	case SpriteGUICategory::GameTrigger: return "Game Trigger";
	case SpriteGUICategory::Item: return "Item";
	case SpriteGUICategory::MagicEffect: return "Magic Effect";
	case SpriteGUICategory::Boss: return "Boss";
	case SpriteGUICategory::Glitched: return "Glitch";
	default: return "Unknown";
	}
}
