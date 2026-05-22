#include "Cinematic.h"
#include "fe_constants.h"
#include "ROM_Manager.h"
#include "Config.h"
#include "GodAllocator.h"
#include <format>
#include <stdexcept>

fe::CinematicPatchResult fe::Cinematic::patch_rom(const fe::Config& p_config,
	std::vector<byte>& p_rom) const {
	fe::ROM_Manager mgr;

	patch_rom_gfx_data(p_config, p_rom, mgr);
	patch_cinematic_data(p_config, p_rom);
	patch_player_intro(p_config, p_rom);
	patch_player_outro(p_config, p_rom);
	return patch_frame_data(p_config, p_rom);
}

fe::CinematicPatchResult fe::Cinematic::patch_frame_data(const fe::Config& p_config,
	std::vector<byte>& p_rom) const {
	fe::GodAllocator alloc;
	const std::size_t frame_count{ frames.size() };

	const auto frameptr{ p_config.pointer(c::ID_GFX_CINEMATIC_ANIM_PTR_LO) };

	std::size_t ptr_table_start{ frameptr.first };
	std::size_t ptr_table_hi_start{ frameptr.first + frame_count };
	std::size_t ptr_table_end{ ptr_table_hi_start + frame_count };
	std::size_t free_space_end{ p_config.constant(c::ID_ISCRIPT_DATA_RG2_END) };

	std::vector<std::vector<byte>> all_framebytes;
	for (const auto& frame : frames)
		all_framebytes.push_back(frame.to_cinematic_bytes());

	auto allocres{ alloc.init_and_allocate({frameptr}, {all_framebytes},
		{std::make_pair(ptr_table_end, free_space_end)}) };

	if (!allocres)
		throw std::runtime_error("Cinematic animation frame data overflows bank");
	else {
		const auto& ptrbytes{ allocres->ptr_table_writes[0].data };
		for (std::size_t i{ 0 }; i < frame_count; ++i) {
			p_rom.at(ptr_table_start + i) = ptrbytes.at(2 * i);
			p_rom.at(ptr_table_hi_start + i) = ptrbytes.at(2 * i + 1);
		}

		const auto& datawrites{ allocres->bucket_writes[0] };
		fe::ROM_Manager mgr;
		mgr.patch_bytes(datawrites.data, p_rom, datawrites.rom_offset);

		// and patch the reference to hi bytes
		mgr.patch_ptr(p_rom, p_config.constant(c::ID_CINEMATIC_FRAME_PTR_HI_REF_OFFSET),
			ptr_table_hi_start - frameptr.second);

		return fe::CinematicPatchResult{
			.data_section_start = ptr_table_start,
			.data_section_end = datawrites.rom_offset + datawrites.data.size()
		};
	}
}

void fe::Cinematic::patch_player_intro(const fe::Config& p_config, std::vector<byte>& p_rom) const {
	// patch intro player data
	const auto& player{ player_data.at(0) };

	p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_PLAYER_X_OFFSET)) = player.initial_position.x;
	p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_PLAYER_Y_OFFSET)) = player.initial_position.y;

	const std::size_t cutoff_offset{ p_config.constant(c::ID_CINEMATIC_INTRO_THRESHOLD_OFFSET) };
	const std::size_t xvel_offset{ cutoff_offset + 5 };
	const std::size_t yvel_offset{ xvel_offset + 4 };

	// stage 0 velocity -> initial velocity
	p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_VEL_X_OFFSET)) =
		static_cast<byte>(player.depth_stages.at(0).velocity.vel_x);

	p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_VEL_Y_OFFSET)) =
		static_cast<byte>(player.depth_stages.at(0).velocity.vel_y);

	// thresholds
	for (std::size_t i{ 0 }; i < 5; ++i)
		p_rom.at(cutoff_offset + i) = player.depth_stages.at(i).y_threshold;

	// stages 1-4 velocities -> velocity tables
	for (std::size_t i{ 0 }; i < 4; ++i) {

		p_rom.at(xvel_offset + i) =
			static_cast<byte>(player.depth_stages.at(i + 1).velocity.vel_x);

		p_rom.at(yvel_offset + i) =
			static_cast<byte>(player.depth_stages.at(i + 1).velocity.vel_y);
	}
}

