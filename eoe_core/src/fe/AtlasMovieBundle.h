#ifndef FE_ATLASMOVIEBUNDLE_H
#define FE_ATLASMOVIEBUNDLE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "AtlasMovieLayout.h"

using byte = unsigned char;

namespace fe {

	enum class AtlasMovieExit : byte { NewGame = 1, TitleReset = 2, ReloadScreen = 3 };
	enum class AtlasMovieAssetKind : byte { SpriteChr = 1, BackgroundChr = 2, Nametable = 3, Palette = 4 };
	enum class AtlasMovieDestination : byte { Ppu = 0, Ram = 1 };
	enum class AtlasMovieTrackKind : byte { Path = 1, Cyclic = 2, CounterToggle = 3 };
	enum class AtlasMovieCoordinate : byte { X = 1, Y = 2 };
	enum class AtlasMovieComparison : byte { LessThan = 1, GreaterEqual = 2 };
	enum class AtlasMovieEnterAction : byte { None = 0, SetFrameCounter = 1 };
	enum class AtlasMovieEffect : byte { None = 0, PaletteFade = 1 };
	enum class AtlasMovieCondition : byte {
		EffectCalls = 1, TrackYGte = 2, MusicZero = 3,
		FrameCounterZero = 4, Frames = 5
	};
	enum class AtlasMovieImportKind : byte {
		SpriteChr = 1, MetaspriteLibrary = 2,
		BackgroundChr = 3, Nametable = 4, Palette = 5
	};
	enum class AtlasMovieProjectRole : byte { Normal = 0, OfficialIntro = 1, OfficialEnding = 2 };

	struct AtlasMovieAsset {
		AtlasMovieAssetKind kind{ AtlasMovieAssetKind::SpriteChr };
		byte bank{ 12 };
		std::uint16_t cpu{ 0x8000 };
		AtlasMovieDestination destination_space{ AtlasMovieDestination::Ppu };
		std::uint16_t destination{ 0 };
		std::uint16_t bytes{ 1 };
	};

	struct AtlasMovieKeyframe {
		byte threshold{ 0 };
		std::int8_t velocity_x{ 0 };
		std::int8_t velocity_y{ 0 };
	};

	struct AtlasMovieWaypoint {
		byte x{ 0 }, y{ 0 };
	};

	struct AtlasMovieAnimationSet {
		bool automatic_facing{ false };
		std::vector<byte> idle, left, right, toward, away, attack, hurt;
	};

	struct AtlasMovieTrack {
		AtlasMovieTrackKind kind{ AtlasMovieTrackKind::Path };
		byte x{ 0 }, x_fraction{ 0 }, y{ 0 }, y_fraction{ 0 };
		std::int8_t velocity_x{ 0 }, velocity_y{ 0 };
		byte integrator_shift{ 7 };

		AtlasMovieCoordinate coordinate{ AtlasMovieCoordinate::Y };
		AtlasMovieComparison comparison{ AtlasMovieComparison::GreaterEqual };
		std::vector<AtlasMovieKeyframe> keyframes;
		byte dwell_frames{ 16 };
		std::vector<std::vector<byte>> stage_frames;

		byte reset_at_pose{ 1 };
		std::vector<byte> visible_frames;

		std::uint16_t counter_address{ 0x001a };
		byte counter_mask{ 1 };
		std::vector<byte> toggle_frames{ 0, 0 };

		// AMP-only editor data
		std::string editor_name;
		std::uint32_t editor_color{ 0 };
		std::string editor_group;
		std::vector<AtlasMovieWaypoint> editor_waypoints;
		AtlasMovieAnimationSet editor_animation;
	};

	struct AtlasMovieSfx {
		byte track{ 0 }, sound{ 0 }, stage_lt{ 0xff };
		byte tick_mask{ 0 }, tick_value{ 0 }, slot_mask{ 0 }, slot_value{ 0 };
	};

