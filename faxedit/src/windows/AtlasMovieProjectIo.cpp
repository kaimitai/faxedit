#include "MainWindow.h"
#include "AtlasMovieUi.h"
#include "common/klib/Kfile.h"
#include "fe/AtlasMovieBundle.h"
#include "fe/AtlasMovieCompatibility.h"
#include "fe/AtlasMovieEditor.h"
#include "fe/AtlasMovieEngine.h"
#include "fe/AtlasMovieRuntime.h"
#include "fe/script/ScriptManager.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <utility>

namespace {

	using namespace fe;
	using namespace fe::atlas_movie;
	using namespace fe::atlas_movie::ui_detail;

	std::filesystem::path amp_path(std::filesystem::path p_path) {
		if (p_path.extension() != ".amp") p_path += ".amp";
		return p_path;
	}

}

void fe::MainWindow::adopt_atlas_movie_project(AtlasMovieBundle p_bundle,
	const std::filesystem::path& p_path, AtlasMovieRuntimeMode p_mode,
	bool p_dirty) {
	m_atlas_movie_bundle = std::move(p_bundle);
	m_atlas_movie_project_path = p_path;
	m_atlas_movie_actor_session = {};
	m_atlas_movie_runtime_mode = p_mode;
	m_atlas_movie_sel_movie = m_atlas_movie_sel_track = 0;
	m_atlas_movie_sel_phase = m_atlas_movie_sel_sfx = 0;
	m_atlas_movie_preview_frame = m_atlas_movie_browser_frame = 0;
	m_atlas_movie_preview_tick = 0;
	m_atlas_movie_preview_playing = false;
	m_atlas_movie_path_draw_mode = m_atlas_movie_path_painting = false;
	m_atlas_movie_actor_place_mode = false;
	m_atlas_movie_painted_path.clear();
	m_atlas_movie_dirty = p_dirty;
	m_atlas_movie_autoload_attempted = true;
}

void fe::MainWindow::new_atlas_movie_project(void) {
	if (m_config.get_region() != "us")
		throw std::runtime_error(
			"New Atlas movie projects currently require Faxanadu USA Rev 0");
	auto bundle{ make_starter_project() };
	adopt_atlas_movie_project(std::move(bundle), {},
		AtlasMovieRuntimeMode::Standalone, true);
	add_message("Created a new Atlas movie project from ROM-owned Faxanadu assets", 2);
}

void fe::MainWindow::load_atlas_movie_bundle_from_ame(
	const std::filesystem::path& p_path) {
	const auto source{ klib::file::read_file_as_bytes(p_path.string()) };
	const auto fmb{ AtlasMovieBundleCodec::extract_from_ame(source) };
	auto bundle{ AtlasMovieBundleCodec::parse(fmb) };
	const auto count{ bundle.movies.size() };
	adopt_atlas_movie_project(std::move(bundle), {},
		AtlasMovieRuntimeMode::Shared, false);
	add_message(std::format("Loaded {} Atlas movies from {}", count,
		p_path.string()), 2);
}

void fe::MainWindow::load_atlas_movie_bundle_from_rom(void) {
	if (!m_game.has_value()) throw std::runtime_error("Load a ROM before reading an installed Atlas movie bundle");
	const auto fmb{ AtlasMovieBundleCodec::extract_from_installed_rom(m_game->m_rom_data) };
	auto bundle{ AtlasMovieBundleCodec::parse(fmb) };
	const auto count{ bundle.movies.size() };
	adopt_atlas_movie_project(std::move(bundle), {},
		AtlasMovieRuntimeMode::Shared, false);
	add_message(std::format("Loaded {} Atlas movies from the in-memory ROM", count), 2);
}

void fe::MainWindow::load_atlas_movie_project(
	const std::filesystem::path& p_path) {
	auto bundle{ AtlasMovieBundleCodec::parse_project(
		klib::file::read_file_as_bytes(p_path.string())) };
	const auto count{ bundle.movies.size() };
	adopt_atlas_movie_project(std::move(bundle), p_path,
		m_atlas_movie_runtime_mode, false);
	add_message(std::format("Loaded Atlas project with {} editable movies from {}",
		count, p_path.string()), 2);
}

