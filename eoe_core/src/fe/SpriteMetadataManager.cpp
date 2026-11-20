#include "SpriteMetadataManager.h"
#include "fe/fe_constants.h"
#include "fe/Config.h"

void fe::SpriteMetadataManager::initialize(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	const std::size_t SPRITE_COUNT{ p_config.constant(c::ID_SPRITE_COUNT) };
	const std::size_t INTER_BOX_OFFSET{ p_config.constant(c::ID_INTERACTION_BOX_OFFSET) };
	const std::size_t WEAPON_BOX_OFFSET{ p_config.constant(c::ID_SPRITE_WEAPON_BOX_OFFSET) };
	const std::size_t COLL_BOX_TYPE_OFFSET{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_TYPE_OFFSET) };
	const std::size_t SPRITE_TYPE_OFFSET{ p_config.constant(c::ID_SPRITE_TYPE_OFFSET) };

	for (std::size_t i{ m_sprite_meta.size() }; i < SPRITE_COUNT; ++i) {
		std::size_t inter_box_offset{ INTER_BOX_OFFSET + 4 * i };
		std::size_t weapon_box_offset{ WEAPON_BOX_OFFSET + 2 * i };

		m_sprite_meta.push_back(SpriteMetadata{
			.m_interaction_box {
				.x_offset = p_rom.at(inter_box_offset),
				.y_offset = p_rom.at(inter_box_offset + 1),
				.w = p_rom.at(inter_box_offset + 2),
				.h = p_rom.at(inter_box_offset + 3)
			},
			.m_weapon_box {
				.w = p_rom.at(weapon_box_offset),
				.h = p_rom.at(weapon_box_offset + 1)
			},
			.m_collision_box_index = p_rom.at(COLL_BOX_TYPE_OFFSET + i),
			.m_type = p_rom.at(SPRITE_TYPE_OFFSET + i)
			});
	}

	const std::size_t COLL_BOX_COUNT{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_COUNT) };
	const std::size_t COLL_BOX_W_OFFSET{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_WIDTH_OFFSET) };
	const std::size_t COLL_BOX_H_OFFSET{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_HEIGHT_OFFSET) };

	for (std::size_t i{ m_collision_boxes.size() }; i < COLL_BOX_COUNT; ++i) {
		m_collision_boxes.push_back(CollisionShape{
			.w = p_rom.at(COLL_BOX_W_OFFSET + i),
			.h = p_rom.at(COLL_BOX_H_OFFSET + i)
			});
	}
}

void fe::SpriteMetadataManager::patch_rom(const fe::Config& p_config, std::vector<byte>& p_rom) {
	const std::size_t INTER_BOX_OFFSET{ p_config.constant(c::ID_INTERACTION_BOX_OFFSET) };
	const std::size_t WEAPON_BOX_OFFSET{ p_config.constant(c::ID_SPRITE_WEAPON_BOX_OFFSET) };
	const std::size_t COLL_BOX_TYPE_OFFSET{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_TYPE_OFFSET) };
	const std::size_t SPRITE_TYPE_OFFSET{ p_config.constant(c::ID_SPRITE_TYPE_OFFSET) };

	for (std::size_t i{ 0 }; i < m_sprite_meta.size(); ++i) {
		const auto& spm{ m_sprite_meta[i] };
		std::size_t inter_box_offset{ INTER_BOX_OFFSET + 4 * i };
		std::size_t weapon_box_offset{ WEAPON_BOX_OFFSET + 2 * i };

		p_rom.at(SPRITE_TYPE_OFFSET + i) = spm.m_type;
		p_rom.at(COLL_BOX_TYPE_OFFSET + i) = spm.m_collision_box_index;

		p_rom.at(inter_box_offset) = spm.m_interaction_box.x_offset;
		p_rom.at(inter_box_offset + 1) = spm.m_interaction_box.y_offset;
		p_rom.at(inter_box_offset + 2) = spm.m_interaction_box.w;
		p_rom.at(inter_box_offset + 3) = spm.m_interaction_box.h;

		p_rom.at(weapon_box_offset) = spm.m_weapon_box.w;
		p_rom.at(weapon_box_offset + 1) = spm.m_weapon_box.h;
	}

	const std::size_t COLL_BOX_W_OFFSET{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_WIDTH_OFFSET) };
	const std::size_t COLL_BOX_H_OFFSET{ p_config.constant(c::ID_SPRITE_COLLISION_BOX_HEIGHT_OFFSET) };

	for (std::size_t i{ 0 }; i < m_collision_boxes.size(); ++i) {
		const auto& collbox{ m_collision_boxes[i] };

		p_rom.at(COLL_BOX_W_OFFSET + i) = collbox.w;
		p_rom.at(COLL_BOX_H_OFFSET + i) = collbox.h;
	}
}
