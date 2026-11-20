#ifndef FE_SPRITE_METADATA_MANAGER_H
#define FE_SPRITE_METADATA_MANAGER_H

#include <vector>

class fe::Config;

using byte = unsigned char;

namespace fe {

	struct CollisionShape {
		byte w, h;
	};

	struct InteractionBox {
		byte x_offset, y_offset, w, h;
	};

	struct SpriteMetadata {
		InteractionBox m_interaction_box;
		CollisionShape m_weapon_box;
		byte m_collision_box_index;
		byte m_type;
	};

	struct SpriteMetadataManager {
		std::vector<SpriteMetadata> m_sprite_meta;
		std::vector<CollisionShape> m_collision_boxes;

		void initialize(const fe::Config& p_config, const std::vector<byte>& p_rom);
		void patch_rom(const fe::Config& p_config, std::vector<byte>& p_rom);
	};

}

#endif