void fe::MainWindow::save_atlas_movie_project(void) {
	if (!m_atlas_movie_bundle) throw std::runtime_error("No Atlas movie project is loaded");
	if (m_atlas_movie_project_path.empty())
		throw std::runtime_error("Choose Save As for this untitled Atlas movie project");
	save_atlas_movie_project_as(m_atlas_movie_project_path);
}

void fe::MainWindow::save_atlas_movie_project_as(
	const std::filesystem::path& p_path) {
	if (!m_atlas_movie_bundle) throw std::runtime_error("No Atlas movie project is loaded");
	const auto project{ AtlasMovieBundleCodec::compile_project(*m_atlas_movie_bundle) };
	const auto path{ amp_path(p_path) };
	klib::file::write_bytes_to_file_atomic(project, path.string());
	m_atlas_movie_project_path = path;
	m_atlas_movie_dirty = false;
	add_message(std::format("Wrote editable Atlas project to {} ({} bytes)",
		path.string(), project.size()), 2);
}

void fe::MainWindow::save_atlas_movie_bundle(void) {
	if (!m_atlas_movie_bundle) throw std::runtime_error("No Atlas movie bundle is loaded");
	const auto report{ AtlasMovieBundleCodec::validate(*m_atlas_movie_bundle) };
	const auto fmb{ AtlasMovieBundleCodec::compile(*m_atlas_movie_bundle) };
	const auto path{ (m_path / "movie-bundle-created.fmb").string() };
	klib::file::write_bytes_to_file(fmb, path);
	add_message(std::format("Wrote {} bytes to {}; reserve bank 12 through ${:04X}",
		report.bytes, path, report.reserved_cpu_end), 2);
}

void fe::MainWindow::save_atlas_movie_standalone_config(void) {
	if (!m_atlas_movie_bundle) throw std::runtime_error("No Atlas movie project is loaded");
	if (m_atlas_movie_runtime_mode != AtlasMovieRuntimeMode::Standalone)
		throw std::runtime_error("Select Standalone mode before exporting its opcode configuration");
	if (m_game.has_value() && AtlasMovieEngine::is_installed(m_game->m_rom_data))
		throw std::runtime_error("Standalone cannot coexist with Shared mode in the loaded ROM");
	if (std::ranges::any_of(m_atlas_movie_bundle->movies,
		[](const auto& movie) { return !movie.imports.empty(); }))
		throw std::runtime_error(
			"Standalone is not ready: remove imported assets or select Shared mode");
	const auto fmb{ AtlasMovieBundleCodec::compile(*m_atlas_movie_bundle) };
	const auto xml{ AtlasMovieRuntime::standalone_config_override(
		m_cache.iscript_opcode_info, fmb) };
	const auto path{ (m_path / "eoe_config_override-atlas-movie-standalone.xml").string() };
	klib::file::write_string_to_file(xml, path);
	add_message(std::format(
		"Wrote Standalone opcode config to {}; use as eoe_config_override.xml before assembling AtlasDevPlayMovie scripts",
		path), 2);
	if (m_game.has_value()
		&& has_atlas_resident_scheduler(m_game->m_rom_data))
		add_message(
			"Atlas Resident Scheduler remains active during movies; palette or sprite effects may be visible",
			0, true);
}

void fe::MainWindow::save_atlas_movie_ame(void) {
	if (!m_atlas_movie_bundle) throw std::runtime_error("No Atlas movie bundle is loaded");
	if (m_atlas_movie_runtime_mode != AtlasMovieRuntimeMode::Shared)
		throw std::runtime_error("AME export is available only in Shared mode");
	const auto package{ AtlasMovieEngine::build_package(*m_atlas_movie_bundle) };
	const auto path{ (m_path / "atlas-movie-engine-created.ame").string() };
	klib::file::write_bytes_to_file(package, path);
	add_message(std::format("Wrote custom Atlas Movie Engine package to {} ({} bytes)", path, package.size()), 2);
}

