#include "fe/AtlasMovieBundle.h"
#include "fe/AtlasMovieAssets.h"
#include "fe/AtlasMovieEditor.h"
#include "fe/AtlasMovieEngine.h"
#include "fe/AtlasMovieLayout.h"
#include "fe/AtlasMoviePreview.h"
#include "fe/AtlasMovieRuntime.h"
#include "common/klib/Kstring.h"
#include "fi/Opcode.h"
#include "common/klib/Kfile.h"
#include "common/pugixml/pugixml.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using byte = unsigned char;

namespace {

#include "fe/AtlasMovieEngineData.inc"

	std::vector<byte> read_bytes(const char* p_path) {
		std::ifstream stream(p_path, std::ios::binary);
		if (!stream) throw std::runtime_error(p_path);
		return { std::istreambuf_iterator<char>(stream), {} };
	}

	void write_bytes(const char* p_path, const std::vector<byte>& p_data) {
		std::ofstream stream(p_path, std::ios::binary);
		if (!stream) throw std::runtime_error(p_path);
		stream.write(reinterpret_cast<const char*>(p_data.data()),
			static_cast<std::streamsize>(p_data.size()));
		if (!stream) throw std::runtime_error(p_path);
	}

	void require(bool p_value, const char* p_label) {
		if (!p_value) throw std::runtime_error(p_label);
		std::cout << "PASS " << p_label << '\n';
	}

	template<class Action>
	void require_rejected(Action&& p_action, const char* p_label) {
		try {
			p_action();
		}
		catch (const std::exception&) {
			std::cout << "PASS " << p_label << '\n';
			return;
		}
		throw std::runtime_error(p_label);
	}

	void append_word(std::vector<byte>& p_data, std::uint16_t p_value) {
		p_data.push_back(static_cast<byte>(p_value));
		p_data.push_back(static_cast<byte>(p_value >> 8));
	}

	std::uint16_t read_word(const std::vector<byte>& p_data, std::size_t p_offset) {
		if (p_offset + 2 > p_data.size()) throw std::runtime_error("truncated test word");
		return static_cast<std::uint16_t>(p_data[p_offset] | p_data[p_offset + 1] << 8);
	}

	std::uint32_t read_dword(const std::vector<byte>& p_data, std::size_t p_offset) {
		if (p_offset + 4 > p_data.size()) throw std::runtime_error("truncated test dword");
		std::uint32_t result{};
		for (unsigned shift{}; shift < 32; shift += 8)
			result |= static_cast<std::uint32_t>(p_data[p_offset + shift / 8]) << shift;
		return result;
	}

	std::uint64_t fnv1a(const std::vector<byte>& p_data, std::size_t p_size) {
		if (p_size > p_data.size()) throw std::runtime_error("truncated hash input");
		std::uint64_t result{ 0xcbf29ce484222325ULL };
		for (std::size_t i{}; i < p_size; ++i) {
			result ^= p_data[i];
			result *= 0x100000001b3ULL;
		}
		return result;
	}

	std::size_t rom_offset(byte p_bank, std::uint16_t p_cpu) {
		return 16 + static_cast<std::size_t>(p_bank) * 0x4000 + p_cpu
			- (p_bank == 15 ? 0xc000 : 0x8000);
	}

	void write_rom(std::vector<byte>& p_rom, byte p_bank, std::uint16_t p_cpu,
		std::initializer_list<byte> p_bytes) {
		std::copy(p_bytes.begin(), p_bytes.end(),
			p_rom.begin() + static_cast<std::ptrdiff_t>(rom_offset(p_bank, p_cpu)));
	}

	void write_word(std::vector<byte>& p_data, std::size_t p_offset,
		std::uint16_t p_value) {
		if (p_offset + 2 > p_data.size()) throw std::runtime_error("truncated test word write");
		p_data[p_offset] = static_cast<byte>(p_value);
		p_data[p_offset + 1] = static_cast<byte>(p_value >> 8);
	}

	std::size_t first_movie_asset_offset(const std::vector<byte>& p_fmb,
		std::size_t p_asset) {
		constexpr std::size_t payload{ 8 };
		if (p_fmb.size() <= payload + 17 || p_fmb[5] == 0
			|| p_asset >= p_fmb[payload + 7])
			throw std::runtime_error("bad FMB asset fixture");
		const auto result{ payload + 18 + p_fmb[payload + 17] + p_asset * 9 };
		if (result + 9 > p_fmb.size()) throw std::runtime_error("truncated FMB asset fixture");
		return result;
	}

	struct ImportLocation {
		std::size_t record, payload, bytes;
	};

	ImportLocation find_import_location(const std::vector<byte>& p_fmb,
		fe::AtlasMovieImportKind p_kind) {
		if (p_fmb.size() < 6 || p_fmb[5] == 0)
			throw std::runtime_error("bad FMB import fixture");
		std::size_t cursor{ 6 };
		for (byte movie{}; movie < p_fmb[5]; ++movie) {
			const auto bytes{ read_word(p_fmb, cursor) };
			cursor += 2 + bytes;
			if (cursor > p_fmb.size()) throw std::runtime_error("truncated FMB movie fixture");
		}
		if (cursor + 7 > p_fmb.size()
			|| !std::equal(p_fmb.begin() + static_cast<std::ptrdiff_t>(cursor),
				p_fmb.begin() + static_cast<std::ptrdiff_t>(cursor + 4), "ATI1"))
			throw std::runtime_error("missing ATI1 test fixture");
		cursor += 5;
		const auto imports{ read_word(p_fmb, cursor) };
		cursor += 2;
		for (std::size_t i{}; i < imports; ++i) {
			const auto record{ cursor };
			if (cursor + 6 > p_fmb.size()) throw std::runtime_error("truncated ATI1 fixture");
			const auto kind{ static_cast<fe::AtlasMovieImportKind>(p_fmb[cursor + 1]) };
			const auto label_bytes{ p_fmb[cursor + 5] };
			cursor += 6 + label_bytes;
			const auto data_bytes{ read_word(p_fmb, cursor) };
			cursor += 2;
			if (cursor + data_bytes > p_fmb.size())
				throw std::runtime_error("truncated ATI1 payload fixture");
			if (kind == p_kind) return { record, cursor, data_bytes };
			cursor += data_bytes;
		}
		throw std::runtime_error("missing ATI1 import kind");
	}

