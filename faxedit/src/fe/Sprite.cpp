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

byte fe::Sprite::get_id(void) const {
	return m_id;
}

byte fe::Sprite::get_x(void) const {
	return m_x;
}

byte fe::Sprite::get_y(void) const {
	return m_y;
}

bool fe::Sprite::has_text(void) const {
	return m_text_id.has_value();
}

byte fe::Sprite::get_text(void) const {
	return m_text_id.value();
}

void fe::Sprite::set_text(byte p_text) {
	m_text_id = p_text;
}