void fe::Cinematic::patch_player_outro(const fe::Config& p_config, std::vector<byte>& p_rom) const {
	const auto& player{ player_data.at(1) };

	p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_PLAYER_X_OFFSET)) = player.initial_position.x;
	p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_PLAYER_Y_OFFSET)) = player.initial_position.y;

	const std::size_t cutoff_offset{ p_config.constant(c::ID_CINEMATIC_OUTRO_THRESHOLD_OFFSET) };
	const std::size_t xvel_offset{ cutoff_offset + 4 };
	const std::size_t yvel_offset{ xvel_offset + 4 };

	// stage 0 velocity -> initial velocity
	p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_VEL_X_OFFSET)) =
		player.depth_stages.at(0).velocity.vel_x;

	p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_VEL_Y_OFFSET)) =
		player.depth_stages.at(0).velocity.vel_y;

	// stages 0-3 thresholds
	for (std::size_t i{ 0 }; i < 4; ++i)
		p_rom.at(cutoff_offset + i) = player.depth_stages.at(i).y_threshold;

	// stage 4 threshold -> terminal compare immediate
	p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_TERMINAL_THRESHOLD_OFFSET)) =
		player.depth_stages.at(4).y_threshold;

	// stages 1-4 velocities -> velocity tables
	for (std::size_t i{ 0 }; i < 4; ++i) {
		p_rom.at(xvel_offset + i) = player.depth_stages.at(i + 1).velocity.vel_x;
		p_rom.at(yvel_offset + i) = player.depth_stages.at(i + 1).velocity.vel_y;
	}
}

void fe::Cinematic::patch_cinematic_data(const fe::Config& p_config,
	std::vector<byte>& p_rom) const {

	// patch waterfall data
	p_rom.at(p_config.constant(c::ID_CINEMATIC_WATERFALL_X_OFFSET)) = waterfall_position.x;
	p_rom.at(p_config.constant(c::ID_CINEMATIC_WATERFALL_Y_OFFSET)) = waterfall_position.y;

	// patch ripple data
	const std::size_t ripple_data_start{ p_config.constant(c::ID_CINEMATIC_RIPPLE_DATA_OFFSET) };

	for (std::size_t i{ 0 }; i < ripple_data.size(); ++i) {
		p_rom.at(ripple_data_start + i) = ripple_data[i].initial_position.x;
		p_rom.at(ripple_data_start + 2 + i) = ripple_data[i].initial_position.y;
		p_rom.at(ripple_data_start + 4 + i) = static_cast<byte>(ripple_data[i].velocity.vel_x);
		p_rom.at(ripple_data_start + 6 + i) = static_cast<byte>(ripple_data[i].velocity.vel_y);
	}
}

void fe::Cinematic::patch_rom_gfx_data(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fe::ROM_Manager& p_mgr) const {

	// patch palettes
	p_mgr.patch_bytes(sprite_palette_intro, p_rom, p_config.constant(c::ID_INTRO_ANIM_PALETTE_OFFSET) + 16);
	p_mgr.patch_bytes(sprite_palette_outro, p_rom, p_config.constant(c::ID_OUTRO_ANIM_PALETTE_OFFSET) + 16);

	// patch sprite chr
	std::vector<byte> all_chrbytes;
	for (const auto& tile : tiles) {
		const auto chrbytes{ tile.to_bytes() };
		all_chrbytes.insert(end(all_chrbytes), begin(chrbytes), end(chrbytes));
	}
	p_mgr.patch_bytes(all_chrbytes, p_rom, p_config.constant(c::ID_CINEMATIC_SPRITE_CHR_OFFSET));
}

