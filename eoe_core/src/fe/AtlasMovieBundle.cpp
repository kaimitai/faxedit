#include "AtlasMovieBundle.h"
#include "AtlasMovieEngine.h"
#include "AtlasMovieLayout.h"
#include <algorithm>
#include <array>
#include <format>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>

namespace {
	using namespace fe;

	constexpr byte FORMAT_VERSION{ 2 };
	constexpr byte IMPORT_VERSION{ 1 };
	constexpr byte PROJECT_VERSION{ 2 };
	constexpr std::size_t VANILLA_HANDLER_COUNT{
		atlas_movie::layout::DISPATCH_ENTRIES };
	constexpr std::uint16_t DISPATCH_HIGH_REF{
		atlas_movie::layout::DISPATCH_HIGH_REF };
	constexpr std::uint16_t DISPATCH_LOW_REF{
		atlas_movie::layout::DISPATCH_LOW_REF };
	constexpr std::uint16_t BUNDLE_CPU{ atlas_movie::layout::BUNDLE_CPU };
	constexpr auto& HANDLERS{ atlas_movie::layout::HANDLERS };

	std::size_t file_offset(byte p_bank, std::uint16_t p_cpu) {
		return atlas_movie::layout::file_offset(p_bank, p_cpu);
	}

	void require(bool p_condition, const std::string& p_message) {
		if (!p_condition)
			throw std::runtime_error("Atlas Movie Creator: " + p_message);
	}

	void require_u8(std::size_t p_value, const std::string& p_label) {
		require(p_value <= 0xff, p_label + " exceeds 255");
	}

	class Reader {
		const std::vector<byte>& m_data;
		std::size_t m_cursor{ 0 }, m_limit;
		std::string m_label;
	public:
		Reader(const std::vector<byte>& p_data, std::string p_label,
			std::size_t p_start = 0, std::size_t p_size = std::numeric_limits<std::size_t>::max()) :
			m_data{ p_data },
			m_limit{ p_size == std::numeric_limits<std::size_t>::max() ? p_data.size() : p_start + p_size },
			m_label{ std::move(p_label) } {
			m_cursor = p_start;
			require(m_limit <= m_data.size(), m_label + " is truncated");
		}
		byte u8() {
			require(m_cursor < m_limit, m_label + " is truncated");
			return m_data[m_cursor++];
		}
		std::uint16_t u16() {
			const auto lo{ u8() }, hi{ u8() };
			return static_cast<std::uint16_t>(lo | hi << 8);
		}
		std::uint32_t u32() {
			std::uint32_t result{};
			for (unsigned shift{ 0 }; shift < 32; shift += 8)
				result |= static_cast<std::uint32_t>(u8()) << shift;
			return result;
		}
		std::vector<byte> bytes(std::size_t p_count) {
			require(p_count <= m_limit - m_cursor, m_label + " is truncated");
			std::vector<byte> result(m_data.begin() + m_cursor, m_data.begin() + m_cursor + p_count);
			m_cursor += p_count;
			return result;
		}
		std::string ascii(std::size_t p_count) {
			const auto raw{ bytes(p_count) };
			require(std::all_of(raw.begin(), raw.end(), [](byte c) { return c >= 0x20 && c <= 0x7e; }),
				m_label + " contains a non-ASCII movie ID");
			return std::string(raw.begin(), raw.end());
		}
		std::size_t cursor() const { return m_cursor; }
		void done() const { require(m_cursor == m_limit, m_label + " has trailing bytes"); }
	};

	void append_word(std::vector<byte>& p_out, std::uint16_t p_value) {
		p_out.push_back(static_cast<byte>(p_value & 0xff));
		p_out.push_back(static_cast<byte>(p_value >> 8));
	}

	void dword(std::vector<byte>& p_out, std::uint32_t p_value) {
		for (unsigned shift{ 0 }; shift < 32; shift += 8)
			p_out.push_back(static_cast<byte>(p_value >> shift));
	}

	void sized_ascii(std::vector<byte>& p_out, const std::string& p_value,
		const std::string& p_label) {
		require_u8(p_value.size(), p_label + " length");
		require(std::all_of(p_value.begin(), p_value.end(), [](unsigned char c) {
			return c >= 0x20 && c <= 0x7e;
		}), p_label + " must contain printable ASCII");
		p_out.push_back(static_cast<byte>(p_value.size()));
		p_out.insert(p_out.end(), p_value.begin(), p_value.end());
	}

	constexpr std::array<std::uint32_t, 8> ACTOR_COLORS{
		0xff46beff, 0xffffd250, 0xff825aff, 0xff82ff78,
		0xffff82c8, 0xff78aaff, 0xff78f5ff, 0xff4696ff
	};

	void ensure_editor_defaults(AtlasMovie& p_movie) {
		for (std::size_t i{ 0 }; i < p_movie.tracks.size(); ++i) {
			auto& track{ p_movie.tracks[i] };
			if (track.editor_name.empty()) track.editor_name = std::format("Actor {}", i + 1);
			if (!track.editor_color) track.editor_color = ACTOR_COLORS[i % ACTOR_COLORS.size()];
		}
	}

	void frame_vector(std::vector<byte>& p_out, const std::vector<byte>& p_frames,
		const std::string& p_label) {
		require_u8(p_frames.size(), p_label + " frame count");
		p_out.push_back(static_cast<byte>(p_frames.size()));
		p_out.insert(p_out.end(), p_frames.begin(), p_frames.end());
	}

	std::vector<byte> compile_editor_metadata(const AtlasMovie& p_movie) {
		AtlasMovie normalized{ p_movie }; ensure_editor_defaults(normalized);
		std::vector<byte> out{ 'A','P','M','1', 1, static_cast<byte>(normalized.tracks.size()) };
		for (const auto& track : normalized.tracks) {
			sized_ascii(out, track.editor_name, "actor name");
			dword(out, track.editor_color);
			sized_ascii(out, track.editor_group, "actor group");
			require_u8(track.editor_waypoints.size(), "actor waypoint count");
			out.push_back(static_cast<byte>(track.editor_waypoints.size()));
			for (const auto& point : track.editor_waypoints) out.insert(out.end(), { point.x, point.y });
			out.push_back(static_cast<byte>(track.editor_animation.automatic_facing));
			frame_vector(out, track.editor_animation.idle, "idle animation");
			frame_vector(out, track.editor_animation.left, "left animation");
			frame_vector(out, track.editor_animation.right, "right animation");
			frame_vector(out, track.editor_animation.toward, "toward animation");
			frame_vector(out, track.editor_animation.away, "away animation");
			frame_vector(out, track.editor_animation.attack, "attack animation");
			frame_vector(out, track.editor_animation.hurt, "hurt animation");
		}
		return out;
	}

