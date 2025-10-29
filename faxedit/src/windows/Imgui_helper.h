#ifndef FE_IMGUI_HELPER_H
#define FE_IMGUI_HELPER_H

#include <format>
#include <string>
#include <type_traits>
#include <vector>
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

namespace fe {

	namespace ui {

		struct UIStyle {
			ImVec4 normal;
			ImVec4 hovered;
			ImVec4 active;
		};

		extern std::vector<UIStyle> g_uiStyles;

		void imgui_screen(const std::string& p_label);
		bool collapsing_header(const std::string& p_label, const std::string& p_tooltip = std::string());
		bool imgui_button(const std::string& p_label, std::size_t p_style = 0, const std::string& p_tooltip = std::string(), bool p_disabled = false);

		template<class T1, class T2, class T3>
		bool imgui_slider_with_arrows(const char* p_id, const std::string& p_label, T1& value,
			T2 p_min, T3 p_max, const std::string& p_tooltip_text = std::string(), bool p_disabled = false) {
			static_assert(std::is_integral<T1>::value, "imgui_slider_with_arrows takes integral values");
			static_assert(std::is_integral<T2>::value, "imgui_slider_with_arrows takes integral values");
			static_assert(std::is_integral<T3>::value, "imgui_slider_with_arrows takes integral values");

			// keep the context unique
			ImGui::PushID(p_id);

			// draw label and optional tooltip text
			if (!p_label.empty()) {
				ImGui::Text(p_label.c_str());

				if (!p_tooltip_text.empty() && ImGui::IsItemHovered()) {
					ImGui::BeginTooltip();
					ImGui::Text(p_tooltip_text.c_str());
					ImGui::EndTooltip();
				}
			}
			bool l_result{ false };

			if (p_disabled)
				ImGui::BeginDisabled();

			// Convert to int for ImGui
			int l_temp = static_cast<int>(value);

			// Left arrow
			if (ImGui::ArrowButton("##left", ImGuiDir_Left)) {
				if (l_temp > static_cast<int>(p_min)) {
					l_temp--;
					l_result = true;
				}
			}

			ImGui::SameLine();

			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

			// Slider
			if (ImGui::SliderInt("##slider", &l_temp, static_cast<int>(p_min), static_cast<int>(p_max)))
				l_result = true;

			ImGui::PopItemWidth();

			ImGui::SameLine();

			// Right arrow
			if (ImGui::ArrowButton("##right", ImGuiDir_Right)) {
				if (l_temp < static_cast<int>(p_max)) {
					l_temp++;
					l_result = true;
				}
			}

			ImGui::PopID();

			if (l_result)
				value = l_temp;

			if (p_disabled)
				ImGui::EndDisabled();

			return static_cast<T1>(l_result);
		}

	}

}

#endif
