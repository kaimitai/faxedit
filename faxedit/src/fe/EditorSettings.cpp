#include "EditorSettings.h"
#include <cmath>

fe::EditorSettings::EditorSettings(void) :
	m_show_sprite_sets_in_buildings{ false },
	m_show_grid{ false },
	m_show_adjacent_screens{ false },
	m_animate{ true },
	m_mattock_overlay{ false },
	m_door_req_overlay{ true },
	m_overlays{ std::vector<char>(16, 0) },
	m_redraw_sprite_gfx{ false },
	m_redraw_cinema_gfx{ false },
	m_door_pad_byte{ false },
	m_enable_config_dump{ false },
	patch_cinematic{ true },
	throw_on_cinematic_overflow{ true },
	m_cam_zoom_factor{ 1.2f },
	m_border_alpha{ 96 },
	m_invert_zoom{ false }
{
	set_sprite_gfx_defaults();
	sanitize();
}

void fe::EditorSettings::set_sprite_gfx_defaults(void) {
	m_patch_sprite_gfx = true;
	scale_frame = 3.0f;
	scale_bank = 2.0f;
	transp_tolerance = 3;
	coll_palettes = { 28, 28, 30 };
}

void fe::EditorSettings::sanitize_float(float& p_val, float p_default) {
	if (!std::isfinite(p_val))
		p_val = p_default;
}

void fe::EditorSettings::sanitize(void) {
	sanitize_float(scale_frame, 3.0f);
	sanitize_float(scale_bank, 2.0f);

	clamp_value(scale_frame, 1.0f, 8.0f);
	clamp_value(scale_bank, 1.0f, 8.0f);
	clamp_value(transp_tolerance, 0, 15);

	if (coll_palettes.size() != 3)
		coll_palettes = { 28, 28, 30 };
	if (m_overlays.size() != 16)
		m_overlays = std::vector<char>(16, 0);

	for (auto& c : m_overlays)
		clamp_value(c, static_cast<char>(0), static_cast<char>(1));
	for (auto& p : coll_palettes)
		clamp_value(p, static_cast<std::size_t>(0), static_cast<std::size_t>(30));
}