	std::vector<byte> make_metasprite_library(byte p_count, byte p_width,
		byte p_height, bool p_visible = false) {
		std::vector<byte> result(static_cast<std::size_t>(p_count) * 2, 0);
		for (byte frame{}; frame < p_count; ++frame) {
			result.insert(result.end(), { 0, 0, p_width, p_height });
			for (std::size_t cell{};
				cell < static_cast<std::size_t>(p_width) * p_height; ++cell) {
				result.push_back(p_visible ? 0 : 0xff);
				if (p_visible) result.push_back(0);
			}
		}
		return result;
	}

	fe::AtlasMovie make_movie(const std::string& p_id,
		fe::AtlasMovieProjectRole p_role, fe::AtlasMovieExit p_exit, byte p_x) {
		fe::AtlasMovie movie;
		movie.id = p_id;
		movie.project_role = p_role;
		movie.exit_mode = p_exit;
		movie.metasprite_pointer_lo = 0xab17;
		movie.metasprite_pointer_hi = 0xab37;
		movie.metasprite_count = 2;
		movie.assets = {
			{ fe::AtlasMovieAssetKind::SpriteChr, 12, 0x9000,
				fe::AtlasMovieDestination::Ppu, 0x0000, 16 },
			{ fe::AtlasMovieAssetKind::BackgroundChr, 12, 0x9010,
				fe::AtlasMovieDestination::Ppu, 0x1000, 16 },
			{ fe::AtlasMovieAssetKind::Nametable, 12, 0x9020,
				fe::AtlasMovieDestination::Ppu, 0x2000, 0x0400 },
			{ fe::AtlasMovieAssetKind::Palette, 12, 0x9420,
				fe::AtlasMovieDestination::Ram, 0x0293, 32 },
		};

		fe::AtlasMovieTrack actor;
		actor.x = p_x;
		actor.y = 80;
		actor.velocity_x = 32;
		actor.coordinate = fe::AtlasMovieCoordinate::X;
		actor.keyframes = { { 160, 0, 0 } };
		actor.stage_frames = { { 0 }, { 1 } };
		actor.editor_name = "Actor";
		actor.editor_color = 0xff46beff;
		movie.tracks.push_back(std::move(actor));

		fe::AtlasMoviePhase phase;
		phase.condition = fe::AtlasMovieCondition::Frames;
		phase.condition_value = 60;
		movie.phases.push_back(phase);
		return movie;
	}

	fe::AtlasMovieBundle make_project() {
		fe::AtlasMovieBundle result;
		result.movies.push_back(make_movie("intro", fe::AtlasMovieProjectRole::OfficialIntro,
			fe::AtlasMovieExit::NewGame, 32));
		result.movies.push_back(make_movie("ending", fe::AtlasMovieProjectRole::OfficialEnding,
			fe::AtlasMovieExit::TitleReset, 48));
		auto optional{ make_movie("optional", fe::AtlasMovieProjectRole::Normal,
			fe::AtlasMovieExit::ReloadScreen, 64) };
		optional.enabled = false;
		result.movies.push_back(std::move(optional));
		return result;
	}

	fe::AtlasMovieBundle make_import_project() {
		auto result{ make_project() };
		auto& movie{ result.movies.front() };
		movie.imports = {
			{ fe::AtlasMovieImportKind::MetaspriteLibrary, "frames", 0, 2,
				make_metasprite_library(2, 1, 1) },
			{ fe::AtlasMovieImportKind::SpriteChr, "actor CHR", 0x0100, 0,
				std::vector<byte>(16, 0x11) },
			{ fe::AtlasMovieImportKind::BackgroundChr, "background CHR", 0x1000, 0,
				std::vector<byte>(16, 0x22) },
			{ fe::AtlasMovieImportKind::Nametable, "nametable", 0x2000, 0,
				std::vector<byte>(0x0400, 0x33) },
			{ fe::AtlasMovieImportKind::Palette, "palette", 0x0293, 0,
				std::vector<byte>(32, 0x0f) },
		};
		return result;
	}

	std::vector<byte> make_legacy_project(const fe::AtlasMovieBundle& p_bundle) {
		const auto amp2{ fe::AtlasMovieBundleCodec::compile_project(p_bundle) };
		if (amp2.size() < 7 || amp2[4] != 2) throw std::runtime_error("bad AMP2 test fixture");
		std::vector<byte> result(amp2.begin(), amp2.begin() + 7);
		result[4] = 1;
		std::size_t cursor{ 7 };
		for (std::size_t i{}; i < read_word(amp2, 5); ++i) {
			const auto record_start{ cursor };
			if (cursor + 6 > amp2.size()) throw std::runtime_error("truncated AMP2 test record");
			const auto fmb_bytes{ read_dword(amp2, cursor + 2) };
			cursor += 6 + fmb_bytes;
			if (cursor > amp2.size()) throw std::runtime_error("truncated AMP2 test movie");
			result.insert(result.end(), amp2.begin() + static_cast<std::ptrdiff_t>(record_start),
				amp2.begin() + static_cast<std::ptrdiff_t>(cursor));
			const auto editor_bytes{ read_dword(amp2, cursor) };
			cursor += 4 + editor_bytes;
			if (cursor > amp2.size()) throw std::runtime_error("truncated AMP2 test metadata");
		}
		if (cursor != amp2.size()) throw std::runtime_error("AMP2 test fixture has trailing bytes");
		return result;
	}

