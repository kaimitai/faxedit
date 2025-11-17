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
	{ ImVec4(0.45f, 0.35f, 0.05f, 1.0f), ImVec4(0.60f, 0.45f, 0.10f, 1.0f), ImVec4(0.75f, 0.55f, 0.15f, 1.0f) },

	// White-Ish
	{ ImVec4(0.95f, 0.95f, 0.95f, 1.00f), ImVec4(0.98f, 0.96f, 0.92f, 1.00f), ImVec4(0.92f, 0.94f, 0.97f, 1.00f) }
};

std::vector<ImVec4> fe::ui::asmColors = {
	ImVec4(1.0f, 0.4f, 0.4f, 1.0f),  // directive: light red
	ImVec4(1.0f, 1.0f, 0.4f, 1.0f),  // label: yellow
	ImVec4(0.8f, 0.8f, 0.8f, 1.0f),  // comment: light gray
	ImVec4(1.0f, 1.0f, 1.0f, 1.0f),  // text: white
	ImVec4(0.6f, 0.8f, 1.0f, 1.0f),  // opcode: light blue
	ImVec4(0.6f, 1.0f, 0.6f, 1.0f)   // operand: light green
};

void fe::ui::imgui_screen(const std::string& p_label, int p_first_x, int p_first_y, int p_first_w, int p_first_h,
	int p_style) {

	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(p_first_w), static_cast<float>(p_first_h)), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(ImVec2(static_cast<float>(p_first_x), static_cast<float>(p_first_y)), ImGuiCond_FirstUseEver);

	ImGui::PushStyleColor(ImGuiCol_TitleBg, g_uiStyles[p_style].active);
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, g_uiStyles[p_style].normal);
	ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, g_uiStyles[p_style].active);

	ImGui::Begin(p_label.c_str());

	ImGui::PopStyleColor(3);
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

void fe::ui::imgui_checkbox(const std::string& p_label, bool& p_val,
	const std::string& p_tooltip) {

	ImGui::Checkbox(p_label.c_str(), &p_val);

	if (!p_tooltip.empty() && ImGui::IsItemHovered()) {
		ImGui::SetTooltip(p_tooltip.c_str());
	}

}

void fe::ui::imgui_checkbox(const std::string& p_label, char& p_val,
	const std::string& p_tooltip) {
	bool l_val{ static_cast<bool>(p_val) };
	imgui_checkbox(p_label, l_val, p_tooltip);
	p_val = static_cast<char>(l_val);
}
