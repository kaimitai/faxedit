#include "AtlasMovieUi.h"
#include <algorithm>
#include <cctype>
#include <format>
#include <ranges>

namespace fe::atlas_movie::ui_detail {

	bool edit_byte(const char* p_label, byte& p_value, int p_min, int p_max) {
		int value{ p_value };
		if (!ImGui::DragInt(p_label, &value, 1.0f, p_min, p_max)) return false;
		p_value = static_cast<byte>(std::clamp(value, p_min, p_max));
		return true;
	}

	bool edit_i8(const char* p_label, std::int8_t& p_value) {
		int value{ p_value };
		if (!ImGui::DragInt(p_label, &value, 1.0f, -128, 127)) return false;
		p_value = static_cast<std::int8_t>(std::clamp(value, -128, 127));
		return true;
	}

	bool edit_word(const char* p_label, std::uint16_t& p_value,
		int p_min, int p_max) {
		int value{ p_value };
		if (!ImGui::DragInt(p_label, &value, 1.0f, p_min, p_max, "$%04X"))
			return false;
		p_value = static_cast<std::uint16_t>(std::clamp(value, p_min, p_max));
		return true;
	}

	const char* frame_family(std::size_t p_frame) {
		if (p_frame <= 7) return "hero / large actor";
		if (p_frame <= 15) return "small actor";
		if (p_frame <= 23) return "hero / large actor";
		if (p_frame <= 25) return "waterfall";
		if (p_frame <= 31) return "ripple / particle";
		return "custom frame";
	}

	bool frame_combo(const char* p_label, byte& p_value, std::size_t p_count) {
		const auto current{ std::format(
			"Frame {:02} — {}", p_value, frame_family(p_value)) };
		bool changed{ false };
		if (ImGui::BeginCombo(p_label, current.c_str())) {
			static char filter[48]{};
			ImGui::SetNextItemWidth(-1);
			ImGui::InputTextWithHint("##frame-search",
				"search frame or actor...", filter, sizeof(filter));
			std::string needle{ filter };
			std::ranges::transform(needle, needle.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			for (std::size_t i{ 0 }; i < p_count; ++i) {
				const auto label{ std::format(
					"Frame {:02} — {}", i, frame_family(i)) };
				std::string haystack{ label };
				std::ranges::transform(haystack, haystack.begin(),
					[](unsigned char c) {
						return static_cast<char>(std::tolower(c));
					});
				if (!needle.empty() && haystack.find(needle) == std::string::npos)
					continue;
				if (ImGui::Selectable(label.c_str(), i == p_value)) {
					p_value = static_cast<byte>(i); changed = true;
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	void frame_drag_source(byte p_frame) {
		if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
		ImGui::SetDragDropPayload(FRAME_PAYLOAD, &p_frame, sizeof(p_frame));
		ImGui::Text("Frame %02u — %s", p_frame, frame_family(p_frame));
		ImGui::EndDragDropSource();
	}

	bool frame_drop_target(byte& p_frame, std::size_t p_count) {
		bool changed{ false };
		if (ImGui::BeginDragDropTarget()) {
			if (const auto* payload{ ImGui::AcceptDragDropPayload(FRAME_PAYLOAD) }) {
				const byte frame{ *static_cast<const byte*>(payload->Data) };
				if (frame < p_count) { p_frame = frame; changed = true; }
			}
			ImGui::EndDragDropTarget();
		}
		return changed;
	}

	void initialize_actor_editor(AtlasMovieTrack& p_track, std::size_t p_index,
		const std::string& p_name) {
		if (p_track.editor_name.empty())
			p_track.editor_name = p_name.empty()
				? std::format("Actor {}", p_index + 1) : p_name;
		if (!p_track.editor_color)
			p_track.editor_color = ACTOR_COLORS[p_index % ACTOR_COLORS.size()];
	}

}