	std::vector<byte> make_ame(const std::vector<byte>& p_fmb) {
		std::vector<byte> result{ 'A','M','E','1',1 };
		append_word(result, static_cast<std::uint16_t>(
			fe::atlas_movie::layout::CORE_BYTES));
		append_word(result, static_cast<std::uint16_t>(
			fe::atlas_movie::layout::TAIL_BYTES));
		append_word(result, static_cast<std::uint16_t>(p_fmb.size()));
		result.insert(result.end(), GENERATED_CORE.begin(), GENERATED_CORE.end());
		result.insert(result.end(), GENERATED_TAIL.begin(), GENERATED_TAIL.end());
		result.insert(result.end(), p_fmb.begin(), p_fmb.end());
		return result;
	}

}

int main(int argc, char** argv) try {
	std::optional<std::string> usa_rom_path;
	if (argc == 3 && std::string(argv[1]) == "--usa-rom") {
		usa_rom_path = argv[2];
		argc = 1;
	}
	if (argc > 4) return 2;
	const auto legacy{ argc >= 2 ? read_bytes(argv[1]) : make_legacy_project(make_project()) };
	auto migrated{ fe::AtlasMovieBundleCodec::parse_project(legacy) };
	const auto runtime_before{ fe::AtlasMovieBundleCodec::compile(migrated) };
	if (argc >= 3) write_bytes(argv[2], fe::AtlasMovieBundleCodec::compile_project(migrated));
	if (argc >= 4) write_bytes(argv[3], runtime_before);

	require(migrated.movies.size() >= 2 && !migrated.movies.front().tracks.empty(),
		"legacy AMP project loads");
	require(!migrated.movies.front().tracks.front().editor_name.empty()
		&& migrated.movies.front().tracks.front().editor_color != 0,
		"legacy tracks receive deterministic editor identity defaults");
	if (argc == 1) {
		require(migrated.movies.size() == 3 && !migrated.movies.back().enabled,
			"disabled project movies survive AMP1 migration");
		require(fe::AtlasMovieBundleCodec::parse(runtime_before).movies.size() == 2,
			"disabled movies consume no runtime records");
	}

	auto& actor{ migrated.movies.front().tracks.front() };
	actor.editor_name = "Hero";
	actor.editor_group = "Royal party";
	actor.editor_color = 0xff3366ff;
	actor.editor_waypoints = { { 16, 32 }, { 80, 96 } };
	actor.editor_animation.automatic_facing = true;
	actor.editor_animation.right = { 0, 1, 0, 1 };
	actor.editor_animation.left = { 1, 0, 1, 0 };

	const auto amp2{ fe::AtlasMovieBundleCodec::compile_project(migrated) };
	require(amp2.size() > 5 && amp2[4] == 2, "project compiler emits AMP version 2");
	const auto decoded{ fe::AtlasMovieBundleCodec::parse_project(amp2) };
	const auto& restored{ decoded.movies.front().tracks.front() };
	require(restored.editor_name == "Hero" && restored.editor_group == "Royal party"
		&& restored.editor_color == 0xff3366ff, "actor identity metadata round-trips");
	require(restored.editor_waypoints.size() == 2
		&& restored.editor_animation.automatic_facing
		&& restored.editor_animation.right == std::vector<byte>({ 0, 1, 0, 1 }),
		"waypoint and semantic animation metadata round-trips");
	require(fe::AtlasMovieBundleCodec::compile_project(decoded) == amp2,
		"AMP2 decode/encode is byte-identical");
	require(fe::AtlasMovieBundleCodec::compile(decoded) == runtime_before,
		"editor-only metadata does not alter FMB runtime bytes");
	require(fe::AtlasMovieBundleCodec::compile(
		fe::AtlasMovieBundleCodec::parse(runtime_before)) == runtime_before,
		"FMB decode/encode is byte-identical");

	const auto imported_project{ make_import_project() };
	const auto imported_fmb{ fe::AtlasMovieBundleCodec::compile(imported_project) };
	const auto imported_decoded{ fe::AtlasMovieBundleCodec::parse(imported_fmb) };
	require(fe::AtlasMovieBundleCodec::compile(imported_decoded) == imported_fmb,
		"canonical ATI1 imports decode and encode byte-identically");
	auto bank_tail{ imported_fmb };
	bank_tail.insert(bank_tail.end(), { 0xde, 0xad, 0xbe, 0xef });
	require(fe::AtlasMovieBundleCodec::validated_prefix_size(bank_tail) == imported_fmb.size(),
		"validated FMB prefix reports its exact size inside a larger bank tail");
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(bank_tail); },
		"normal FMB parsing still rejects unrelated trailing bytes");

	auto metasprite_count_mismatch{ make_import_project() };
	metasprite_count_mismatch.movies.front().imports.front().aux = 1;
	metasprite_count_mismatch.movies.front().imports.front().data
		= make_metasprite_library(1, 1, 1);
	require_rejected([&] {
		(void)fe::AtlasMovieBundleCodec::compile(metasprite_count_mismatch);
	}, "metasprite import count must match the movie frame count");

	auto maximum_metasprite{ make_import_project() };
	maximum_metasprite.movies.front().imports.front().data
		= make_metasprite_library(2,
			fe::AtlasMovieBundleCodec::METASPRITE_MAX_WIDTH,
			fe::AtlasMovieBundleCodec::METASPRITE_MAX_HEIGHT);
	const auto maximum_metasprite_fmb{
		fe::AtlasMovieBundleCodec::compile(maximum_metasprite) };
	require(fe::AtlasMovieBundleCodec::compile(
		fe::AtlasMovieBundleCodec::parse(maximum_metasprite_fmb)) == maximum_metasprite_fmb,
		"maximum supported metasprite dimensions compile canonically");
	auto full_oam_metasprite{ make_import_project() };
	full_oam_metasprite.movies.front().imports.front().data
		= make_metasprite_library(2, 8, 8, true);
	require(!fe::AtlasMovieBundleCodec::compile(full_oam_metasprite).empty(),
		"a 64-cell metasprite fits the NES OAM limit");
	auto overflowing_oam_metasprite{ make_import_project() };
	overflowing_oam_metasprite.movies.front().imports.front().data
		= make_metasprite_library(2, 13, 5, true);
	require_rejected([&] {
		(void)fe::AtlasMovieBundleCodec::compile(overflowing_oam_metasprite);
	}, "a 65-cell metasprite is rejected before it can wrap OAM");
	auto wide_metasprite{ make_import_project() };
	wide_metasprite.movies.front().imports.front().data
		= make_metasprite_library(2,
			static_cast<byte>(fe::AtlasMovieBundleCodec::METASPRITE_MAX_WIDTH + 1), 1);
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(wide_metasprite); },
		"metasprites wider than the renderer limit are rejected");
	auto tall_metasprite{ make_import_project() };
	tall_metasprite.movies.front().imports.front().data
		= make_metasprite_library(2, 1,
			static_cast<byte>(fe::AtlasMovieBundleCodec::METASPRITE_MAX_HEIGHT + 1));
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(tall_metasprite); },
		"metasprites taller than the renderer limit are rejected");

	auto wrong_metasprite_table{ imported_fmb };
	write_word(wrong_metasprite_table, 20,
		static_cast<std::uint16_t>(read_word(wrong_metasprite_table, 20) + 1));
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(wrong_metasprite_table); },
		"ATI1 metasprite import must match the FMV1 pointer tables");
	auto wrong_metasprite_payload{ imported_fmb };
	wrong_metasprite_payload[find_import_location(wrong_metasprite_payload,
		fe::AtlasMovieImportKind::MetaspriteLibrary).payload] ^= 1;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(wrong_metasprite_payload); },
		"ATI1 metasprite payload must contain canonical relocated record pointers");
	auto wrong_background_pointer{ imported_fmb };
	const auto background_asset{ first_movie_asset_offset(wrong_background_pointer, 1) };
	write_word(wrong_background_pointer, background_asset + 2,
		static_cast<std::uint16_t>(read_word(wrong_background_pointer,
			background_asset + 2) + 1));
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(wrong_background_pointer); },
		"ATI1 background payload must match its exact FMV1 source pointer");
	auto wrong_nametable_destination{ imported_fmb };
	const auto nametable_asset{ first_movie_asset_offset(wrong_nametable_destination, 2) };
	write_word(wrong_nametable_destination, nametable_asset + 5,
		static_cast<std::uint16_t>(read_word(wrong_nametable_destination,
			nametable_asset + 5) + 16));
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(wrong_nametable_destination); },
		"ATI1 nametable payload must match its exact FMV1 destination");
	auto wrong_palette_size{ imported_fmb };
	write_word(wrong_palette_size, first_movie_asset_offset(wrong_palette_size, 3) + 7, 16);
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(wrong_palette_size); },
		"ATI1 palette payload must match its exact FMV1 byte count");
	auto wrong_sprite_space{ imported_fmb };
	wrong_sprite_space[first_movie_asset_offset(wrong_sprite_space, 4) + 4]
		= static_cast<byte>(fe::AtlasMovieDestination::Ram);
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(wrong_sprite_space); },
		"ATI1 sprite payload must match its exact FMV1 destination space");
	auto reordered_sprite_binding{ imported_fmb };
	const auto base_sprite{ first_movie_asset_offset(reordered_sprite_binding, 0) };
	const auto imported_sprite{ first_movie_asset_offset(reordered_sprite_binding, 4) };
	std::swap_ranges(reordered_sprite_binding.begin() + static_cast<std::ptrdiff_t>(base_sprite),
		reordered_sprite_binding.begin() + static_cast<std::ptrdiff_t>(base_sprite + 9),
		reordered_sprite_binding.begin() + static_cast<std::ptrdiff_t>(imported_sprite));
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(reordered_sprite_binding); },
		"noncanonical ATI1 descriptor ordering is rejected");
	require_rejected([&] {
		(void)fe::AtlasMovieBundleCodec::validated_prefix_size(wrong_background_pointer);
	}, "validated FMB prefix rejects a noncanonical ATI1 binding");

	auto misaligned_sprite_chr{ decoded };
	misaligned_sprite_chr.movies.front().assets.front().destination = 1;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(misaligned_sprite_chr); },
		"sprite CHR destination must be tile-aligned");
	auto misaligned_background_chr{ decoded };
	misaligned_background_chr.movies.front().assets[1].destination = 0x1001;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(misaligned_background_chr); },
		"background CHR destination must be tile-aligned");
	auto sprite_chr_alias{ decoded };
	sprite_chr_alias.movies.front().assets.front().destination = 0x1000;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(sprite_chr_alias); },
		"sprite CHR must stay in pattern table $0000-$0FFF");
	auto background_chr_alias{ decoded };
	background_chr_alias.movies.front().assets[1].destination = 0x0ff0;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(background_chr_alias); },
		"background CHR must stay in pattern table $1000-$1FFF");
	auto short_nametable{ decoded };
	short_nametable.movies.front().assets[2].bytes = 16;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(short_nametable); },
		"nametable must contain the complete 1024-byte screen");
	auto mirrored_nametable{ decoded };
	mirrored_nametable.movies.front().assets[2].destination = 0x3000;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(mirrored_nametable); },
		"nametable PPU aliases are rejected");
	auto relocated_background{ decoded.movies.front() };
	relocated_background.assets[1].destination = 0x1400;
	require(fe::atlas_movie::movie_background_tile(relocated_background, 0x40) == 0
		&& fe::atlas_movie::movie_background_tile(relocated_background, 0x3f)
			== std::nullopt,
		"preview maps nametable tiles from the configured background CHR destination");

	auto valid_music{ decoded };
	valid_music.movies.front().entry_music = 16;
	require(!fe::AtlasMovieBundleCodec::compile(valid_music).empty(),
		"highest stock entry music ID is accepted");
	valid_music.movies.front().entry_music = 0xfe;
	require(!fe::AtlasMovieBundleCodec::compile(valid_music).empty(),
		"entry music stop sentinel is accepted");
	auto invalid_music{ decoded };
	invalid_music.movies.front().entry_music = 17;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(invalid_music); },
		"entry music beyond the stock song table is rejected");
	invalid_music.movies.front().entry_music = 0x80;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(invalid_music); },
		"negative non-sentinel entry music is rejected");

	std::vector<byte> bank_15_rom(16 + 16 * 0x4000, 0);
	bank_15_rom[16 + 15 * 0x4000] = 0x5a;
	auto bank_15_asset{ decoded.movies.front().assets.front() };
	bank_15_asset.bank = 15; bank_15_asset.cpu = 0x8000; bank_15_asset.bytes = 1;
	require(fe::atlas_movie::asset_bytes(bank_15_rom, bank_15_asset)
		== std::vector<byte>({ 0x5a }),
		"bank 15 assets use the switchable $8000 address window");
	const auto budget{ fe::AtlasMovieBundleCodec::detailed_budget(decoded) };
	require(!budget.movies.empty() && !budget.tracks.empty(),
		"detailed runtime budget reports movies and actors");
	require(!budget.assets.empty() && budget.assets.front().bytes == 9,
		"runtime budget uses the exact nine-byte asset descriptor size");
	auto arbitrary_dwell{ decoded };
	arbitrary_dwell.movies.front().tracks.front().dwell_frames = 3;
	require(!fe::AtlasMovieBundleCodec::compile(arbitrary_dwell).empty(),
		"path dwell supports every nonzero runtime value");
	auto edited_movie{ decoded.movies.front() };
	auto& edited_track{ edited_movie.tracks.front() };
	require(fe::atlas_movie::apply_painted_path(edited_movie, edited_track,
		{ { 32, 80 }, { 96, 80 }, { 160, 112 }, { 208, 112 } }, 48, 0)
		== false && edited_track.editor_waypoints.size() == 4,
		"painted actor path compiles into runtime keyframes");
	const auto path_preview{ fe::atlas_movie::preview(edited_movie, 30) };
	require(!path_preview.tracks.empty()
		&& (path_preview.tracks.front().x != edited_track.x
			|| path_preview.tracks.front().y != edited_track.y),
		"extracted preview simulator advances compiled actor movement");
	require(fe::atlas_movie::simulated_path(edited_track).size() > 2,
		"editor route visualization uses the extracted runtime simulator");
	auto translated_actor{ decoded.movies.front().tracks.front() };
	translated_actor.editor_waypoints = { { 32, 80 }, { 120, 96 } };
	const auto translated_threshold{ translated_actor.keyframes.front().threshold };
	fe::atlas_movie::translate_actor(translated_actor, 7, 5);
	require(translated_actor.x == 39 && translated_actor.y == 85
		&& translated_actor.editor_waypoints.front().x == 39
		&& translated_actor.editor_waypoints.front().y == 85
		&& translated_actor.keyframes.front().threshold
			== static_cast<byte>(translated_threshold + 7),
		"actor translation moves its origin, path, and gate thresholds together");
	translated_actor.keyframes.front().threshold = 253;
	const auto before_clamped_x{ translated_actor.x };
	fe::atlas_movie::translate_actor(translated_actor, 8, 0);
	require(translated_actor.x == static_cast<byte>(before_clamped_x + 2)
		&& translated_actor.keyframes.front().threshold == 255,
		"actor translation clamps against absolute gate thresholds");
	auto draw_only_movie{ decoded.movies.front() };
	auto draw_only_path{ draw_only_movie.tracks.front() };
	draw_only_path.stage_frames = { { 1, 0 }, { 0, 1 } };
	draw_only_path.dwell_frames = 3;
	fe::AtlasMovieTrack draw_only_cyclic;
	draw_only_cyclic.kind = fe::AtlasMovieTrackKind::Cyclic;
	draw_only_cyclic.x = 48;
	draw_only_cyclic.y = 96;
	draw_only_cyclic.integrator_shift = 1;
	draw_only_cyclic.dwell_frames = 4;
	draw_only_cyclic.visible_frames = { 1 };
	draw_only_cyclic.reset_at_pose = 2;
	draw_only_movie.tracks = { draw_only_path, draw_only_cyclic };
	fe::AtlasMoviePhase draw_only_phase;
	draw_only_phase.update_mask = 0;
	draw_only_phase.draw_mask = 3;
	draw_only_phase.condition = fe::AtlasMovieCondition::Frames;
	draw_only_phase.condition_value = 1;
	draw_only_movie.phases = { draw_only_phase };
	auto draw_only_bundle{ decoded };
	draw_only_bundle.movies.front() = draw_only_movie;
	(void)fe::AtlasMovieBundleCodec::compile(draw_only_bundle);
	const auto draw_only_preview{ fe::atlas_movie::preview(draw_only_movie, 1) };
	require(draw_only_preview.tracks[0].visible
		&& draw_only_preview.tracks[0].frame == 1
		&& draw_only_preview.tracks[1].visible
		&& draw_only_preview.tracks[1].frame == 1,
		"draw-only first phases keep Path and Cyclic initial frames visible");

	fe::AtlasMovieTrack animation_contract;
	animation_contract.kind = fe::AtlasMovieTrackKind::Path;
	animation_contract.integrator_shift = 1;
	for (unsigned slots{ 1 }; slots <= 255; ++slots) {
		animation_contract.stage_frames = { std::vector<byte>(slots) };
		for (unsigned i{}; i < slots; ++i)
			animation_contract.stage_frames.front()[i] = static_cast<byte>(i);
		for (unsigned dwell{ 1 }; dwell <= 255; ++dwell) {
			animation_contract.dwell_frames = static_cast<byte>(dwell);
			fe::atlas_movie::PreviewTrack state;
			for (unsigned update{ 1 }; update <= 256; ++update) {
				fe::atlas_movie::advance_track(animation_contract, state, 0);
				const auto tick{ static_cast<byte>(update) };
				const auto slot{ static_cast<byte>((tick / dwell) % slots) };
				if (state.tick != tick || state.frame != slot)
					throw std::runtime_error("path animation division contract changed");
			}
		}
	}
	require(true,
		"path animation matches tick / dwell modulo slots for every 8-bit input");

	auto counter_movie{ decoded.movies.front() };
	auto counter_track{ fe::atlas_movie::default_track(
		fe::AtlasMovieTrackKind::CounterToggle, 0) };
	counter_track.counter_address = 0x04d8;
	counter_track.counter_mask = 1;
	counter_track.toggle_frames = { 0, 1 };
	counter_movie.tracks = { counter_track };
	for (auto& phase : counter_movie.phases) {
		phase.update_mask = phase.draw_mask = 1;
		if (phase.effect != fe::AtlasMovieEffect::None) {
			phase.effect = fe::AtlasMovieEffect::None;
			phase.effect_track = 0xff;
		}
		if (phase.condition == fe::AtlasMovieCondition::TrackYGte)
			phase.condition = fe::AtlasMovieCondition::Frames;
	}
	const auto unresolved_counter{ fe::atlas_movie::preview(counter_movie, 1) };
	require(!unresolved_counter.tracks.front().counter_resolved
		&& unresolved_counter.tracks.front().frame == 0,
		"preview marks arbitrary RAM counter toggles unresolved and shows Frame A");
	const auto resolved_counter{ fe::atlas_movie::preview(counter_movie, 1, 240,
		[](std::uint16_t address, std::size_t) -> std::optional<byte> {
			return address == 0x04d8 ? std::optional<byte>{ 1 } : std::nullopt;
		}) };
	require(resolved_counter.tracks.front().counter_resolved
		&& resolved_counter.tracks.front().frame == 1,
		"preview counter reader reproduces arbitrary RAM toggle state");
	auto counter_project{ decoded };
	counter_track.counter_address = 0x07ff;
	counter_project.movies.front().tracks = { counter_track };
	for (auto& phase : counter_project.movies.front().phases) {
		phase.update_mask = phase.draw_mask = 1;
		if (phase.effect != fe::AtlasMovieEffect::None) {
			phase.effect = fe::AtlasMovieEffect::None;
			phase.effect_track = 0xff;
		}
		if (phase.condition == fe::AtlasMovieCondition::TrackYGte)
			phase.condition = fe::AtlasMovieCondition::Frames;
	}
	require(!fe::AtlasMovieBundleCodec::compile(counter_project).empty(),
		"CounterToggle accepts the final internal RAM address");
	counter_project.movies.front().tracks.front().counter_address = 0x0800;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(counter_project); },
		"CounterToggle rejects addresses outside internal RAM");
	counter_movie.tracks.front().x = 45;
	counter_movie.tracks.front().y = 123;
	counter_movie.phases.front().update_mask = 0;
	counter_movie.phases.front().condition = fe::AtlasMovieCondition::TrackYGte;
	counter_movie.phases.front().condition_track = 0;
	counter_movie.phases.front().condition_value = 123;
	const auto counter_position{ fe::atlas_movie::preview(counter_movie, 1) };
	require(counter_position.tracks.front().x == 45
		&& counter_position.tracks.front().y == 123
		&& counter_position.finished,
		"CounterToggle initializes deterministic coordinates for TrackYGte");
	auto paste_bundle{ decoded };
	const auto paste_before{ fe::AtlasMovieBundleCodec::compile_project(paste_bundle) };
	auto invalid_actor{ counter_track };
	invalid_actor.toggle_frames = { 0, 0xff };
	require_rejected([&] {
		(void)fe::atlas_movie::paste_actors(paste_bundle, 0, { invalid_actor }, 0);
	}, "cross-project actor paste rejects missing destination frames");
	require(fe::AtlasMovieBundleCodec::compile_project(paste_bundle) == paste_before,
		"rejected actor paste is transactional");
	const auto pasted_indices{ fe::atlas_movie::paste_actors(
		paste_bundle, 0, { paste_bundle.movies.front().tracks.front() }, 0) };
	require(pasted_indices.size() == 1
		&& paste_bundle.movies.front().tracks.size()
			== decoded.movies.front().tracks.size() + 1
		&& paste_bundle.movies.front().tracks.back().keyframes.front().threshold
			== static_cast<byte>(decoded.movies.front().tracks.front()
				.keyframes.front().threshold + 8),
		"valid actor paste offsets the actor and its runtime path together");

	const auto file_test_directory{ std::filesystem::temp_directory_path()
		/ ("faxedit-atlas-movie-save-"
			+ std::to_string(reinterpret_cast<std::uintptr_t>(&migrated))) };
	std::filesystem::remove_all(file_test_directory);
	std::filesystem::create_directories(file_test_directory);
	const auto project_path{ file_test_directory / "project.amp" };
	const std::vector<byte> old_project{ 1, 2, 3 };
	const std::vector<byte> new_project{ 4, 5, 6, 7 };
	klib::file::write_bytes_to_file(old_project, project_path.string());
	klib::file::write_bytes_to_file_atomic(new_project, project_path.string());
	require(read_bytes(project_path.c_str()) == new_project
		&& !std::filesystem::exists(project_path.string() + ".tmp"),
		"atomic project save replaces the target and removes its temporary file");
	std::filesystem::create_directory(project_path.string() + ".tmp");
	require_rejected([&] {
		klib::file::write_bytes_to_file_atomic(old_project, project_path.string());
	}, "failed atomic project save reports an error");
	require(read_bytes(project_path.c_str()) == new_project,
		"failed atomic project save preserves the previous project");