	void parse_editor_metadata(AtlasMovie& p_movie, const std::vector<byte>& p_data) {
		Reader r{ p_data, "APM1 editor metadata" };
		require(r.bytes(4) == std::vector<byte>({ 'A','P','M','1' }), "bad APM1 magic");
		require(r.u8() == 1, "unsupported APM1 version");
		require(r.u8() == p_movie.tracks.size(), "APM1 track count does not match movie");
		for (auto& track : p_movie.tracks) {
			track.editor_name = r.ascii(r.u8());
			track.editor_color = r.u32();
			track.editor_group = r.ascii(r.u8());
			const auto waypoints{ r.u8() };
			for (byte i{ 0 }; i < waypoints; ++i)
				track.editor_waypoints.push_back({ r.u8(), r.u8() });
			const byte automatic{ r.u8() };
			require(automatic <= 1, "APM1 automatic-facing flag is invalid");
			track.editor_animation.automatic_facing = automatic != 0;
			auto frames = [&]() { return r.bytes(r.u8()); };
			track.editor_animation.idle = frames();
			track.editor_animation.left = frames();
			track.editor_animation.right = frames();
			track.editor_animation.toward = frames();
			track.editor_animation.away = frames();
			track.editor_animation.attack = frames();
			track.editor_animation.hurt = frames();
		}
		r.done(); ensure_editor_defaults(p_movie);
	}

	AtlasMovieBundle runtime_bundle(const AtlasMovieBundle& p_bundle) {
		AtlasMovieBundle result;
		for (const auto role : { AtlasMovieProjectRole::OfficialIntro, AtlasMovieProjectRole::OfficialEnding })
			for (const auto& movie : p_bundle.movies)
				if (movie.enabled && movie.project_role == role) result.movies.push_back(movie);
		for (const auto& movie : p_bundle.movies)
			if (movie.enabled && movie.project_role == AtlasMovieProjectRole::Normal)
				result.movies.push_back(movie);
		return result;
	}

	std::vector<std::size_t> metasprite_record_offsets(const std::vector<byte>& p_data,
		byte p_count) {
		require(p_count > 0 && p_data.size() >= static_cast<std::size_t>(p_count) * 2,
			"imported metasprite library is truncated");
		std::vector<std::size_t> result;
		std::size_t cursor{ static_cast<std::size_t>(p_count) * 2 };
		for (byte frame{ 0 }; frame < p_count; ++frame) {
			result.push_back(cursor);
			require(cursor + 4 <= p_data.size(), "imported metasprite header is truncated");
			const byte width{ p_data[cursor + 2] }, height{ p_data[cursor + 3] };
			require(width > 0 && height > 0
				&& width <= AtlasMovieBundleCodec::METASPRITE_MAX_WIDTH
				&& height <= AtlasMovieBundleCodec::METASPRITE_MAX_HEIGHT,
				"imported metasprite dimensions are outside 1..16 by 1..30");
			cursor += 4;
			std::size_t visible_cells{};
			for (std::size_t cell{ 0 }; cell < static_cast<std::size_t>(width) * height; ++cell) {
				require(cursor < p_data.size(), "imported metasprite cells are truncated");
				if (p_data[cursor] == 0xff) ++cursor;
				else { cursor += 2; ++visible_cells; }
				require(cursor <= p_data.size(), "imported metasprite attribute is truncated");
			}
			require(visible_cells <= 64,
				"imported metasprite frame exceeds the NES 64-sprite OAM limit");
		}
		require(cursor == p_data.size(), "imported metasprite library has trailing bytes");
		return result;
	}

	std::vector<byte> relocate_metasprites(const AtlasMovieImport& p_import,
		std::uint16_t p_data_cpu);

	AtlasMovieAssetKind import_asset_kind(AtlasMovieImportKind p_kind) {
		switch (p_kind) {
		case AtlasMovieImportKind::SpriteChr: return AtlasMovieAssetKind::SpriteChr;
		case AtlasMovieImportKind::BackgroundChr: return AtlasMovieAssetKind::BackgroundChr;
		case AtlasMovieImportKind::Nametable: return AtlasMovieAssetKind::Nametable;
		case AtlasMovieImportKind::Palette: return AtlasMovieAssetKind::Palette;
		default: throw std::runtime_error("Atlas Movie Creator: ATI1 import has no asset binding");
		}
	}

	void validate_import_binding(AtlasMovie& p_movie,
		const AtlasMovieImport& p_import, std::uint16_t p_data_cpu,
		std::uint16_t p_data_size) {
		if (p_import.kind == AtlasMovieImportKind::MetaspriteLibrary) {
			require(p_import.destination == 0,
				"ATI1 metasprite destination must be zero");
			require(p_import.aux == p_movie.metasprite_count,
				"ATI1 metasprite count does not match FMV1");
			require(p_movie.metasprite_bank == 12
				&& p_movie.metasprite_pointer_lo == p_data_cpu
				&& p_movie.metasprite_pointer_hi
					== static_cast<std::uint16_t>(p_data_cpu + p_import.aux),
				"ATI1 metasprite payload is not bound to the FMV1 pointer tables");
			require(p_import.data == relocate_metasprites(p_import, p_data_cpu),
				"ATI1 metasprite record pointers are not canonical");
			return;
		}

		require(p_import.aux == 0, "ATI1 non-metasprite aux byte must be zero");
		const auto kind{ import_asset_kind(p_import.kind) };
		const auto destination_space{ kind == AtlasMovieAssetKind::Palette
			? AtlasMovieDestination::Ram : AtlasMovieDestination::Ppu };
		auto exact = [&](const AtlasMovieAsset& asset) {
			return asset.kind == kind && asset.bank == 12 && asset.cpu == p_data_cpu
				&& asset.destination_space == destination_space
				&& asset.destination == p_import.destination && asset.bytes == p_data_size;
		};

		if (p_import.kind == AtlasMovieImportKind::SpriteChr) {
			const auto found{ std::ranges::find_if(p_movie.assets, exact) };
			require(found != p_movie.assets.end(),
				"ATI1 sprite CHR binding does not exactly match FMV1");
			p_movie.assets.erase(found);
			return;
		}

		const auto found{ std::ranges::find_if(p_movie.assets,
			[kind](const auto& asset) { return asset.kind == kind; }) };
		require(found != p_movie.assets.end() && exact(*found),
			"ATI1 replacement binding does not exactly match FMV1");
	}

	std::vector<byte> relocate_metasprites(const AtlasMovieImport& p_import,
		std::uint16_t p_data_cpu) {
		auto result{ p_import.data };
		const auto offsets{ metasprite_record_offsets(result, p_import.aux) };
		for (std::size_t i{ 0 }; i < offsets.size(); ++i) {
			const auto address{ static_cast<std::size_t>(p_data_cpu) + offsets[i] };
			require(address < 0xc000, "imported metasprite record crosses bank 12");
			result[i] = static_cast<byte>(address & 0xff);
			result[p_import.aux + i] = static_cast<byte>(address >> 8);
		}
		return result;
	}

	template<class Enum>
	Enum checked_enum(byte p_value, byte p_min, byte p_max, const std::string& p_label) {
		require(p_value >= p_min && p_value <= p_max, p_label + " enum is invalid");
		return static_cast<Enum>(p_value);
	}