void fe::Cinematic::parse_rom(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	fe::ROM_Manager mgr;

	parse_palettes(p_config, p_rom, mgr);
	parse_chr_data(p_config, p_rom);
	parse_animation_frames(p_config, p_rom, mgr);
	parse_waterfall_data(p_config, p_rom);
	parse_ripple_data(p_config, p_rom);
	parse_player_intro(p_config, p_rom);
	parse_player_outro(p_config, p_rom);
}

void fe::Cinematic::parse_player_outro(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	if (player_data.size() != 1)
		return;

	fe::SplashPlayerAnimationData player;
	player.initial_position.x = p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_PLAYER_X_OFFSET));
	player.initial_position.y = p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_PLAYER_Y_OFFSET));

	const std::size_t cutoff_offset{ p_config.constant(c::ID_CINEMATIC_OUTRO_THRESHOLD_OFFSET) };
	const std::size_t xvel_offset{ cutoff_offset + 4 };
	const std::size_t yvel_offset{ xvel_offset + 4 };

	player.depth_stages.push_back(DepthState{
	.y_threshold { p_rom.at(cutoff_offset)},
	.velocity {
		.vel_x { static_cast<char>(p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_VEL_X_OFFSET)))},
		.vel_y { static_cast<char>(p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_VEL_Y_OFFSET)))}
		}
		});

	for (std::size_t i{ 0 }; i < 3; ++i)
		player.depth_stages.push_back(DepthState{
			.y_threshold { p_rom.at(cutoff_offset + i + 1)},
			.velocity {
				.vel_x { static_cast<char>(p_rom.at(xvel_offset + i))},
				.vel_y { static_cast<char>(p_rom.at(yvel_offset + i))}
				}
			});

	player.depth_stages.push_back(DepthState{
		.y_threshold { p_rom.at(p_config.constant(c::ID_CINEMATIC_OUTRO_TERMINAL_THRESHOLD_OFFSET))},
		.velocity {
			.vel_x { static_cast<char>(p_rom.at(xvel_offset + 3))},
			.vel_y { static_cast<char>(p_rom.at(yvel_offset + 3))}
			}
		});

	player_data.push_back(player);
}

void fe::Cinematic::parse_player_intro(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	if (!player_data.empty())
		return;

	fe::SplashPlayerAnimationData player;
	player.initial_position.x = p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_PLAYER_X_OFFSET));
	player.initial_position.y = p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_PLAYER_Y_OFFSET));

	const std::size_t cutoff_offset{ p_config.constant(c::ID_CINEMATIC_INTRO_THRESHOLD_OFFSET) };
	const std::size_t xvel_offset{ cutoff_offset + 5 };
	const std::size_t yvel_offset{ xvel_offset + 4 };

	player.depth_stages.push_back(DepthState{
		.y_threshold { p_rom.at(cutoff_offset)},
		.velocity {
			.vel_x { static_cast<char>(p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_VEL_X_OFFSET)))},
			.vel_y { static_cast<char>(p_rom.at(p_config.constant(c::ID_CINEMATIC_INTRO_VEL_Y_OFFSET)))}
			}
		});

	for (std::size_t i{ 0 }; i < 4; ++i)
		player.depth_stages.push_back(DepthState{
			.y_threshold { p_rom.at(cutoff_offset + i + 1)},
			.velocity {
				.vel_x { static_cast<char>(p_rom.at(xvel_offset + i))},
				.vel_y { static_cast<char>(p_rom.at(yvel_offset + i))}
				}
			});

	player_data.push_back(player);
}

void fe::Cinematic::parse_ripple_data(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	const std::size_t ripple_data_start{ p_config.constant(c::ID_CINEMATIC_RIPPLE_DATA_OFFSET) };

	for (std::size_t i{ ripple_data.size() }; i < 2; ++i) {
		SplashRippleAnimationData ripple;
		ripple.initial_position.x = p_rom.at(ripple_data_start + i);
		ripple.initial_position.y = p_rom.at(ripple_data_start + 2 + i);
		ripple.velocity.vel_x = static_cast<char>(p_rom.at(ripple_data_start + 4 + i));
		ripple.velocity.vel_y = static_cast<char>(p_rom.at(ripple_data_start + 6 + i));
		ripple_data.push_back(ripple);
	}
}

