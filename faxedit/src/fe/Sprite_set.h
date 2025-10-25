#ifndef FE_SPRITE_SET_H
#define FE_SPRITE_SET_H

#include "Sprite.h"
#include <optional>
#include <vector>

using byte = unsigned char;

namespace fe {

	struct Sprite_set {

		std::optional<byte> m_command_byte;
		std::vector<fe::Sprite> m_sprites;

		Sprite_set(void) = default;
		std::size_t size(void) const;
		void push_back(const fe::Sprite& p_sprite);
		fe::Sprite& at(std::size_t p_sprite_no);
		const fe::Sprite& at(std::size_t p_sprite_no) const;
		Sprite_set(const std::vector<byte>& p_rom_data, std::size_t p_offset);
		bool empty(void) const;

		std::vector<byte> get_bytes(void) const;
	};

}

#endif