	AtlasMovieTrack parse_track(const std::vector<byte>& p_record) {
		Reader r{ p_record, "track record" };
		AtlasMovieTrack track;
		track.kind = checked_enum<AtlasMovieTrackKind>(r.u8(), 1, 3, "track kind");
		if (track.kind == AtlasMovieTrackKind::Path || track.kind == AtlasMovieTrackKind::Cyclic) {
			track.x = r.u8(); track.x_fraction = r.u8();
			track.y = r.u8(); track.y_fraction = r.u8();
			track.velocity_x = static_cast<std::int8_t>(r.u8());
			track.velocity_y = static_cast<std::int8_t>(r.u8());
			track.integrator_shift = r.u8();
		}
		if (track.kind == AtlasMovieTrackKind::Path) {
			track.coordinate = checked_enum<AtlasMovieCoordinate>(r.u8(), 1, 2, "path coordinate");
			track.comparison = checked_enum<AtlasMovieComparison>(r.u8(), 1, 2, "path comparison");
			const auto keyframe_count{ r.u8() };
			for (byte i{ 0 }; i < keyframe_count; ++i)
				track.keyframes.push_back({ r.u8(), static_cast<std::int8_t>(r.u8()), static_cast<std::int8_t>(r.u8()) });
			track.dwell_frames = r.u8();
			const auto stages{ r.u8() }, slots{ r.u8() };
			for (byte stage{ 0 }; stage < stages; ++stage)
				track.stage_frames.push_back(r.bytes(slots));
		}
		else if (track.kind == AtlasMovieTrackKind::Cyclic) {
			track.dwell_frames = r.u8();
			const auto visible{ r.u8() };
			track.reset_at_pose = r.u8();
			track.visible_frames = r.bytes(visible);
		}
		else {
			track.x = r.u8(); track.y = r.u8();
			track.counter_address = r.u16();
			track.counter_mask = r.u8();
			track.toggle_frames = r.bytes(r.u8());
		}
		r.done();
		return track;
	}

	AtlasMovie parse_movie(const std::vector<byte>& p_payload) {
		Reader r{ p_payload, "FMV1 movie" };
		require(r.bytes(4) == std::vector<byte>({ 'F','M','V','1' }), "bad FMV1 magic");
		require(r.u8() == FORMAT_VERSION, "unsupported FMV1 version");
		AtlasMovie movie;
		movie.exit_mode = checked_enum<AtlasMovieExit>(r.u8(), 1, 3, "exit mode");
		movie.entry_music = r.u8();
		const auto assets{ r.u8() }, tracks{ r.u8() }, phases{ r.u8() }, sfx{ r.u8() };
		movie.metasprite_bank = r.u8();
		movie.metasprite_pointer_lo = r.u16();
		movie.metasprite_pointer_hi = r.u16();
		movie.metasprite_count = r.u8();
		movie.id = r.ascii(r.u8());
		for (byte i{ 0 }; i < assets; ++i) {
			AtlasMovieAsset asset;
			asset.kind = checked_enum<AtlasMovieAssetKind>(r.u8(), 1, 4, "asset kind");
			asset.bank = r.u8(); asset.cpu = r.u16();
			asset.destination_space = checked_enum<AtlasMovieDestination>(r.u8(), 0, 1, "asset destination");
			asset.destination = r.u16(); asset.bytes = r.u16();
			movie.assets.push_back(asset);
		}
		for (byte i{ 0 }; i < tracks; ++i)
			movie.tracks.push_back(parse_track(r.bytes(r.u16())));
		for (byte i{ 0 }; i < sfx; ++i)
			movie.sfx.push_back({ r.u8(), r.u8(), r.u8(), r.u8(), r.u8(), r.u8(), r.u8() });
		for (byte i{ 0 }; i < phases; ++i) {
			AtlasMoviePhase phase;
			phase.update_mask = r.u8(); phase.draw_mask = r.u8();
			phase.enter_action = checked_enum<AtlasMovieEnterAction>(r.u8(), 0, 1, "phase enter action");
			phase.enter_value = r.u8();
			phase.effect = checked_enum<AtlasMovieEffect>(r.u8(), 0, 1, "phase effect");
			phase.effect_track = r.u8(); phase.effect_stage = r.u8(); phase.effect_period = r.u8();
			phase.effect_subtract = r.u8(); phase.effect_floor = r.u8();
			phase.condition = checked_enum<AtlasMovieCondition>(r.u8(), 1, 5, "phase condition");
			phase.condition_track = r.u8(); phase.condition_value = r.u16();
			movie.phases.push_back(phase);
		}
		r.done(); ensure_editor_defaults(movie);
		return movie;
	}

	std::pair<AtlasMovieBundle, std::size_t> parse_prefix(const std::vector<byte>& p_data) {
		Reader r{ p_data, "FMB1 bundle" };
		require(r.bytes(4) == std::vector<byte>({ 'F','M','B','1' }), "bad FMB1 magic");
		require(r.u8() == FORMAT_VERSION, "unsupported FMB1 version");
		const auto count{ r.u8() };
		require(count > 0, "bundle has no movies");
		AtlasMovieBundle bundle;
		for (byte i{ 0 }; i < count; ++i)
			bundle.movies.push_back(parse_movie(r.bytes(r.u16())));
		if (!bundle.movies.empty()) bundle.movies[0].project_role = AtlasMovieProjectRole::OfficialIntro;
		if (bundle.movies.size() > 1) bundle.movies[1].project_role = AtlasMovieProjectRole::OfficialEnding;
		return { bundle, r.cursor() };
	}

	std::pair<AtlasMovieBundle, std::size_t> parse_bundle(const std::vector<byte>& p_data) {
		auto [bundle, consumed]{ parse_prefix(p_data) };
		if (consumed + 4 > p_data.size()
			|| !std::equal(p_data.begin() + consumed, p_data.begin() + consumed + 4,
				std::array<byte, 4>{ 'A','T','I','1' }.begin()))
			return { std::move(bundle), consumed };
		Reader r{ p_data, "ATI1 imports", consumed, p_data.size() - consumed };
		require(r.bytes(4) == std::vector<byte>({ 'A','T','I','1' }), "bad ATI1 magic");
		require(r.u8() == IMPORT_VERSION, "unsupported ATI1 version");
		const auto import_count{ r.u16() };
		for (std::size_t i{ 0 }; i < import_count; ++i) {
			const byte movie_index{ r.u8() };
			require(movie_index < bundle.movies.size(), "ATI1 import references a missing movie");
			AtlasMovieImport imported;
			imported.kind = checked_enum<AtlasMovieImportKind>(r.u8(), 1, 5, "import kind");
			imported.destination = r.u16(); imported.aux = r.u8();
			imported.label = r.ascii(r.u8());
			const auto data_size{ r.u16() };
			const auto data_cpu_size{ static_cast<std::size_t>(BUNDLE_CPU) + r.cursor() };
			require(data_cpu_size <= 0xffff, "ATI1 import address exceeds CPU space");
			const auto data_cpu{ static_cast<std::uint16_t>(data_cpu_size) };
			imported.data = r.bytes(data_size);
			auto& movie{ bundle.movies[movie_index] };
			validate_import_binding(movie, imported, data_cpu, data_size);
			movie.imports.push_back(std::move(imported));
		}
		return { std::move(bundle), r.cursor() };
	}

