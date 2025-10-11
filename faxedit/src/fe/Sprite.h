#ifndef FE_SPRITE_H
#define FE_SPRITE_H

#include <optional>
#include <vector>

using byte = unsigned char;

namespace fe {

	class Sprite {

		byte m_id, m_x, m_y;
		std::optional<byte> m_text_id;

	public:
		Sprite(byte p_id, byte p_x, byte p_y, byte p_text);
		Sprite(byte p_id, byte p_x, byte p_y);

		void set_text(byte p_text);

		byte get_id(void) const;
		byte get_x(void) const;
		byte get_y(void) const;
		bool has_text(void) const;
		byte get_text(void) const;
	};

}

#endif
