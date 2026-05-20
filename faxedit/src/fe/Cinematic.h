#ifndef FE_CINEMATIC_H
#define FE_CINEMATIC_H

#include "./sprite/SpriteAnimationFrame.h"
#include "./../common/klib/NES_tile.h"
#include <vector>

using byte = unsigned char;

namespace fe {

	class Config;
	class ROM_Manager;

	struct Velocity {
		signed char vel_x, vel_y;
	};

	struct AnimPosition {
		byte x{ 0xff }, y{ 0xff };
	};

	struct DepthState {
		byte y_threshold;
		Velocity velocity;
	};

	struct SplashPlayerAnimationData {
		AnimPosition initial_position;
		std::vector<DepthState> depth_stages;
	};

	struct SplashRippleAnimationData {
		Velocity velocity;
		AnimPosition initial_position;
	};

	struct CinematicPatchResult {
		std::size_t data_section_start,
			data_section_end;
	};

	struct Cinematic {

	private:
		void parse_palettes(const fe::Config& p_config, const std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_mgr);
		void parse_chr_data(const fe::Config& p_config, const std::vector<byte>& p_rom);
		void parse_animation_frames(const fe::Config& p_config,
			const std::vector<byte>& p_rom);
		void parse_waterfall_data(const fe::Config& p_config, const std::vector<byte>& p_rom);
		void parse_ripple_data(const fe::Config& p_config, const std::vector<byte>& p_rom);
		void parse_player_intro(const fe::Config& p_config, const std::vector<byte>& p_rom);
		void parse_player_outro(const fe::Config& p_config, const std::vector<byte>& p_rom);

		void patch_rom_gfx_data(const fe::Config& p_config, std::vector<byte>& p_rom,
			const fe::ROM_Manager& p_mgr) const;
		void patch_cinematic_data(const fe::Config& p_config, std::vector<byte>& p_rom) const;
		void patch_player_intro(const fe::Config& p_config, std::vector<byte>& p_rom) const;
		void patch_player_outro(const fe::Config& p_config, std::vector<byte>& p_rom) const;
		fe::CinematicPatchResult patch_frame_data(const fe::Config& p_config,
			std::vector<byte>& p_rom) const;

	public:
		std::vector<fe::SpriteAnimationFrame> frames;
		std::vector<klib::NES_tile> tiles;
		std::vector<byte> sprite_palette_intro, sprite_palette_outro;

		std::vector<SplashPlayerAnimationData> player_data;
		std::vector<SplashRippleAnimationData> ripple_data;

		AnimPosition waterfall_position;

		void parse_rom(const fe::Config& p_config, const std::vector<byte>& p_rom);
		fe::CinematicPatchResult patch_rom(const fe::Config& p_config,
			std::vector<byte>& p_rom) const;
	};

}

#endif
