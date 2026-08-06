#include "Sprite_set.h"

fe::Sprite_set::Sprite_set(const std::vector<byte>& p_rom_data, std::size_t p_offset) {

}

std::size_t fe::Sprite_set::size(void) const {
	return m_sprites.size();
}

fe::Sprite& fe::Sprite_set::at(std::size_t p_sprite_no) {
	return m_sprites.at(p_sprite_no);
}

const fe::Sprite& fe::Sprite_set::at(std::size_t p_sprite_no) const {
	return m_sprites.at(p_sprite_no);
}

void fe::Sprite_set::push_back(const fe::Sprite& p_sprite) {
	m_sprites.push_back(p_sprite);
}

bool fe::Sprite_set::empty(void) const {
	return m_sprites.empty();
}

std::vector<byte> fe::Sprite_set::get_bytes(void) const {
	std::vector<byte> l_result;

	// if there are both sprites with and without text - put the text ones first
	std::vector<std::size_t> l_txt_sprites, l_mute_sprites;

	for (std::size_t s{ 0 }; s < m_sprites.size(); ++s) {
		if (m_sprites[s].m_text_id.has_value())
			l_txt_sprites.push_back(s);
		else
			l_mute_sprites.push_back(s);
	}

	// hypothesis: we don't really need to end the text portion with 0xff if all the sprites use text
	// as the game doesn't look for text bytes beyond the actual screen sprite counts
	for (std::size_t i{ 0 }; i < l_txt_sprites.size(); ++i) {
		const auto& l_sprite{ m_sprites[l_txt_sprites[i]] };
		l_result.push_back(l_sprite.m_id);
		l_result.push_back(l_sprite.m_y * 16 + l_sprite.m_x);
	}

	for (std::size_t i{ 0 }; i < l_mute_sprites.size(); ++i) {
		const auto& l_sprite{ m_sprites[l_mute_sprites[i]] };
		l_result.push_back(l_sprite.m_id);
		l_result.push_back(l_sprite.m_y * 16 + l_sprite.m_x);
	}

	l_result.push_back(0xff);

	// add the text bytes
	for (std::size_t i{ 0 }; i < l_txt_sprites.size(); ++i) {
		const auto& l_sprite{ m_sprites[l_txt_sprites[i]] };
		l_result.push_back(l_sprite.m_text_id.value());
	}

	l_result.push_back(0xff);

	// add sprite command byte if it exists
	if (m_command_byte.has_value()) {
		l_result.push_back(0x80);
		l_result.push_back(m_command_byte.value());
	}

	return l_result;
}
