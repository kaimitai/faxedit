#include "SpriteGUILoader.h"
#include "./../fe_constants.h"
#include "./../../common/klib/Kstring.h"

fe::SpriteRenderOverrides fe::SpriteGUILoader::load_render_overrides(const fe::Config& p_config,
	const fe::SpriteGfxManager& p_mgr) const {
	std::map<std::size_t, std::set<std::size_t>> disabled_frames;
	std::map<std::size_t, std::size_t> bank_remaps;
	std::map<std::size_t, int> extra_x_offsets;
	std::set<std::size_t> snake_stacks;

	auto gui_map = p_config.bmap(c::ID_SPRITE_HANDLER_GUI);

	for (const auto& kv : gui_map) {
		std::size_t handler_id{ static_cast<std::size_t>(kv.first) };
		auto npc_ids{ p_mgr.query_npcs_using_handler(handler_id) };
		const auto rules{ klib::str::split_string(kv.second, ';') };

		for (const auto& rule : rules) {
			auto rulewithargs{ klib::str::split_string(rule, ':') };
			if (rulewithargs.empty())
				continue;

			auto rulestr{ klib::str::trim(rulewithargs.at(0)) };
			if (rulestr == RCMD_STACK) {
				snake_stacks.merge(npc_ids);
			}
			else if (rulestr == RCMD_ADD_X_OFFSET && rulewithargs.size() > 1) {
				int xoffset{ klib::str::parse_numeric(rulewithargs[1]) };
				for (std::size_t npcid : npc_ids)
					extra_x_offsets[npcid] = xoffset;
			}
			else if (rulestr == RCMD_HIDE_FRAMES && rulewithargs.size() > 1) {
				auto hideframe_strs{ klib::str::split_string(rulewithargs[1], ',') };
				std::set<std::size_t> hideframe_ids;

				for (const auto& str : hideframe_strs)
					hideframe_ids.insert(static_cast<std::size_t>(klib::str::parse_numeric(
						klib::str::trim(str)
					)));

				for (std::size_t npcid : npc_ids)
					for (std::size_t frameid : hideframe_ids)
						disabled_frames[npcid].insert(frameid);
			}
			else if (rulestr == RCMD_CHR_BANK_FROM && rulewithargs.size() > 1) {
				std::size_t target_handler_w_bank{ static_cast<std::size_t>(klib::str::parse_numeric(rulewithargs[1])) };
				const auto target_npc_ids{ p_mgr.query_npcs_using_handler(target_handler_w_bank) };

				std::size_t target_npc_bank{ target_npc_ids.empty() ?
					p_mgr.c_npcs.banks.size() - 1 : // default to common chr bank if all else fails
					*begin(target_npc_ids) };

				for (std::size_t npcid : npc_ids)
					bank_remaps[npcid] = target_npc_bank;
			}
		}
	}

	return fe::SpriteRenderOverrides{
		.disabled_frames = disabled_frames,
		.bank_remaps = bank_remaps,
		.extra_x_offsets = extra_x_offsets,
		.snake_stacks = snake_stacks
	};
}

void fe::SpriteGUILoader::load_sprites_for_gui(const fe::Config& p_config,
	const fe::SpriteGfxManager& p_mgr, const std::vector<byte>& p_rom) {
	const auto& npcs{ p_mgr.c_npcs };
	const std::size_t SPRITE_COUNT{ p_mgr.npc_start_frames.size() };
	const std::size_t SPRITE_TYPE_OFFSET{ p_config.constant(c::ID_SPRITE_TYPE_OFFSET) };

	fe::SpriteRenderOverrides overrides;

	try {
		overrides = load_render_overrides(p_config, p_mgr);
	}
	catch (const std::exception&) {
		// ignore - this is just rendering
	}

	// extract sprite categories
	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i)
		sprite_cats.push_back(fe::SpriteGUICategory(p_rom.at(SPRITE_TYPE_OFFSET + i)));

	// copy all banks from the manager
	for (const auto& bank : npcs.banks)
		banks.push_back(bank);

	// loop over all npcs, copy their frames and apply any transform if needed
	for (std::size_t i{ 0 }; i < SPRITE_COUNT; ++i) {

		// set bank mapping: override > static ppu > individual sprite chr bank
		if (overrides.bank_remaps.contains(i))
			npc_to_bank_idx.push_back(overrides.bank_remaps.at(i));
		else if (p_mgr.npc_using_common_gfx.contains(i))
			npc_to_bank_idx.push_back(npcs.banks.size() - 1);
		else
			npc_to_bank_idx.push_back(i);

		// copy all frames while weeding out unused frames
		std::vector<fe::SpriteAnimationFrame> l_frames;
		for (std::size_t j{ 0 }; j < p_mgr.npc_frame_counts.at(i); ++j) {
			if (overrides.disabled_frames.contains(i) && overrides.disabled_frames.at(i).contains(j))
				continue;
			l_frames.push_back(npcs.frames.at(p_mgr.npc_start_frames.at(i) + j).frame);
		}

		if (overrides.extra_x_offsets.contains(i))
			add_offsets(l_frames, overrides.extra_x_offsets.at(i));
		if (overrides.snake_stacks.contains(i))
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
