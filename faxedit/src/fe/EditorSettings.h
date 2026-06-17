#ifndef FE_EDITORSETTINGS_H
#define FE_EDITORSETTINGS_H

#include <vector>

using byte = unsigned char;

namespace fe {

	struct EditorSettings {

		// patch settings
		bool m_patch_world_chr_data, m_patch_tilemaps,
			m_patch_metadata, m_patch_sprite_data, m_patch_bank15_data,
			m_patch_sprite_gfx, m_patch_cinematics,
			throw_on_cinematic_overflow,
			m_patch_palettes, m_patch_stages, m_patch_mattock_animations,
			m_patch_push_blocks, m_patch_jump_on_tiles, m_patch_bg_gfx,
			m_patch_scenes, m_patch_fog;

		// gui settings
		bool m_show_sprite_sets_in_buildings, m_show_grid, m_animate,
			m_show_adjacent_screens, m_mattock_overlay, m_door_req_overlay;
		std::vector<char> m_overlays;
		// sprite gfx settings
		bool m_redraw_sprite_gfx, m_redraw_cinema_gfx;
		std::vector<std::size_t> coll_palettes;
		float scale_frame, scale_bank;
		int transp_tolerance;
		// advanced settings
		bool m_door_pad_byte, m_enable_config_dump;
		// camera and screen rendering settings
		float m_cam_zoom_factor;
		byte m_border_alpha;
		bool m_invert_zoom;
		// warnings
		bool m_warn_tilemap_95_pct, m_warn_00_doors;

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
		void set_sprite_palettes_defaults(void);
		void set_patching_defaults(void);
		void set_rendering_defaults(void);
		void set_sprite_gfx_defaults(void);
		void set_advanced_defaults(void);
	};

}

#endif