void fe::Cinematic::parse_waterfall_data(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	if (waterfall_position.x == 0xff && waterfall_position.y == 0xff) {
		waterfall_position.x = p_rom.at(p_config.constant(c::ID_CINEMATIC_WATERFALL_X_OFFSET));
		waterfall_position.y = p_rom.at(p_config.constant(c::ID_CINEMATIC_WATERFALL_Y_OFFSET));
	}
}

void fe::Cinematic::parse_palettes(const fe::Config& p_config,
	const std::vector<byte>& p_rom, const fe::ROM_Manager& p_mgr) {
	if (sprite_palette_intro.empty())
		sprite_palette_intro = p_mgr.read_bytes(p_rom,
			p_config.constant(c::ID_INTRO_ANIM_PALETTE_OFFSET) + 16, 16);
	if (sprite_palette_outro.empty())
		sprite_palette_outro = p_mgr.read_bytes(p_rom,
			p_config.constant(c::ID_OUTRO_ANIM_PALETTE_OFFSET) + 16, 16);
}

void fe::Cinematic::parse_chr_data(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	for (std::size_t i{ tiles.size() };
		i < p_config.constant(c::ID_CINEMATIC_SPRITE_CHR_COUNT); ++i)
		tiles.push_back(klib::NES_tile(p_rom,
			p_config.constant(c::ID_CINEMATIC_SPRITE_CHR_OFFSET) + 16 * i));

	// TODO: check if padding is needed for the bmp interfaces
	// while (tiles.size() < 256) tiles.push_back(klib::NES_tile());
}

void fe::Cinematic::parse_animation_frames(const fe::Config& p_config,
	const std::vector<byte>& p_rom, const fe::ROM_Manager& p_mgr) {
	const auto frameptr{ p_config.pointer(c::ID_GFX_CINEMATIC_ANIM_PTR_LO) };
	std::size_t l_lo{ frameptr.first };

	const auto hi_ref_rom_offset{ p_config.constant(c::ID_CINEMATIC_FRAME_PTR_HI_REF_OFFSET) };
	const auto l_hi{ p_mgr.read_uint16_le(p_rom, hi_ref_rom_offset) + frameptr.second };

	std::size_t frame_cnt{ l_hi - l_lo };

	for (std::size_t i{ frames.size() }; i < frame_cnt; ++i) {
		fe::SpriteAnimationFrame frame;
		const auto frame_offset{ static_cast<std::size_t>(p_rom.at(l_hi + i)) * 256 +
			static_cast<std::size_t>(p_rom.at(l_lo + i)) + frameptr.second };

		frame.offset_x = static_cast<int>(static_cast<char>(p_rom.at(frame_offset)));
		frame.offset_y = static_cast<int>(static_cast<char>(p_rom.at(frame_offset + 1)));
		std::size_t w{ p_rom.at(frame_offset + 2) };
		std::size_t h{ p_rom.at(frame_offset + 3) };

		std::size_t p_offset{ frame_offset + 4 };

		for (std::size_t y{ 0 }; y < h; ++y) {

			std::vector<std::optional<SpriteFrameTile>> l_row;

			for (std::size_t x{ 0 }; x < w; ++x) {
				byte tile_no{ p_rom.at(p_offset++) };

				if (tile_no == 0xff) {
					l_row.push_back(std::nullopt);
				}
				else {
					byte l_attr{ p_rom.at(p_offset++) };

					l_row.push_back(
						fe::SpriteFrameTile(tile_no,
							l_attr & 0b11, // low 2 bits: palette idx
							(l_attr & 0x80) != 0, // v-flip, most significant bit
							(l_attr & 0x40) != 0) // h-flip: 2nd most significant bit
					);
				}
			}

			frame.tilemap.push_back(l_row);
		}

		frames.push_back(frame);
	}
}
