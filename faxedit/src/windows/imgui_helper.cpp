#include "Imgui_helper.h"

void fe::ui::imgui_screen(const std::string& p_label) {
	ImGui::Begin(p_label.c_str());
}

bool fe::ui::collapsing_header(const std::string& p_label, const std::string& p_tooltip_text) {
	ImGui::SetNextItemOpen(false, ImGuiCond_Once);

	bool l_result{ ImGui::CollapsingHeader(p_label.c_str()) };
	
	if (!p_tooltip_text.empty() && ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text(p_tooltip_text.c_str());
		ImGui::EndTooltip();
	}
	
	return l_result;
}

bool fe::ui::imgui_button(const std::string& p_label, const std::string& p_tooltip_text) {
	bool l_result{ ImGui::Button(p_label.c_str()) };

	if (!p_tooltip_text.empty() && ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text(p_tooltip_text.c_str());
		ImGui::EndTooltip();
	}

	return l_result;
}
