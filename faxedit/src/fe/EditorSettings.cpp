#include "EditorSettings.h"
#include <cmath>

fe::EditorSettings::EditorSettings(void) :
	m_show_sprite_sets_in_buildings{ false },
	m_redraw_sprite_gfx{ false },
	m_patch_sprite_gfx{ true },
	scale_frame{ 3.0f },
	scale_bank{ 2.0f },
	transp_tolerance{ 3 },
	m_sw_doors_in_towns{ false },
	m_door_pad_byte{ false }
{
	sanitize();
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

	for (auto& p : coll_palettes)
		clamp_value(p, static_cast<std::size_t>(0), static_cast<std::size_t>(30));
}
