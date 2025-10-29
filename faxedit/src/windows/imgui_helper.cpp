#include "Imgui_helper.h"
#include "../common/imgui/imgui_internal.h"

std::vector<fe::ui::UIStyle> fe::ui::g_uiStyles = {
	// Default (neutral blue from ImGui dark theme, slightly darkened)
	{ ImVec4(0.20f, 0.50f, 0.85f, 1.0f), ImVec4(0.30f, 0.60f, 0.95f, 1.0f), ImVec4(0.20f, 0.50f, 0.85f, 1.0f) },

	// Red (deep crimson tones)
	{ ImVec4(0.45f, 0.10f, 0.10f, 1.0f), ImVec4(0.60f, 0.15f, 0.15f, 1.0f), ImVec4(0.75f, 0.20f, 0.20f, 1.0f) },

	// Green (forest tones)
	{ ImVec4(0.15f, 0.45f, 0.15f, 1.0f), ImVec4(0.20f, 0.60f, 0.20f, 1.0f), ImVec4(0.25f, 0.75f, 0.25f, 1.0f) },

	// Blue (midnight tones)
	{ ImVec4(0.15f, 0.15f, 0.45f, 1.0f), ImVec4(0.20f, 0.20f, 0.60f, 1.0f), ImVec4(0.25f, 0.25f, 0.75f, 1.0f) },

	// Yellow (dark amber tones)
	{ ImVec4(0.45f, 0.35f, 0.05f, 1.0f), ImVec4(0.60f, 0.45f, 0.10f, 1.0f), ImVec4(0.75f, 0.55f, 0.15f, 1.0f) }
};

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

bool fe::ui::imgui_button(const std::string& p_label, std::size_t p_style,
	const std::string& p_tooltip_text, bool p_disabled) {

	const UIStyle& style = g_uiStyles[p_style];

	ImGui::PushStyleColor(ImGuiCol_Button, style.normal);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, style.hovered);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, style.active);

	if (p_disabled) {
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
	}

	bool l_result{ ImGui::Button(p_label.c_str()) };

	if (p_disabled) {
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}

	if (!p_tooltip_text.empty() && ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text(p_tooltip_text.c_str());
		ImGui::EndTooltip();
	}

	ImGui::PopStyleColor(3);

	return l_result;
}
