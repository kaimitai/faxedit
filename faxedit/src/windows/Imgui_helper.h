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

		void imgui_screen(const std::string& p_label, int p_first_x, int p_first_y, int p_first_w, int p_first_h,
			int p_style = 0);
		bool collapsing_header(const std::string& p_label, const std::string& p_tooltip = std::string());
		bool imgui_button(const std::string& p_label, std::size_t p_style = 0, const std::string& p_tooltip = std::string(), bool p_disabled = false);

		template<class T1, class T2, class T3>
		bool imgui_slider_with_arrows(const char* p_id, const std::string& p_label, T1& value,
			T2 p_min, T3 p_max, const std::string& p_tooltip_text = std::string(), bool p_disabled = false,
			bool p_main_slider = false) {
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

			if (p_main_slider) {
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.35f, 0.30f, 0.05f, 1.0f)); // muted gold
				ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.45f, 0.38f, 0.10f, 1.0f)); // warmer gold
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.55f, 0.45f, 0.15f, 1.0f)); // bright gold

				ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // bright yellow
				ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(1.0f, 0.6f, 0.0f, 1.0f)); // orange-yellow

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.30f, 0.05f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.38f, 0.10f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.45f, 0.15f, 1.0f));
			}

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

			if (p_main_slider)
				ImGui::PopStyleColor(8);

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