	std::vector<byte> compile_track(const AtlasMovieTrack& p_track) {
		std::vector<byte> out{ static_cast<byte>(p_track.kind) };
		if (p_track.kind == AtlasMovieTrackKind::Path || p_track.kind == AtlasMovieTrackKind::Cyclic) {
			out.insert(out.end(), { p_track.x, p_track.x_fraction, p_track.y, p_track.y_fraction,
				static_cast<byte>(p_track.velocity_x), static_cast<byte>(p_track.velocity_y), p_track.integrator_shift });
		}
		if (p_track.kind == AtlasMovieTrackKind::Path) {
			out.insert(out.end(), { static_cast<byte>(p_track.coordinate), static_cast<byte>(p_track.comparison),
				static_cast<byte>(p_track.keyframes.size()) });
			for (const auto& keyframe : p_track.keyframes)
				out.insert(out.end(), { keyframe.threshold, static_cast<byte>(keyframe.velocity_x),
					static_cast<byte>(keyframe.velocity_y) });
			out.insert(out.end(), { p_track.dwell_frames, static_cast<byte>(p_track.stage_frames.size()),
				static_cast<byte>(p_track.stage_frames.front().size()) });
			for (const auto& row : p_track.stage_frames)
				out.insert(out.end(), row.begin(), row.end());
		}
		else if (p_track.kind == AtlasMovieTrackKind::Cyclic) {
			out.insert(out.end(), { p_track.dwell_frames, static_cast<byte>(p_track.visible_frames.size()),
				p_track.reset_at_pose });
			out.insert(out.end(), p_track.visible_frames.begin(), p_track.visible_frames.end());
		}
		else {
			out.insert(out.end(), { p_track.x, p_track.y });
			append_word(out, p_track.counter_address);
			out.insert(out.end(), { p_track.counter_mask, static_cast<byte>(p_track.toggle_frames.size()) });
			out.insert(out.end(), p_track.toggle_frames.begin(), p_track.toggle_frames.end());
		}
		return out;
	}

	std::vector<byte> compile_movie(const AtlasMovie& p_movie) {
		std::vector<byte> out{ 'F','M','V','1', FORMAT_VERSION,
			static_cast<byte>(p_movie.exit_mode), p_movie.entry_music,
			static_cast<byte>(p_movie.assets.size()), static_cast<byte>(p_movie.tracks.size()),
			static_cast<byte>(p_movie.phases.size()), static_cast<byte>(p_movie.sfx.size()),
			p_movie.metasprite_bank };
			append_word(out, p_movie.metasprite_pointer_lo);
			append_word(out, p_movie.metasprite_pointer_hi);
		out.insert(out.end(), { p_movie.metasprite_count, static_cast<byte>(p_movie.id.size()) });
		out.insert(out.end(), p_movie.id.begin(), p_movie.id.end());
		for (const auto& asset : p_movie.assets) {
			out.insert(out.end(), { static_cast<byte>(asset.kind), asset.bank });
			append_word(out, asset.cpu);
			out.push_back(static_cast<byte>(asset.destination_space));
			append_word(out, asset.destination); append_word(out, asset.bytes);
		}
		for (const auto& track : p_movie.tracks) {
			const auto record{ compile_track(track) };
			append_word(out, static_cast<std::uint16_t>(record.size()));
			out.insert(out.end(), record.begin(), record.end());
		}
		for (const auto& event : p_movie.sfx)
			out.insert(out.end(), { event.track, event.sound, event.stage_lt, event.tick_mask,
				event.tick_value, event.slot_mask, event.slot_value });
		for (const auto& phase : p_movie.phases) {
			out.insert(out.end(), { phase.update_mask, phase.draw_mask,
				static_cast<byte>(phase.enter_action), phase.enter_value,
				static_cast<byte>(phase.effect), phase.effect_track, phase.effect_stage,
				phase.effect_period, phase.effect_subtract, phase.effect_floor,
				static_cast<byte>(phase.condition), phase.condition_track });
			append_word(out, phase.condition_value);
		}
		return out;
	}

	std::vector<byte> compile_movies_prefix(const AtlasMovieBundle& p_bundle) {
		std::vector<byte> out{ 'F','M','B','1', FORMAT_VERSION, static_cast<byte>(p_bundle.movies.size()) };
		for (const auto& movie : p_bundle.movies) {
			const auto payload{ compile_movie(movie) };
			require(payload.size() <= 0xffff, "compiled movie exceeds 65535 bytes");
			append_word(out, static_cast<std::uint16_t>(payload.size()));
			out.insert(out.end(), payload.begin(), payload.end());
		}
		return out;
	}

	struct ImportBinding {
		std::size_t movie, import;
		std::optional<std::size_t> asset;
	};