#ifndef _WIN32
	if (std::filesystem::exists("/dev/full"))
		require_rejected([&] {
			klib::file::write_bytes_to_file(new_project, "/dev/full");
		}, "file writer reports a flush failure");
#endif
	std::filesystem::remove_all(file_test_directory);

	const auto ame{ make_ame(runtime_before) };
	fe::AtlasMovieEngine::validate_package(ame);
	require(true, "generated AME executable and FMB payload are accepted");
	require(fe::AtlasMovieBundleCodec::extract_from_ame(ame) == runtime_before,
		"AME extraction preserves the FMB payload");
	require(fe::AtlasMovieBundleCodec::replace_in_ame(ame, decoded) == ame,
		"AME replacement is byte-identical for unchanged runtime data");

	const auto standalone_at_8000{ fe::AtlasMovieRuntime::build_standalone(runtime_before, 0x8000) };
	const auto standalone_at_a000{ fe::AtlasMovieRuntime::build_standalone(runtime_before, 0xa000) };
	require(standalone_at_8000.size()
		== fe::AtlasMovieRuntime::STANDALONE_PLAYER_BYTES + runtime_before.size(),
		"standalone runtime has the player plus exact FMB payload");
	require(fnv1a(standalone_at_8000, fe::AtlasMovieRuntime::STANDALONE_PLAYER_BYTES)
		== 0xbed21ff70b38d1fdULL,
		"standalone player bytes match the generated source artifact");
	require(std::equal(runtime_before.begin(), runtime_before.end(),
		standalone_at_8000.end() - static_cast<std::ptrdiff_t>(runtime_before.size())),
		"standalone runtime preserves the compiled FMB suffix");
	require(read_word(standalone_at_a000, 5)
		== static_cast<std::uint16_t>(read_word(standalone_at_8000, 5) + 0x2000)
		&& static_cast<std::uint16_t>(standalone_at_a000[13] | standalone_at_a000[17] << 8)
		== static_cast<std::uint16_t>(
			(standalone_at_8000[13] | standalone_at_8000[17] << 8) + 0x2000),
		"standalone absolute and split-immediate pointers relocate with the player");

	const auto vanilla_opcodes{ fi::load_vanilla_opcodes() };
	fe::AtlasMovieRuntime::validate_shared_install(vanilla_opcodes);
	require(true, "vanilla opcode map is eligible for Shared installation");
	const auto standalone_xml{ fe::AtlasMovieRuntime::standalone_config_override(
		vanilla_opcodes, runtime_before) };
	pugi::xml_document standalone_document;
	require(standalone_xml.find("Impl=AtlasDevPlayMovie") != std::string::npos
		&& standalone_xml.find("hack_movie_data") != std::string::npos
		&& standalone_xml.find("AtlasDevPlayMovieShared") == std::string::npos
		&& standalone_document.load_string(standalone_xml.c_str()),
		"standalone config exports one generated opcode and its FMB data");
	auto domain_opcodes{ vanilla_opcodes };
	byte domain_opcode{ static_cast<byte>(domain_opcodes.opcodes.size()) };
	for (const auto domain : magic_enum::enum_values<fi::ArgDomain>()) {
		if (domain == fi::ArgDomain::None) continue;
		const auto name{ klib::str::enum_to_string(domain) };
		domain_opcodes.opcodes.emplace(domain_opcode++, fi::Opcode(
			"Domain" + name, { { fi::ArgType::Byte, domain } },
			fi::Flow::Continue, false));
	}
	domain_opcodes.base_opcode_count = domain_opcodes.opcodes.size();
	const auto domain_xml{ fe::AtlasMovieRuntime::standalone_config_override(
		domain_opcodes, runtime_before) };
	for (const auto domain : magic_enum::enum_values<fi::ArgDomain>()) {
		if (domain == fi::ArgDomain::None) continue;
		require(domain_xml.find("Args=Byte:" + klib::str::enum_to_string(domain))
			!= std::string::npos,
			"standalone config preserves every explicit argument domain");
	}

	std::vector<byte> installed_rom(16 + 16 * 0x4000, 0xff);
	write_rom(installed_rom, 15, 0xfc98, { 0x20, 0x59, 0xf8, 0x0c, 0x07, 0xa7 });
	write_rom(installed_rom, 12, 0x82ae, { 0x20, 0x0c, 0xa7, 0x4c, 0x13, 0xc9 });
	write_rom(installed_rom, 12, 0xad8b, { 0x20, 0xa4, 0x87, 0x4c, 0x13, 0xa7 });
	std::copy(GENERATED_CORE.begin(), GENERATED_CORE.end(),
		installed_rom.begin() + static_cast<std::ptrdiff_t>(rom_offset(12, 0xa708)));
	std::copy(GENERATED_TAIL.begin(), GENERATED_TAIL.end(),
		installed_rom.begin() + static_cast<std::ptrdiff_t>(rom_offset(12, 0xad91)));
	std::copy(runtime_before.begin(), runtime_before.end(),
		installed_rom.begin() + static_cast<std::ptrdiff_t>(rom_offset(12, 0xb264)));
	const auto dispatch_low{ static_cast<std::uint16_t>(
		fe::atlas_movie::layout::BUNDLE_CPU + runtime_before.size()) };
	const auto dispatch_high{ static_cast<std::uint16_t>(dispatch_low
		+ fe::atlas_movie::layout::DISPATCH_ENTRIES) };
	for (std::size_t i{}; i < fe::atlas_movie::layout::HANDLERS.size(); ++i) {
		installed_rom[rom_offset(12, dispatch_low) + i]
			= static_cast<byte>(fe::atlas_movie::layout::HANDLERS[i]);
		installed_rom[rom_offset(12, dispatch_high) + i]
			= static_cast<byte>(fe::atlas_movie::layout::HANDLERS[i] >> 8);
	}
	write_rom(installed_rom, 12, fe::atlas_movie::layout::DISPATCH_LOW_REF,
		{ static_cast<byte>(dispatch_low), static_cast<byte>(dispatch_low >> 8) });
	write_rom(installed_rom, 12, fe::atlas_movie::layout::DISPATCH_HIGH_REF,
		{ static_cast<byte>(dispatch_high), static_cast<byte>(dispatch_high >> 8) });
	require(fe::AtlasMovieEngine::is_installed(installed_rom),
		"complete generated Shared engine is detected");
	auto corrupted_install{ installed_rom };
	corrupted_install[rom_offset(12, 0xa708) + 100] ^= 0x01;
	require(!fe::AtlasMovieEngine::is_installed(corrupted_install),
		"installed Shared engine detection rejects changed executable code");
	const auto shared_opcodes{ fe::AtlasMovieRuntime::resolve_opcode_info(
		vanilla_opcodes, installed_rom) };
	require(shared_opcodes.opcodes.size() == 25
		&& shared_opcodes.opcodes.at(0x18).name == "AtlasDevPlayMovieShared"
		&& shared_opcodes.required_impls.empty()
		&& shared_opcodes.base_opcode_count == 25,
		"installed AME contributes one preinstalled Shared opcode without an Impl");
	require(fe::AtlasMovieEngine::script_data_start(installed_rom)
		== rom_offset(12, static_cast<std::uint16_t>(0xb264 + runtime_before.size() + 50)),
		"installed AME reserves FMB and Shared dispatch space before scripts");
	const auto installed_before_replace{ installed_rom };
	(void)fe::AtlasMovieBundleCodec::replace_in_installed_rom(installed_rom, decoded);
	require(installed_rom == installed_before_replace,
		"unchanged installed FMB and dispatch tables replace byte-identically");
	std::vector<byte> unpatched_rom(installed_rom.size(), 0xff);
	require_rejected([&] {
		(void)fe::AtlasMovieRuntime::resolve_opcode_info(shared_opcodes, unpatched_rom);
	}, "Shared opcode map is rejected when AME is not installed");

	auto mixed_modes{ vanilla_opcodes };
	mixed_modes.opcodes.emplace(0x18, fi::Opcode("AtlasDevPlayMovie",
		std::initializer_list<fi::Argument>{ { fi::ArgType::Byte, fi::ArgDomain::None } },
		fi::Flow::End, true));
	mixed_modes.required_impls.push_back("AtlasDevPlayMovie");
	require_rejected([&] {
		fe::AtlasMovieRuntime::validate_shared_install(mixed_modes);
	}, "Shared installation rejects a configured Standalone opcode before ROM writes");
	require_rejected([&] {
		(void)fe::AtlasMovieRuntime::resolve_opcode_info(mixed_modes, installed_rom);
	}, "Standalone and Shared movie modes cannot coexist");

	auto truncated{ runtime_before };
	truncated.pop_back();
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::parse(truncated); },
		"truncated FMB is rejected");
	auto overflow{ decoded };
	overflow.movies.front().imports.push_back({ fe::AtlasMovieImportKind::BackgroundChr,
		"oversized background", 0x1000, 0, std::vector<byte>(4096, 0) });
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(overflow); },
		"bank-12 overflow is rejected");
	auto standalone_import{ decoded };
	standalone_import.movies.front().imports.push_back({ fe::AtlasMovieImportKind::SpriteChr,
		"standalone import", 0x0800, 0, std::vector<byte>(16, 0) });
	const auto standalone_import_fmb{ fe::AtlasMovieBundleCodec::compile(standalone_import) };
	require_rejected([&] {
		(void)fe::AtlasMovieRuntime::build_standalone(standalone_import_fmb, 0x9000);
	}, "standalone mode rejects Shared-layout imported graphics");
	auto oversized_track{ decoded };
	auto& oversized_path{ oversized_track.movies.front().tracks.front() };
	oversized_path.stage_frames = {
		std::vector<byte>(130, 0), std::vector<byte>(130, 0)
	};
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(oversized_track); },
		"track records beyond the player's 8-bit index are rejected");
	auto invalid_palette{ decoded };
	auto& palette{ invalid_palette.movies.front().assets.back() };
	palette.destination_space = fe::AtlasMovieDestination::Ppu;
	palette.destination = 0x3f00;
	require_rejected([&] { (void)fe::AtlasMovieBundleCodec::compile(invalid_palette); },
		"palette uploads outside the runtime RAM contract are rejected");

	std::vector<byte> blank_rom(16 + 16 * 0x4000);
	const auto blank_before{ blank_rom };
	auto tampered_core{ ame };
	tampered_core[11 + 100] ^= 0x01;
	require_rejected([&] {
		fe::AtlasMovieEngine::validate_package(tampered_core);
	}, "AME validation rejects one changed core byte");
	auto tampered_tail{ ame };
	tampered_tail[11 + fe::atlas_movie::layout::CORE_BYTES + 100] ^= 0x01;
	require_rejected([&] {
		fe::AtlasMovieEngine::validate_package(tampered_tail);
	}, "AME validation rejects one changed tail byte");
	auto malformed_ame{ ame };
	malformed_ame[11 + fe::atlas_movie::layout::CORE_BYTES
		+ fe::atlas_movie::layout::TAIL_BYTES] = 'X';
	require_rejected([&] { (void)fe::AtlasMovieEngine::install(blank_rom, malformed_ame); },
		"malformed AME movie data is rejected before installation");
	require(blank_rom == blank_before, "failed AME installation leaves the ROM unchanged");
	if (usa_rom_path) {
		auto clean_usa{ read_bytes(usa_rom_path->c_str()) };
		const auto clean_usa_before{ clean_usa };
		const auto installed{ fe::AtlasMovieEngine::install(clean_usa, ame) };
		require(installed.bundle_bytes == runtime_before.size()
			&& clean_usa != clean_usa_before,
			"AME installation mutates a clean USA Rev 0 ROM and reports its FMB size");
		require(fe::AtlasMovieEngine::is_installed(clean_usa),
			"successful USA Rev 0 installation passes complete engine detection");
		require(fe::AtlasMovieBundleCodec::extract_from_installed_rom(clean_usa)
			== runtime_before,
			"successful USA Rev 0 installation preserves the exact FMB payload");
		require(installed.reserved_file_end
			== fe::AtlasMovieEngine::script_data_start(clean_usa),
			"successful USA Rev 0 installation reserves through relocated dispatch tables");
	}

	std::cout << "All Atlas Movie tests passed\n";
	return 0;
}
catch (const std::exception& ex) {
	std::cerr << "FAIL " << ex.what() << '\n';
	return 1;
}
