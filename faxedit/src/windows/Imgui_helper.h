#ifndef FE_IMGUI_HELPER_H
#define FE_IMGUI_HELPER_H

#include <format>
#include <string>
#include <type_traits>
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

namespace fe {

	namespace ui {

		void imgui_screen(const std::string& p_label);
		bool collapsing_header(const std::string& p_label);

		template<class T>
		bool imgui_slider_with_arrows(const char* p_id, const std::string& p_label, T& value,
			T p_min, T p_max) {
			static_assert(std::is_integral<T>::value, "imgui_slider_with_arrows takes integral values");

			bool l_result{ false };

			// Convert to int for ImGui
			int l_temp = static_cast<int>(value);

			// keep the context unique
			ImGui::PushID(p_id);

			// Left arrow
			if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
				if (l_temp > static_cast<int>(p_min)) {
					l_temp--;
					l_result = true;
				}
			}

			ImGui::SameLine();

			// Slider
			if (ImGui::SliderInt("##slider", &l_temp, static_cast<int>(p_min), static_cast<int>(p_max)))
				l_result = true;

			ImGui::SameLine();

			// Right arrow
			if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
				if (l_temp < static_cast<int>(p_max)) {
					l_temp++;
					l_result = true;
				}
			}

			ImGui::Text(p_label.c_str());

			ImGui::PopID();

			if (l_result)
				value = l_temp;

			return l_result;
		}

	}

}

#endif