	std::vector<byte> compile_bundle_unchecked(const AtlasMovieBundle& p_bundle) {
		std::size_t import_count{ 0 };
		for (const auto& movie : p_bundle.movies) import_count += movie.imports.size();
		if (import_count == 0) return compile_movies_prefix(p_bundle);

		AtlasMovieBundle prepared{ p_bundle };
		std::vector<ImportBinding> bindings;
		for (std::size_t movie_index{ 0 }; movie_index < prepared.movies.size(); ++movie_index) {
			auto& movie{ prepared.movies[movie_index] };
			for (std::size_t import_index{ 0 }; import_index < movie.imports.size(); ++import_index) {
				const auto& imported{ movie.imports[import_index] };
				std::optional<std::size_t> asset_index;
				if (imported.kind == AtlasMovieImportKind::MetaspriteLibrary) {
					movie.metasprite_bank = 12; movie.metasprite_count = imported.aux;
					movie.metasprite_pointer_lo = movie.metasprite_pointer_hi = BUNDLE_CPU;
				}
				else {
					const auto asset_kind{ static_cast<AtlasMovieAssetKind>(static_cast<byte>(imported.kind) == 1
						? 1 : static_cast<byte>(imported.kind) - 1) };
					if (imported.kind == AtlasMovieImportKind::SpriteChr) {
						movie.assets.push_back({ asset_kind, 12, BUNDLE_CPU, AtlasMovieDestination::Ppu,
							imported.destination, static_cast<std::uint16_t>(imported.data.size()) });
						asset_index = movie.assets.size() - 1;
					}
					else {
						const auto found{ std::ranges::find_if(movie.assets,
							[asset_kind](const auto& asset) { return asset.kind == asset_kind; }) };
						require(found != movie.assets.end(), "import replaces a missing movie asset kind");
						asset_index = static_cast<std::size_t>(found - movie.assets.begin());
						found->bank = 12; found->cpu = BUNDLE_CPU;
						found->bytes = static_cast<std::uint16_t>(imported.data.size());
						found->destination = imported.destination;
					}
				}
				bindings.push_back({ movie_index, import_index, asset_index });
			}
		}

		const auto placeholder_prefix{ compile_movies_prefix(prepared) };
		std::vector<byte> trailer{ 'A','T','I','1', IMPORT_VERSION };
		append_word(trailer, static_cast<std::uint16_t>(import_count));
		for (const auto& binding : bindings) {
			auto& movie{ prepared.movies[binding.movie] };
			const auto& imported{ movie.imports[binding.import] };
			trailer.insert(trailer.end(), { static_cast<byte>(binding.movie),
				static_cast<byte>(imported.kind) });
			append_word(trailer, imported.destination);
			trailer.insert(trailer.end(), { imported.aux, static_cast<byte>(imported.label.size()) });
			trailer.insert(trailer.end(), imported.label.begin(), imported.label.end());
			append_word(trailer, static_cast<std::uint16_t>(imported.data.size()));
			const auto data_cpu_size{ static_cast<std::size_t>(BUNDLE_CPU)
				+ placeholder_prefix.size() + trailer.size() };
			require(data_cpu_size <= 0xffff, "import address exceeds CPU space");
			const auto data_cpu{ static_cast<std::uint16_t>(data_cpu_size) };
			if (imported.kind == AtlasMovieImportKind::MetaspriteLibrary) {
				movie.metasprite_pointer_lo = data_cpu;
				movie.metasprite_pointer_hi = static_cast<std::uint16_t>(data_cpu + imported.aux);
				const auto relocated{ relocate_metasprites(imported, data_cpu) };
				trailer.insert(trailer.end(), relocated.begin(), relocated.end());
			}
			else {
				movie.assets[*binding.asset].cpu = data_cpu;
				trailer.insert(trailer.end(), imported.data.begin(), imported.data.end());
			}
		}
		const auto prefix{ compile_movies_prefix(prepared) };
		require(prefix.size() == placeholder_prefix.size(), "import relocation changed movie record size");
		std::vector<byte> result{ prefix };
		result.insert(result.end(), trailer.begin(), trailer.end());
		return result;
	}

	void require_canonical_encoding(const AtlasMovieBundle& p_bundle,
		const std::vector<byte>& p_data, std::size_t p_consumed) {
		const auto canonical{ compile_bundle_unchecked(p_bundle) };
		require(canonical.size() == p_consumed
			&& std::equal(canonical.begin(), canonical.end(), p_data.begin()),
			"FMB1/ATI1 encoding is not canonical");
	}

	void write_word(std::vector<byte>& p_data, std::size_t p_offset, std::uint16_t p_value) {
		require(p_offset + 2 <= p_data.size(), "ROM write is outside file");
		p_data[p_offset] = static_cast<byte>(p_value & 0xff);
		p_data[p_offset + 1] = static_cast<byte>(p_value >> 8);
	}

	std::uint16_t read_word(const std::vector<byte>& p_data, std::size_t p_offset) {
		require(p_offset + 2 <= p_data.size(), "word read is outside file");
		return static_cast<std::uint16_t>(p_data[p_offset] | p_data[p_offset + 1] << 8);
	}

}

fe::AtlasMovieBundle fe::AtlasMovieBundleCodec::parse(const std::vector<byte>& p_fmb) {
	const auto [bundle, consumed]{ parse_bundle(p_fmb) };
	require(consumed == p_fmb.size(), "FMB1 bundle has trailing bytes");
	validate(bundle);
	require_canonical_encoding(bundle, p_fmb, consumed);
	return bundle;
}

std::size_t fe::AtlasMovieBundleCodec::validated_prefix_size(
	const std::vector<byte>& p_data) {
	const auto [bundle, consumed]{ parse_bundle(p_data) };
	validate(bundle);
	require_canonical_encoding(bundle, p_data, consumed);
	return consumed;
}

