#include "Sprite.h"

fe::Sprite::Sprite(byte p_id, byte p_x, byte p_y) :
	m_id{ p_id }, m_x{ p_x }, m_y{ p_y }
{
}

fe::Sprite::Sprite(byte p_id, byte p_x, byte p_y, byte p_text) :
	fe::Sprite::Sprite(p_id, p_x, p_y)
{
	m_text_id = p_text;
}

void fe::Sprite::set_text(byte p_text) {
	m_text_id = p_text;
}