	struct AtlasMoviePhase {
		byte update_mask{ 1 }, draw_mask{ 1 };
		AtlasMovieEnterAction enter_action{ AtlasMovieEnterAction::None };
		byte enter_value{ 0 };
		AtlasMovieEffect effect{ AtlasMovieEffect::None };
		byte effect_track{ 0xff }, effect_stage{ 0 }, effect_period{ 0 };
		byte effect_subtract{ 0 }, effect_floor{ 0 };
		AtlasMovieCondition condition{ AtlasMovieCondition::Frames };
		byte condition_track{ 0xff };
		std::uint16_t condition_value{ 1 };
	};

	// Atlas-owned data appended to FMB1
	struct AtlasMovieImport {
		AtlasMovieImportKind kind{ AtlasMovieImportKind::SpriteChr };
		std::string label;
		std::uint16_t destination{ 0 };
		// MetaspriteLibrary: aux=count, data=[lo][hi][frames]
		byte aux{ 0 };
		std::vector<byte> data;
	};

	struct AtlasMovie {
		std::string id{ "movie" };
		// disabled movies stay in AMP but are omitted from FMB/AME
		bool enabled{ true };
		AtlasMovieProjectRole project_role{ AtlasMovieProjectRole::Normal };
		AtlasMovieExit exit_mode{ AtlasMovieExit::ReloadScreen };
		// $FF=keep, $FE=stop, otherwise music ID
		byte entry_music{ 0xff };
		byte metasprite_bank{ 12 };
		std::uint16_t metasprite_pointer_lo{ 0xaa53 };
		std::uint16_t metasprite_pointer_hi{ 0xaa6b };
		byte metasprite_count{ 24 };
		std::vector<AtlasMovieAsset> assets;
		std::vector<AtlasMovieTrack> tracks;
		std::vector<AtlasMovieSfx> sfx;
		std::vector<AtlasMoviePhase> phases;
		std::vector<AtlasMovieImport> imports;
	};

	struct AtlasMovieBundle {
		std::vector<AtlasMovie> movies;
	};

	struct AtlasMovieBundleReport {
		std::size_t bytes{ 0 };
		std::uint16_t reserved_cpu_end{ 0 };
		std::vector<std::string> warnings;
	};

	struct AtlasMovieBudgetLine {
		std::string label;
		std::size_t bytes{ 0 };
	};

	struct AtlasMovieDetailedBudget {
		std::size_t bundle_header_bytes{ 0 }, import_header_bytes{ 0 };
		std::vector<AtlasMovieBudgetLine> movies, tracks, assets, imports, phases, events;
		std::vector<std::string> suggestions;
	};

	class AtlasMovieBundleCodec {
	public:
		static constexpr std::uint16_t BUNDLE_CPU{ atlas_movie::layout::BUNDLE_CPU };
		static constexpr std::size_t DISPATCH_BYTES{ atlas_movie::layout::DISPATCH_BYTES };
		static constexpr byte METASPRITE_MAX_WIDTH{ 16 };
		static constexpr byte METASPRITE_MAX_HEIGHT{ 30 };

		static AtlasMovieBundle parse(const std::vector<byte>& p_fmb);
		// Validate one FMB at offset zero while allowing a containing bank tail.
		static std::size_t validated_prefix_size(const std::vector<byte>& p_data);
		static std::vector<byte> compile(const AtlasMovieBundle& p_bundle);
		static AtlasMovieBundleReport validate(const AtlasMovieBundle& p_bundle);
		static AtlasMovieDetailedBudget detailed_budget(const AtlasMovieBundle& p_bundle);
		static AtlasMovieBundle parse_project(const std::vector<byte>& p_amp);
		static std::vector<byte> compile_project(const AtlasMovieBundle& p_bundle);

		static std::vector<byte> extract_from_ame(const std::vector<byte>& p_ame);
		static std::vector<byte> replace_in_ame(const std::vector<byte>& p_ame,
			const AtlasMovieBundle& p_bundle);
		static std::vector<byte> extract_from_installed_rom(const std::vector<byte>& p_rom);
		static AtlasMovieBundleReport replace_in_installed_rom(std::vector<byte>& p_rom,
			const AtlasMovieBundle& p_bundle);
	};

}

#endif
