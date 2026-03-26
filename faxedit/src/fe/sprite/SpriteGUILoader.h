#ifndef FE_GUI_SPRITE_LOADER_H
#define FE_GUI_SPRITE_LOADER_H

#include "SpriteGfxManager.h"
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "./../Config.h"
#include "./../../common/klib/NES_tile.h"

using byte = unsigned char;
using ChrBank = std::vector<klib::NES_tile>;
using FrameAnimation = std::vector<fe::SpriteAnimationFrame>;

namespace fe {

	enum class SpriteGUICategory : byte {
		Enemy = 0,
		DroppedItem = 1,
		NPC = 2,
		SpecialEffect = 3,
		GameTrigger = 4,
		Item = 5,
		MagicEffect = 6,
		Boss = 7,
		// not a real in-game category but we use this in the GUI
		Glitched = 8
	};

	struct SpriteAnimationGUIData {
		// w and h for the sprite
		int w, h;
		// x and y offsets per frame
		std::vector<std::pair<int, int>> offsets;
		// sprite category
		fe::SpriteGUICategory category;
	};

	struct SpriteRenderOverrides {
		// map sprite id -> {ignore frames}
		std::map<std::size_t, std::set<std::size_t>> disabled_frames;
		// map sprite id -> sprite chr-bank override
		std::map<std::size_t, std::size_t> bank_remaps;
		// map from npc id -> extra x-offset values to add to all frames for the sprite
		std::map<std::size_t, int> extra_x_offsets;
		// npcs to apply the stack snake transform to
		std::set<std::size_t> snake_stacks;
	};

	struct SpriteGUILoader {
		const std::string_view RCMD_HIDE_FRAMES{ "hide_frames" };
		const std::string_view RCMD_CHR_BANK_FROM{ "chr_bank_from" };
		const std::string_view RCMD_ADD_X_OFFSET{ "add_x_offset" };
		const std::string_view RCMD_STACK{ "stack" };

		std::vector<FrameAnimation> animations;
		std::vector<ChrBank> banks;
		std::vector<std::size_t> npc_to_bank_idx;
		std::vector<fe::SpriteGUICategory> sprite_cats;

		fe::SpriteRenderOverrides load_render_overrides(const fe::Config& p_config,
			const fe::SpriteGfxManager& p_mgr) const;
		void load_sprites_for_gui(const fe::Config& p_config,
			const fe::SpriteGfxManager& p_mgr, const std::vector<byte>& p_rom);
		void add_offsets(FrameAnimation& frames, int p_x, int p_y = 0) const;
		void stack_snake(FrameAnimation& frames) const;

		std::vector<fe::SpriteAnimationGUIData> get_animation_dimension_data(void) const;
		std::pair<int, int> get_bounding_rect(const FrameAnimation& frames) const;

		static std::string SpriteCatToString(fe::SpriteGUICategory category);
	};

}

#endif
