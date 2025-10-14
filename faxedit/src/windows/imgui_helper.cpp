#include "Imgui_helper.h"

void fe::ui::imgui_screen(const std::string& p_label) {
	ImGui::Begin(p_label.c_str());
}

bool fe::ui::collapsing_header(const std::string& p_label) {
	ImGui::SetNextItemOpen(true, ImGuiCond_Once);

	return ImGui::CollapsingHeader(p_label.c_str());
}