void fe::MainWindow::apply_atlas_movie_bundle(void) {
	if (!m_atlas_movie_bundle) throw std::runtime_error("No Atlas movie bundle is loaded");
	if (!m_game.has_value()) throw std::runtime_error("Load a ROM before installing Atlas Movie Engine");
	if (m_atlas_movie_runtime_mode != AtlasMovieRuntimeMode::Shared)
		throw std::runtime_error("Direct ROM installation is available only in Shared mode");
	const bool atlas_scheduler_detected{
		has_atlas_resident_scheduler(m_game->m_rom_data) };
	AtlasMovieBundleCodec::validate(*m_atlas_movie_bundle);
	const auto config_files{ get_config_file_paths() };
	const fe::Config current_config(config_files.first, config_files.second,
		m_game->m_rom_data, m_config.get_region());
	const auto configured_opcodes{
		fe::script::get_iscript_opcode_info(current_config) };
	if (AtlasMovieEngine::is_installed(m_game->m_rom_data)) {
		const auto report{ AtlasMovieBundleCodec::replace_in_installed_rom(m_game->m_rom_data, *m_atlas_movie_bundle) };
		add_message(std::format("Replaced in-memory Atlas bundle: {} bytes; reserve through ${:04X}",
			report.bytes, report.reserved_cpu_end), 2);
	}
	else {
		AtlasMovieRuntime::validate_shared_install(configured_opcodes);
		const auto package{ AtlasMovieEngine::build_package(*m_atlas_movie_bundle) };
		const auto result{ AtlasMovieEngine::install(m_game->m_rom_data, package) };
		add_message(std::format("Installed custom Atlas engine in memory: {} movie bytes", result.bundle_bytes), 2);
	}
	m_settings.m_patch_cinematics = false;
	m_cache.iscript_opcode_info = AtlasMovieRuntime::resolve_opcode_info(
		configured_opcodes, m_game->m_rom_data);
	if (atlas_scheduler_detected)
		add_message(
			"Atlas Resident Scheduler remains active during movies; palette or sprite effects may be visible",
			0, true);
}

void fe::MainWindow::place_atlas_movie_pose(AtlasMovieBundle& p_bundle,
	AtlasMovie& p_movie, byte p_frame, byte p_x, byte p_y,
	std::size_t p_phase_index) {
	if (p_movie.tracks.size() >= 8)
		throw std::runtime_error("Atlas movies support at most eight tracks");
	if (p_frame >= p_movie.metasprite_count)
		throw std::runtime_error(
			"Selected movie pose is outside this movie's frame library");
	AtlasMovie backup{ p_movie };
	try {
		auto actor{ default_track(AtlasMovieTrackKind::CounterToggle, p_frame) };
		actor.x = p_x; actor.y = p_y;
		initialize_actor_editor(actor, p_movie.tracks.size(),
			std::format("Pose F{:02}", p_frame));
		p_movie.tracks.push_back(std::move(actor));
		m_atlas_movie_sel_track = p_movie.tracks.size() - 1;
		p_phase_index = std::min(p_phase_index, p_movie.phases.size() - 1);
		p_movie.phases[p_phase_index].update_mask
			|= static_cast<byte>(1u << m_atlas_movie_sel_track);
		p_movie.phases[p_phase_index].draw_mask
			|= static_cast<byte>(1u << m_atlas_movie_sel_track);
		AtlasMovieBundleCodec::validate(p_bundle);
		m_atlas_movie_dirty = true;
		m_atlas_movie_actor_place_mode = false;
		add_message(std::format(
			"Placed movie pose F{:02} as Track {} at {}, {}",
			p_frame, m_atlas_movie_sel_track, p_x, p_y), 2);
	}
	catch (...) {
		p_movie = std::move(backup);
		throw;
	}
}