fe::AtlasMovieBundleReport fe::AtlasMovieBundleCodec::validate(const AtlasMovieBundle& p_bundle) {
	require(!p_bundle.movies.empty(), "bundle needs at least one movie");
	require(p_bundle.movies.size() <= 0xffff, "project movie count exceeds 65535");
	std::size_t official_intro{ 0 }, official_ending{ 0 };
	bool intro_enabled{ false }, ending_enabled{ false };
	for (const auto& movie : p_bundle.movies) {
		require(static_cast<byte>(movie.project_role) <= 2, "movie project role is invalid");
		if (movie.project_role == AtlasMovieProjectRole::OfficialIntro) {
			++official_intro; intro_enabled = movie.enabled;
		}
		if (movie.project_role == AtlasMovieProjectRole::OfficialEnding) {
			++official_ending; ending_enabled = movie.enabled;
		}
	}
	require(official_intro == 1 && official_ending == 1 && intro_enabled && ending_enabled,
		"project needs exactly one enabled official intro and one enabled official ending");
	const auto included{ runtime_bundle(p_bundle) };
	require(included.movies.size() >= 2, "project must include official intro and ending movies");
	require_u8(included.movies.size(), "included movie count");
	std::set<std::string> ids;
	std::size_t total_imports{ 0 };
	for (const auto& movie : p_bundle.movies) {
		require(!movie.id.empty() && movie.id.size() <= 255, "movie ID must contain 1..255 characters");
		require(std::all_of(movie.id.begin(), movie.id.end(), [](unsigned char c) { return c >= 0x20 && c <= 0x7e; }),
			"movie ID must be printable ASCII");
		require(ids.insert(movie.id).second, "movie IDs must be unique");
		const auto exit_mode{ static_cast<byte>(movie.exit_mode) };
		require(exit_mode >= 1 && exit_mode <= 3, "movie exit mode is invalid");
		require(movie.entry_music <= 16 || movie.entry_music == 0xfe
			|| movie.entry_music == 0xff,
			"entry music must be stock ID 0..16, $FE stop, or $FF keep");
		require(movie.assets.size() == 4, "each movie must upload four assets");
		require(!movie.tracks.empty() && movie.tracks.size() <= 8, "movie needs 1..8 tracks");
		require(!movie.phases.empty() && movie.phases.size() <= 255, "movie needs 1..255 phases");
		require_u8(movie.sfx.size(), "SFX count");
		require(movie.metasprite_bank == 12 && movie.metasprite_count > 0,
			"metasprite library must be a nonempty bank-12 table");
		for (const auto pointer : { movie.metasprite_pointer_lo, movie.metasprite_pointer_hi })
			require(pointer >= 0x8000 && static_cast<std::size_t>(pointer) + movie.metasprite_count <= 0xc000,
				"metasprite pointer table crosses bank 12");
		std::set<byte> asset_kinds;
		for (const auto& asset : movie.assets) {
			const auto kind{ static_cast<byte>(asset.kind) };
			require(kind >= 1 && kind <= 4, "asset kind is invalid");
			require(asset_kinds.insert(kind).second, "asset kinds must be unique");
		}
		std::set<byte> singleton_imports;
		auto effective_assets{ movie.assets };
		for (const auto& imported : movie.imports) {
			++total_imports;
			const byte kind{ static_cast<byte>(imported.kind) };
			require(kind >= 1 && kind <= 5, "import kind is invalid");
			require(imported.label.size() <= 255
				&& std::all_of(imported.label.begin(), imported.label.end(),
					[](unsigned char c) { return c >= 0x20 && c <= 0x7e; }),
				"import label must be printable ASCII and at most 255 characters");
			require(!imported.data.empty() && imported.data.size() <= 0xffff,
				"import payload must contain 1..65535 bytes");
			if (imported.kind == AtlasMovieImportKind::MetaspriteLibrary) {
				require(singleton_imports.insert(kind).second,
					"movie has multiple imported metasprite libraries");
				require(imported.destination == 0,
					"imported metasprite destination must be zero");
				require(imported.aux == movie.metasprite_count,
					"imported metasprite count must match the movie frame count");
				(void)metasprite_record_offsets(imported.data, imported.aux);
			}
			else if (imported.kind == AtlasMovieImportKind::SpriteChr) {
				require(imported.aux == 0, "non-metasprite import aux byte must be zero");
				effective_assets.push_back({ AtlasMovieAssetKind::SpriteChr, 12, BUNDLE_CPU,
					AtlasMovieDestination::Ppu, imported.destination,
					static_cast<std::uint16_t>(imported.data.size()) });
			}
			else {
				require(imported.aux == 0, "non-metasprite import aux byte must be zero");
				require(singleton_imports.insert(kind).second,
					"movie has multiple imports replacing the same asset kind");
				const auto asset_kind{ static_cast<AtlasMovieAssetKind>(kind - 1) };
				const auto found{ std::ranges::find_if(effective_assets,
					[asset_kind](const auto& asset) { return asset.kind == asset_kind; }) };
				require(found != effective_assets.end(), "import replaces a missing movie asset kind");
				found->bank = 12;
				found->cpu = BUNDLE_CPU;
				found->destination = imported.destination;
				found->bytes = static_cast<std::uint16_t>(imported.data.size());
			}
		}

		std::size_t palette_assets{};
		std::vector<std::pair<std::size_t, std::size_t>> ppu_ranges;
		for (const auto& asset : effective_assets) {
			const auto kind{ static_cast<byte>(asset.kind) };
			require(kind >= 1 && kind <= 4, "asset kind is invalid");
			require(asset.bank < 16 && asset.cpu >= 0x8000 && asset.cpu < 0xc000,
				"asset source is outside a switchable bank");
			require(asset.bytes > 0 && static_cast<std::size_t>(asset.cpu) + asset.bytes <= 0xc000,
				"asset crosses its bank boundary");
			const auto destination{ static_cast<std::size_t>(asset.destination) + asset.bytes };
			if (asset.destination_space == AtlasMovieDestination::Ppu) {
				require(asset.kind != AtlasMovieAssetKind::Palette && asset.bytes <= 4096
					&& asset.bytes % 16 == 0,
					"PPU assets must contain 1..4096 complete 16-byte blocks");
				if (asset.kind == AtlasMovieAssetKind::SpriteChr
					|| asset.kind == AtlasMovieAssetKind::BackgroundChr)
					require(asset.destination % 16 == 0,
						"CHR destination must be aligned to a 16-byte tile");
				if (asset.kind == AtlasMovieAssetKind::SpriteChr)
					require(destination <= 0x1000, "sprite CHR crosses pattern table $0FFF");
				else if (asset.kind == AtlasMovieAssetKind::BackgroundChr)
					require(asset.destination >= 0x1000 && destination <= 0x2000,
						"background CHR must stay in pattern table $1000-$1FFF");
				else if (asset.kind == AtlasMovieAssetKind::Nametable)
					require(asset.destination == 0x2000 && asset.bytes == 0x0400,
						"nametable must be exactly 1024 bytes at $2000");
				for (const auto [start, end] : ppu_ranges)
					require(asset.destination >= end || destination <= start,
						"PPU asset destinations overlap");
				ppu_ranges.push_back({ asset.destination, destination });
			}
			else {
				require(asset.destination_space == AtlasMovieDestination::Ram
					&& asset.kind == AtlasMovieAssetKind::Palette && asset.bank == 12
					&& asset.destination == 0x0293 && asset.bytes == 32,
					"RAM asset must be the 32-byte bank-12 palette copied to $0293");
				++palette_assets;
			}
		}
		require(palette_assets == 1, "movie needs exactly one RAM palette asset");
		for (const auto& track : movie.tracks) {
			require(track.editor_name.size() <= 63, "actor name exceeds 63 characters");
			require(track.editor_group.size() <= 63, "actor group exceeds 63 characters");
			require(track.editor_waypoints.size() <= 16, "actor path exceeds sixteen editable waypoints");
			auto validate_editor_frames = [&](const std::vector<byte>& frames, const std::string& label) {
				for (const auto frame : frames)
					require(frame < movie.metasprite_count, label + " references a missing metasprite frame");
			};
			validate_editor_frames(track.editor_animation.idle, "idle animation");
			validate_editor_frames(track.editor_animation.left, "left animation");
			validate_editor_frames(track.editor_animation.right, "right animation");
			validate_editor_frames(track.editor_animation.toward, "toward animation");
			validate_editor_frames(track.editor_animation.away, "away animation");
			validate_editor_frames(track.editor_animation.attack, "attack animation");
			validate_editor_frames(track.editor_animation.hurt, "hurt animation");
			const auto track_kind{ static_cast<byte>(track.kind) };
			require(track_kind >= 1 && track_kind <= 3, "track kind is invalid");
			if (track.kind == AtlasMovieTrackKind::Path || track.kind == AtlasMovieTrackKind::Cyclic)
				require(track.integrator_shift >= 1 && track.integrator_shift <= 8, "integrator shift must be 1..8");
			if (track.kind == AtlasMovieTrackKind::Path) {
				require(static_cast<byte>(track.coordinate) >= 1
					&& static_cast<byte>(track.coordinate) <= 2
					&& static_cast<byte>(track.comparison) >= 1
					&& static_cast<byte>(track.comparison) <= 2,
					"path gate coordinate/comparison is invalid");
				require(!track.keyframes.empty() && track.keyframes.size() <= 15, "path needs 1..15 keyframes");
				require(track.stage_frames.size() == track.keyframes.size() + 1, "path frame stages must equal keyframes + 1");
				require(track.dwell_frames > 0, "path dwell cannot be zero");
				const auto slots{ track.stage_frames.front().size() };
				require(slots > 0 && slots <= 255, "path stages need 1..255 animation slots");
				for (const auto& row : track.stage_frames)
				{
					require(row.size() == slots, "path animation rows must have equal slot counts");
					for (const auto frame : row)
						require(frame < movie.metasprite_count, "path references a missing metasprite frame");
				}
			}
			else if (track.kind == AtlasMovieTrackKind::Cyclic) {
				require(track.dwell_frames > 0, "cyclic dwell cannot be zero");
				require(!track.visible_frames.empty() && track.visible_frames.size() < track.reset_at_pose,
					"cyclic visible frames must end before reset pose");
				for (const auto frame : track.visible_frames)
					require(frame < movie.metasprite_count, "cyclic track references a missing metasprite frame");
			}
			else {
				require(track.counter_mask > 0 && track.toggle_frames.size() == 2,
					"counter toggle needs a mask and exactly two frames");
				require(track.counter_address <= 0x07ff,
					"counter toggle address must be internal RAM $0000-$07FF");
				for (const auto frame : track.toggle_frames)
					require(frame < movie.metasprite_count, "counter toggle references a missing metasprite frame");
			}
			require(compile_track(track).size() <= 255,
				"track record exceeds the movie player's 255-byte indexing limit");
		}
		const auto allowed_mask{ static_cast<unsigned>((1u << movie.tracks.size()) - 1u) };
		for (const auto& phase : movie.phases) {
			require(static_cast<byte>(phase.enter_action) <= 1, "phase enter action is invalid");
			require(static_cast<byte>(phase.effect) <= 1, "phase effect is invalid");
			const auto condition{ static_cast<byte>(phase.condition) };
			require(condition >= 1 && condition <= 5, "phase condition is invalid");
			require(((phase.update_mask | phase.draw_mask) & ~allowed_mask) == 0,
				"phase masks reference missing tracks");
			if (phase.effect == AtlasMovieEffect::PaletteFade)
				require(phase.effect_track < movie.tracks.size() && phase.effect_period > 0,
					"palette fade needs a valid track and nonzero period");
			if (phase.condition == AtlasMovieCondition::TrackYGte)
				require(phase.condition_track < movie.tracks.size() && phase.condition_value <= 0xff,
					"track-Y phase condition needs a valid track and an 8-bit threshold");
		}
		for (const auto& event : movie.sfx) {
			require(event.track < movie.tracks.size(), "SFX references a missing track");
			require(movie.tracks[event.track].kind == AtlasMovieTrackKind::Path,
				"SFX triggers require a path track");
			require((event.tick_value & ~event.tick_mask) == 0 && (event.slot_value & ~event.slot_mask) == 0,
				"SFX trigger value escapes its mask");
		}
	}
	require(total_imports <= 0xffff, "ATI1 import count exceeds 65535");
	const auto bytes{ compile_bundle_unchecked(included).size() };
	const auto end{ static_cast<std::size_t>(BUNDLE_CPU) + bytes + DISPATCH_BYTES };
	if (end > 0xc000) {
		const auto capacity{ static_cast<std::size_t>(0xc000 - BUNDLE_CPU) };
		const auto used{ bytes + DISPATCH_BYTES };
		throw std::runtime_error(std::format(
			"Atlas Movie Creator: bank 12 overflow by {} bytes ({} used / {} available, including {} dispatch bytes)",
			used - capacity, used, capacity, DISPATCH_BYTES));
	}
	return { bytes, static_cast<std::uint16_t>(end),
		{ "Sprite/OAM budgets require validation against the selected ROM metasprite library." } };
}

