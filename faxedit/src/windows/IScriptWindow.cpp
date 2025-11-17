#include "MainWindow.h"
#include "Imgui_helper.h"
#include "./../fe/fe_app_constants.h"

void fe::MainWindow::draw_iscript_window(SDL_Renderer* p_rnd) {
	ui::imgui_screen("iScripts", c::WIN_ISCRIPT_X, c::WIN_ISCRIPT_Y,
		c::WIN_ISCRIPT_W, c::WIN_ISCRIPT_H);

	if (m_iscript_win_set_focus) {
		ImGui::SetWindowFocus();
		m_iscript_win_set_focus = false;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 2.0f));

	ImGui::SeparatorText("Selected iScript");
	ui::imgui_slider_with_arrows("###iscript", "", m_sel_iscript, 0, 151, "",
		false, true);

	ImGui::SameLine();

	if (ui::imgui_button("Close", 4, "Close the iScript window"))
		m_iscript_window = false;

	ImGui::BeginChild("AsmScrollRegion", ImVec2(0, 0), true,
		ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

	for (const auto& tok : m_iscripts[m_sel_iscript]) {
		ImGui::PushStyleColor(ImGuiCol_Text, ui::asmColors[tok.color_idx]);
		ImGui::TextUnformatted(tok.text.c_str());
		ImGui::PopStyleColor();

		if (!tok.newline)
			ImGui::SameLine();
	}

	ImGui::PopStyleVar();
	ImGui::EndChild();

	ImGui::End();
}
