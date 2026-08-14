#ifndef FE_ATLASMOVIEUI_H
#define FE_ATLASMOVIEUI_H

#include "fe/AtlasMovieBundle.h"
#include "../common/imgui/imgui.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace fe::atlas_movie::ui_detail {

	inline constexpr char FRAME_PAYLOAD[]{ "ATLAS_MOVIE_FRAME" };
	inline constexpr char GAME_SPRITE_PAYLOAD[]{ "ATLAS_GAME_SPRITE" };
	inline constexpr std::array<std::uint32_t, 8> ACTOR_COLORS{
		IM_COL32(255, 190, 70, 255), IM_COL32(80, 210, 255, 255),
		IM_COL32(255, 90, 130, 255), IM_COL32(120, 255, 130, 255),
		IM_COL32(200, 130, 255, 255), IM_COL32(255, 245, 120, 255),
		IM_COL32(100, 170, 255, 255), IM_COL32(255, 150, 70, 255)
	};

	bool edit_byte(const char* p_label, byte& p_value,
		int p_min = 0, int p_max = 255);
	bool edit_i8(const char* p_label, std::int8_t& p_value);
	bool edit_word(const char* p_label, std::uint16_t& p_value,
		int p_min = 0, int p_max = 0xffff);
	const char* frame_family(std::size_t p_frame);
	bool frame_combo(const char* p_label, byte& p_value, std::size_t p_count);
	void frame_drag_source(byte p_frame);
	bool frame_drop_target(byte& p_frame, std::size_t p_count);
	void initialize_actor_editor(AtlasMovieTrack& p_track, std::size_t p_index,
		const std::string& p_name = {});

	template<class Enum, std::size_t N>
	bool enum_combo(const char* p_label, Enum& p_value,
		const std::array<const char*, N>& p_names, int p_base = 0) {
		int value{ static_cast<int>(p_value) - p_base };
		if (!ImGui::Combo(p_label, &value, p_names.data(), static_cast<int>(N)))
			return false;
		p_value = static_cast<Enum>(value + p_base);
		return true;
	}

}

#endif