std::vector<byte> fe::AtlasMovieBundleCodec::compile(const AtlasMovieBundle& p_bundle) {
	validate(p_bundle);
	return compile_bundle_unchecked(runtime_bundle(p_bundle));
}

fe::AtlasMovieDetailedBudget fe::AtlasMovieBundleCodec::detailed_budget(
	const AtlasMovieBundle& p_bundle) {
	validate(p_bundle);
	const auto included{ runtime_bundle(p_bundle) };
	AtlasMovieDetailedBudget result;
	result.bundle_header_bytes = 6 + included.movies.size() * 2;
	std::size_t import_count{};
	for (const auto& movie : included.movies) import_count += movie.imports.size();
	result.import_header_bytes = import_count ? 7 : 0;
	for (const auto& movie : included.movies) {
		std::size_t movie_bytes{ compile_movie(movie).size() };
		for (const auto& imported : movie.imports)
			if (imported.kind == AtlasMovieImportKind::SpriteChr) movie_bytes += 10;
		result.movies.push_back({ movie.id, movie_bytes + 2 });
		for (std::size_t i{ 0 }; i < movie.tracks.size(); ++i)
			result.tracks.push_back({ std::format("{} / {}", movie.id,
				movie.tracks[i].editor_name.empty() ? std::format("Actor {}", i + 1) : movie.tracks[i].editor_name),
				compile_track(movie.tracks[i]).size() + 2 });
		for (std::size_t i{ 0 }; i < movie.assets.size(); ++i)
			result.assets.push_back({ std::format("{} / asset {}", movie.id, i), 9 });
		for (const auto& imported : movie.imports) {
			result.imports.push_back({ std::format("{} / {}", movie.id, imported.label),
				8 + imported.label.size() + imported.data.size() });
			if (imported.kind == AtlasMovieImportKind::SpriteChr)
				result.assets.push_back({ std::format("{} / {} CHR descriptor", movie.id, imported.label), 9 });
		}
		for (std::size_t i{ 0 }; i < movie.phases.size(); ++i)
			result.phases.push_back({ std::format("{} / phase {}", movie.id, i), 14 });
		for (std::size_t i{ 0 }; i < movie.sfx.size(); ++i)
			result.events.push_back({ std::format("{} / SFX {}", movie.id, i), 7 });
	}
	for (std::size_t movie_a{ 0 }; movie_a < included.movies.size(); ++movie_a)
		for (std::size_t import_a{ 0 }; import_a < included.movies[movie_a].imports.size(); ++import_a)
			for (std::size_t movie_b{ movie_a }; movie_b < included.movies.size(); ++movie_b)
				for (std::size_t import_b{ movie_b == movie_a ? import_a + 1 : 0 };
					import_b < included.movies[movie_b].imports.size(); ++import_b) {
					const auto& a{ included.movies[movie_a].imports[import_a] };
					const auto& b{ included.movies[movie_b].imports[import_b] };
					if (a.kind == b.kind && a.data == b.data)
						result.suggestions.push_back(std::format(
							"Duplicate {}-byte import: {}/{} and {}/{}; reuse one ROM-owned asset or shared import",
							a.data.size(), included.movies[movie_a].id, a.label,
							included.movies[movie_b].id, b.label));
				}
	if (!result.imports.empty())
		result.suggestions.push_back("Imported graphics dominate quickly; prefer existing ROM CHR/nametables when the scene permits it.");
	return result;
}

