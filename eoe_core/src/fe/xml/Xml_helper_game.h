#ifndef FE_XML_HELPER_GAME_H
#define FE_XML_HELPER_GAME_H

#include "fe/Game.h"
#include "fe/xml/Xml_helper.h"
#include "common/pugixml/pugixml.hpp"
#include <vector>

using byte = unsigned char;

namespace fe {

	namespace xml {

		// eoe data
		pugi::xml_document save_game_xml(const fe::Game& p_game);
		void save_game_xml_to_file(const std::string& p_filepath, const fe::Game& p_game);
		fe::Game load_game_xml(const pugi::xml_document& p_doc);
		fe::Game load_game_xml_from_file(const std::string& p_filepath);

		// sprite gfx helpers
		void add_sprite_gfx_container(pugi::xml_node p_node, const fe::SpriteFrameCollection& p_coll,
			bool sparsify_last_bank = false);
		void add_chr_bank(pugi::xml_node p_node, std::size_t p_bank_no, const std::vector<klib::NES_tile>& p_tiles);
		void add_frame(pugi::xml_node p_node, std::size_t p_frame_no, const fe::SpriteAnimationFrame& p_frame);

		// cinematic helpers
		void add_player_animation(pugi::xml_node p_node, std::size_t p_animation_no,
			const fe::SplashPlayerAnimationData& p_anim);
		void add_ripple_animation(pugi::xml_node p_node, std::size_t p_animation_no,
			const fe::SplashRippleAnimationData& p_anim);

		void read_cinematic_data(pugi::xml_node p_node, fe::Game& p_game);

		fe::SpriteFrameCollection read_sprite_gfx_container(pugi::xml_node p_node,
			bool expand_last_bank = false);
		std::vector<klib::NES_tile> read_chr_bank(pugi::xml_node p_node);
		fe::SpriteAnimationFrame read_frame(pugi::xml_node p_node);

		fe::DoorType text_to_doortype(const std::string& p_str);
	}

}

#endif
