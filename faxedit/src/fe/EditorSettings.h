#ifndef FE_EDITORSETTINGS_H
#define FE_EDITORSETTINGS_H

#include <vector>

namespace fe {

	struct EditorSettings {

		// patch settings
		bool m_patch_sprite_gfx;
		// gui settings
		bool m_show_sprite_sets_in_buildings;
		// sprite gfx settings
		bool m_redraw_sprite_gfx;
		std::vector<std::size_t> coll_palettes;
		float scale_frame, scale_bank;
		int transp_tolerance;
		// advanced settings
		bool m_sw_doors_in_towns;
		bool m_door_pad_byte;

		void sanitize(void);
		void sanitize_float(float& p_val, float p_default);

		template<class T>
		void clamp_value(T& p_val, T p_min, T p_max) {
			if (p_val < p_min)
				p_val = p_min;
			if (p_val > p_max)
				p_val = p_max;
		}

	public:
		EditorSettings(void);
	};

}

#endif