std::vector<byte> fe::AtlasMovieBundleCodec::compile_project(const AtlasMovieBundle& p_bundle) {
	validate(p_bundle);
	std::vector<byte> result{ 'A','M','P','1', PROJECT_VERSION };
	append_word(result, static_cast<std::uint16_t>(p_bundle.movies.size()));
	for (const auto& movie : p_bundle.movies) {
		AtlasMovie stored{ movie }; stored.enabled = true;
		const auto fmb{ compile_bundle_unchecked(AtlasMovieBundle{ { std::move(stored) } }) };
		require(fmb.size() <= std::numeric_limits<std::uint32_t>::max(), "project movie blob exceeds 4 GiB");
		result.push_back(static_cast<byte>(movie.enabled));
		result.push_back(static_cast<byte>(movie.project_role));
		dword(result, static_cast<std::uint32_t>(fmb.size()));
		result.insert(result.end(), fmb.begin(), fmb.end());
		const auto editor{ compile_editor_metadata(movie) };
		dword(result, static_cast<std::uint32_t>(editor.size()));
		result.insert(result.end(), editor.begin(), editor.end());
	}
	return result;
}

fe::AtlasMovieBundle fe::AtlasMovieBundleCodec::parse_project(const std::vector<byte>& p_amp) {
	Reader r{ p_amp, "AMP1 project" };
	require(r.bytes(4) == std::vector<byte>({ 'A','M','P','1' }), "bad AMP1 magic");
	const byte project_version{ r.u8() };
	require(project_version >= 1 && project_version <= PROJECT_VERSION, "unsupported AMP1 version");
	const auto count{ r.u16() };
	AtlasMovieBundle result;
	for (std::size_t i{ 0 }; i < count; ++i) {
		const byte enabled{ r.u8() };
		require(enabled <= 1, "AMP1 enabled flag is invalid");
		const auto role{ checked_enum<AtlasMovieProjectRole>(r.u8(), 0, 2, "AMP1 project role") };
		const std::uint32_t bytes{ r.u32() };
		const auto blob{ r.bytes(bytes) };
		auto [single, consumed]{ parse_bundle(blob) };
		require(consumed == blob.size(), "AMP1 movie blob has trailing bytes");
		require(single.movies.size() == 1, "AMP1 entry must contain exactly one movie");
		require_canonical_encoding(single, blob, consumed);
		single.movies.front().enabled = enabled != 0;
		single.movies.front().project_role = role;
		if (project_version >= 2)
			parse_editor_metadata(single.movies.front(), r.bytes(r.u32()));
		else ensure_editor_defaults(single.movies.front());
		result.movies.push_back(std::move(single.movies.front()));
	}
	r.done(); validate(result);
	return result;
}

std::vector<byte> fe::AtlasMovieBundleCodec::extract_from_ame(const std::vector<byte>& p_ame) {
	AtlasMovieEngine::validate_package(p_ame);
	const auto core{ read_word(p_ame, 5) }, tail{ read_word(p_ame, 7) }, bundle{ read_word(p_ame, 9) };
	std::vector<byte> result(p_ame.begin() + 11 + core + tail, p_ame.end());
	return result;
}

std::vector<byte> fe::AtlasMovieBundleCodec::replace_in_ame(const std::vector<byte>& p_ame,
	const AtlasMovieBundle& p_bundle) {
	(void)extract_from_ame(p_ame);
	validate(p_bundle);
	const auto fmb{ compile(p_bundle) };
	const auto core{ read_word(p_ame, 5) }, tail{ read_word(p_ame, 7) };
	std::vector<byte> result(p_ame.begin(), p_ame.begin() + 11 + core + tail);
	write_word(result, 9, static_cast<std::uint16_t>(fmb.size()));
	result.insert(result.end(), fmb.begin(), fmb.end());
	return result;
}

std::vector<byte> fe::AtlasMovieBundleCodec::extract_from_installed_rom(const std::vector<byte>& p_rom) {
	require(AtlasMovieEngine::is_installed(p_rom), "Atlas Movie Engine is not installed");
	const auto start{ file_offset(12, BUNDLE_CPU) };
	require(start < p_rom.size(), "installed bundle address is outside ROM");
	std::vector<byte> bank_tail(p_rom.begin() + start,
		p_rom.begin() + std::min(p_rom.size(), file_offset(12, 0xbfff) + 1));
	const auto [bundle, consumed]{ parse_bundle(bank_tail) };
	validate(bundle);
	require_canonical_encoding(bundle, bank_tail, consumed);
	return std::vector<byte>(bank_tail.begin(), bank_tail.begin() + consumed);
}

fe::AtlasMovieBundleReport fe::AtlasMovieBundleCodec::replace_in_installed_rom(
	std::vector<byte>& p_rom, const AtlasMovieBundle& p_bundle) {
	const auto old_fmb{ extract_from_installed_rom(p_rom) };
	const auto old_low{ static_cast<std::uint16_t>(BUNDLE_CPU + old_fmb.size()) };
	const auto old_high{ static_cast<std::uint16_t>(old_low + VANILLA_HANDLER_COUNT) };
	require(read_word(p_rom, file_offset(12, DISPATCH_LOW_REF)) == old_low
		&& read_word(p_rom, file_offset(12, DISPATCH_HIGH_REF)) == old_high,
		"active dispatch tables were relocated; export an AME before adding generated opcodes");
	for (std::size_t i{ 0 }; i < HANDLERS.size(); ++i) {
		require(p_rom[file_offset(12, old_low) + i] == (HANDLERS[i] & 0xff)
			&& p_rom[file_offset(12, old_high) + i] == (HANDLERS[i] >> 8),
			"active opcode tables are not the plain Atlas layout");
	}

	const auto report{ validate(p_bundle) };
	const auto fmb{ compile(p_bundle) };
	const auto new_low{ static_cast<std::uint16_t>(BUNDLE_CPU + fmb.size()) };
	const auto new_high{ static_cast<std::uint16_t>(new_low + VANILLA_HANDLER_COUNT) };
	const auto old_end{ static_cast<std::uint16_t>(old_high + VANILLA_HANDLER_COUNT) };
	const auto new_end{ static_cast<std::uint16_t>(new_high + VANILLA_HANDLER_COUNT) };
	std::fill(p_rom.begin() + file_offset(12, BUNDLE_CPU),
		p_rom.begin() + file_offset(12, std::max(old_end, new_end)), 0xff);
	std::copy(fmb.begin(), fmb.end(), p_rom.begin() + file_offset(12, BUNDLE_CPU));
	for (std::size_t i{ 0 }; i < HANDLERS.size(); ++i) {
		p_rom[file_offset(12, new_low) + i] = static_cast<byte>(HANDLERS[i] & 0xff);
		p_rom[file_offset(12, new_high) + i] = static_cast<byte>(HANDLERS[i] >> 8);
	}
	write_word(p_rom, file_offset(12, DISPATCH_LOW_REF), new_low);
	write_word(p_rom, file_offset(12, DISPATCH_HIGH_REF), new_high);
	return report;
}
