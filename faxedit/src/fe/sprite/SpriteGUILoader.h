#ifndef FE_GUI_SPRITE_LOADER_H
#define FE_GUI_SPRITE_LOADER_H

#include "SpriteGfxManager.h"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
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

	struct SpriteGUILoader {

		std::vector<FrameAnimation> animations;
		std::vector<ChrBank> banks;
		std::vector<std::size_t> npc_to_bank_idx;
		std::vector<fe::SpriteGUICategory> sprite_cats;

		void load_sprites_for_gui(const fe::SpriteGfxManager& p_mgr, const std::vector<byte>& p_rom,
			std::size_t p_spr_cat_offset);
		void add_offsets(FrameAnimation& frames, int p_x, int p_y = 0) const;
		void stack_snake(FrameAnimation& frames) const;

		std::vector<fe::SpriteAnimationGUIData> get_animation_dimension_data(void) const;
		std::pair<int, int> get_bounding_rect(const FrameAnimation& frames) const;

		static std::string SpriteCatToString(fe::SpriteGUICategory category);
	};

}

#endif
