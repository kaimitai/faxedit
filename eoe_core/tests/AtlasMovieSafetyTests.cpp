#include "common/klib/Asm6502.h"
#include "fe/AtlasMovieAssets.h"
#include "fe/AtlasMovieBundle.h"
#include "fe/AtlasMovieCompatibility.h"
#include "fe/AtlasMovieEditor.h"
#include "fe/AtlasMovieEngine.h"
#include "fe/AtlasMovieLayout.h"
#include "fe/AtlasMovieRuntime.h"
#include "fe/Config.h"
#include "fe/script/ScriptManager.h"
#include "fh/HackManager.h"
#include "fi/Opcode.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

using byte = unsigned char;

namespace {

	std::vector<byte> read_bytes(const std::filesystem::path& p_path) {
		std::ifstream stream(p_path, std::ios::binary);
		if (!stream) throw std::runtime_error("unable to read " + p_path.string());
		return { std::istreambuf_iterator<char>(stream), {} };
	}

	void write_text(const std::filesystem::path& p_path, const std::string& p_text) {
		std::ofstream stream(p_path);
		if (!stream) throw std::runtime_error("unable to write " + p_path.string());
		stream << p_text;
		if (!stream) throw std::runtime_error("unable to finish " + p_path.string());
	}

	void write_cpu_word(std::vector<byte>& p_rom, byte p_bank,
		std::uint16_t p_cpu, std::uint16_t p_value) {
		const auto offset{ fe::atlas_movie::layout::file_offset(p_bank, p_cpu) };
		if (offset + 2 > p_rom.size())
			throw std::runtime_error("test word write is outside ROM");
		p_rom[offset] = static_cast<byte>(p_value);
		p_rom[offset + 1] = static_cast<byte>(p_value >> 8);
	}

	void require(bool p_value, const char* p_label) {
		if (!p_value) throw std::runtime_error(p_label);
		std::cout << "PASS " << p_label << '\n';
	}

	std::vector<byte> visible_metasprites(byte p_count, byte p_width,
		byte p_height) {
		std::vector<byte> result(static_cast<std::size_t>(p_count) * 2, 0);
		for (byte frame{}; frame < p_count; ++frame) {
			result.insert(result.end(), { 0, 0, p_width, p_height });
			for (std::size_t cell{};
				cell < static_cast<std::size_t>(p_width) * p_height; ++cell)
				result.insert(result.end(), { 0, 0 });
		}
		return result;
	}

	template<class Action>
	void require_rejected(Action&& p_action, const char* p_label) {
		try { p_action(); }
		catch (const std::exception&) {
			std::cout << "PASS " << p_label << '\n';
			return;
		}
		throw std::runtime_error(p_label);
	}

	struct RemoveOnExit {
		std::filesystem::path path;
		~RemoveOnExit(void) { std::error_code error; std::filesystem::remove(path, error); }
	};

}

int main(int argc, char** argv) try {
	if (argc != 4) return 2;
	const std::filesystem::path config_path{ argv[1] };
	const std::filesystem::path rom_path{ argv[2] };
	const std::filesystem::path override_path{ argv[3] };
	RemoveOnExit remove_override{ override_path };
	const auto clean_rom{ read_bytes(rom_path) };
	fe::Config base_config(config_path.string(), "", clean_rom);
	require(base_config.get_region() == "us", "safety tests use USA Rev 0");
	const auto configured_opcodes{
		fe::script::get_iscript_opcode_info(base_config) };
	require(configured_opcodes.opcodes.size() == 0x18
		&& configured_opcodes.required_impls.empty(),
		"base config leaves extended movie opcodes inactive");
	fe::AtlasMovieRuntime::validate_shared_install(configured_opcodes);
	require(true, "base config is eligible for Shared installation");

	const auto project{ fe::atlas_movie::make_starter_project() };
	const auto fmb{ fe::AtlasMovieBundleCodec::compile(project) };
	const auto package{ fe::AtlasMovieEngine::build_package(project) };
	fe::AtlasMovieEngine::validate_package(package);
	require(fe::AtlasMovieBundleCodec::extract_from_ame(package) == fmb,
		"compiled-in Shared engine packages the exact current FMB");
	auto shared_source_overlap{ project };
	shared_source_overlap.movies.front().assets[1].bank = 12;
	shared_source_overlap.movies.front().assets[1].cpu
		= fe::atlas_movie::layout::CORE_CPU;
	const auto shared_overlap_package{
		fe::AtlasMovieEngine::build_package(shared_source_overlap) };
	auto shared_overlap_rom{ clean_rom };
	const auto shared_overlap_before{ shared_overlap_rom };
	require_rejected([&] {
		(void)fe::AtlasMovieEngine::install(
			shared_overlap_rom, shared_overlap_package);
	}, "Shared rejects a ROM-owned graphic under its replacement core");
	require(shared_overlap_rom == shared_overlap_before,
		"failed Shared source-overlap install leaves the ROM unchanged");
	auto oam_overflow_project{ project };
	auto& crowded_movie{ oam_overflow_project.movies.front() };
	crowded_movie.imports.push_back({
		fe::AtlasMovieImportKind::MetaspriteLibrary, "crowded frames", 0, 2,
		visible_metasprites(2, 8, 5) });
	crowded_movie.metasprite_count = 2;
	crowded_movie.tracks.push_back(crowded_movie.tracks.front());
	for (auto& phase : crowded_movie.phases) phase.draw_mask |= 2;
	const auto oam_overflow_package{
		fe::AtlasMovieEngine::build_package(oam_overflow_project) };
	auto oam_overflow_rom{ clean_rom };
	const auto oam_overflow_before{ oam_overflow_rom };
	require_rejected([&] {
		(void)fe::AtlasMovieEngine::install(
			oam_overflow_rom, oam_overflow_package);
	}, "Shared rejects phases that may draw over 64 OAM cells");
	require(oam_overflow_rom == oam_overflow_before,
		"failed OAM-budget install leaves the ROM unchanged");
	constexpr std::array<byte, 5> scheduler_hook{
		0x20, 0xce, 0xfc, 0xea, 0xea };
	require(!fe::atlas_movie::has_atlas_resident_scheduler(clean_rom),
		"clean ROM has no Atlas Resident Scheduler signature");
	auto shared_scheduler_rom{ clean_rom };
	std::copy(scheduler_hook.begin(), scheduler_hook.end(),
		shared_scheduler_rom.begin() + static_cast<std::ptrdiff_t>(
			fe::atlas_movie::layout::file_offset(15, 0xc9af)));
	require(fe::atlas_movie::has_atlas_resident_scheduler(shared_scheduler_rom),
		"Atlas Resident Scheduler signature is detected for a compatibility warning");
	(void)fe::AtlasMovieEngine::install(shared_scheduler_rom, package);
	require(fe::AtlasMovieEngine::is_installed(shared_scheduler_rom)
		&& fe::atlas_movie::has_atlas_resident_scheduler(shared_scheduler_rom),
		"Shared installs without removing the detected Atlas Resident Scheduler");

	require(fe::atlas_movie::layout::file_offset(15, 0x8000)
		== fe::atlas_movie::layout::file_offset(15, 0xc000),
		"physical bank 15 is previewable through fixed and switchable windows");

	const auto vanilla{ fi::load_vanilla_opcodes() };
	const auto start_file{ base_config.constant("iscript_data_rg2_start") };
	const auto start_cpu{ klib::Asm6502::get_rom_address(start_file).CpuAddr };
	const auto table_bytes{ 2 * (vanilla.opcodes.size() + 1) };

	auto overlap_project{ project };
	overlap_project.movies.front().assets.front().bank = 12;
	overlap_project.movies.front().assets.front().cpu = start_cpu;
	const auto overlap_fmb{ fe::AtlasMovieBundleCodec::compile(overlap_project) };
	require_rejected([&] {
		(void)fe::AtlasMovieRuntime::build_standalone(overlap_fmb, start_cpu);
	}, "Standalone builder rejects a ROM asset under its player or FMB");
	require_rejected([&] {
		fe::AtlasMovieRuntime::validate_standalone_sources(
			overlap_fmb, start_cpu, static_cast<std::uint16_t>(start_cpu + 0x1000));
	}, "Standalone rejects a ROM asset under its generated allocation");

	auto boundary_project{ project };
	std::vector<byte> boundary_fmb;
	for (std::size_t phases{ boundary_project.movies.back().phases.size() };
		phases < 255; ++phases) {
		if (phases > boundary_project.movies.back().phases.size())
			boundary_project.movies.back().phases.push_back(
				boundary_project.movies.back().phases.back());
		try {
			auto candidate{ fe::AtlasMovieBundleCodec::compile(boundary_project) };
			const auto code{ fe::AtlasMovieRuntime::build_standalone(candidate, start_cpu) };
			const auto end{ static_cast<std::size_t>(start_cpu) + code.size() };
			if (end <= 0xc000 && end + table_bytes > 0xc000) {
				boundary_fmb = std::move(candidate);
				break;
			}
		}
		catch (const std::exception&) {}
	}
	require(!boundary_fmb.empty(), "constructed a valid player/FMB-only boundary witness");
	write_text(override_path,
		fe::AtlasMovieRuntime::standalone_config_override(vanilla, boundary_fmb));
	fe::Config boundary_config(config_path.string(), override_path.string(), clean_rom);
	auto boundary_rom{ clean_rom };
	const auto boundary_before{ boundary_rom };
	fh::HackManager manager;
	require_rejected([&] {
		(void)manager.apply_script_library(boundary_config, boundary_rom, start_file,
			{ fh::HackLib::AtlasDevPlayMovie }, vanilla.opcodes.size());
	}, "Standalone rejects a dispatch table crossing bank 12");
	require(boundary_rom == boundary_before,
		"failed script-library installation is transactional");

	write_text(override_path,
		fe::AtlasMovieRuntime::standalone_config_override(vanilla, fmb));
	fe::Config normal_config(config_path.string(), override_path.string(), clean_rom);
	auto standalone_scheduler_rom{ clean_rom };
	std::copy(scheduler_hook.begin(), scheduler_hook.end(),
		standalone_scheduler_rom.begin() + static_cast<std::ptrdiff_t>(
			fe::atlas_movie::layout::file_offset(15, 0xc9af)));
	const auto standalone_scheduler_end{
		fe::AtlasMovieRuntime::install_standalone(
			normal_config, standalone_scheduler_rom, start_cpu) };
	require(standalone_scheduler_end > start_cpu
		&& fe::atlas_movie::has_atlas_resident_scheduler(standalone_scheduler_rom),
		"Standalone installs without removing the detected Atlas Resident Scheduler");
	(void)fe::script::get_iscript_opcode_info(
		normal_config, standalone_scheduler_rom);
	auto record_overlap_rom{ clean_rom };
	const auto parsed_fmb{ fe::AtlasMovieBundleCodec::parse(fmb) };
	const auto& source_movie{ parsed_fmb.movies.front() };
	const auto record_cpu{ static_cast<std::uint16_t>(start_cpu + 6) };
	const auto record_offset{
		fe::atlas_movie::layout::file_offset(12, record_cpu) };
	constexpr std::array<byte, 5> record{ 0, 0, 1, 1, 0xff };
	std::copy(record.begin(), record.end(),
		record_overlap_rom.begin() + static_cast<std::ptrdiff_t>(record_offset));
	record_overlap_rom[fe::atlas_movie::layout::file_offset(
		12, source_movie.metasprite_pointer_lo)] = static_cast<byte>(record_cpu);
	record_overlap_rom[fe::atlas_movie::layout::file_offset(
		12, source_movie.metasprite_pointer_hi)] = static_cast<byte>(record_cpu >> 8);
	const auto record_overlap_before{ record_overlap_rom };
	require_rejected([&] {
		(void)fe::AtlasMovieRuntime::install_standalone(
			normal_config, record_overlap_rom, start_cpu);
	}, "Standalone rejects a pointed metasprite record under its player");
	require(record_overlap_rom == record_overlap_before,
		"failed Standalone source-overlap install leaves the ROM unchanged");
	auto normal_rom{ clean_rom };
	const auto normal_end{ manager.apply_script_library(normal_config, normal_rom,
		start_file, { fh::HackLib::AtlasDevPlayMovie }, vanilla.opcodes.size()) };
	require(normal_end <= base_config.constant("iscript_data_rg2_end")
		&& normal_rom != clean_rom,
		"normal Standalone library and dispatch table still install");
	auto table_record_rom{ clean_rom };
	const auto table_end_cpu{
		klib::Asm6502::get_rom_address(normal_end).CpuAddr };
	const auto table_record_cpu{ static_cast<std::uint16_t>(
		table_end_cpu - table_bytes) };
	const auto table_record_offset{
		fe::atlas_movie::layout::file_offset(12, table_record_cpu) };
	std::copy(record.begin(), record.end(),
		table_record_rom.begin() + static_cast<std::ptrdiff_t>(table_record_offset));
	table_record_rom[fe::atlas_movie::layout::file_offset(
		12, source_movie.metasprite_pointer_lo)]
		= static_cast<byte>(table_record_cpu);
	table_record_rom[fe::atlas_movie::layout::file_offset(
		12, source_movie.metasprite_pointer_hi)]
		= static_cast<byte>(table_record_cpu >> 8);
	const auto table_record_before{ table_record_rom };
	require_rejected([&] {
		(void)manager.apply_script_library(normal_config, table_record_rom,
			start_file, { fh::HackLib::AtlasDevPlayMovie }, vanilla.opcodes.size());
	}, "Standalone rejects a pointed metasprite record under its dispatch table");
	require(table_record_rom == table_record_before,
		"failed full-library source-overlap install is transactional");

	auto shared_rom{ clean_rom };
	(void)fe::AtlasMovieEngine::install(shared_rom, package);
	require(fe::AtlasMovieEngine::is_installed(shared_rom),
		"generated Shared package passes complete installation health check");
	auto combined_scheduler_rom{ shared_rom };
	std::copy(scheduler_hook.begin(), scheduler_hook.end(),
		combined_scheduler_rom.begin() + static_cast<std::ptrdiff_t>(
			fe::atlas_movie::layout::file_offset(15, 0xc9af)));
	const auto combined_info{ fe::AtlasMovieRuntime::resolve_opcode_info(
		vanilla, combined_scheduler_rom) };
	require(combined_info.opcodes.at(0x18).name == "AtlasDevPlayMovieShared",
		"existing Shared ROM resolves its opcode with the Atlas Resident Scheduler present");
	auto malformed_shared{ vanilla };
	malformed_shared.opcodes.emplace(0x18, fi::Opcode("AtlasDevPlayMovieShared",
		{ { fi::ArgType::Short, fi::ArgDomain::None } }, fi::Flow::Continue, false));
	require_rejected([&] {
		(void)fe::AtlasMovieRuntime::resolve_opcode_info(malformed_shared, shared_rom);
	}, "Shared mnemonic rejects noncanonical argument and flow metadata");

	auto bad_title{ shared_rom };
	bad_title[fe::atlas_movie::layout::file_offset(15, 0xfc98)] ^= 1;
	require(!fe::AtlasMovieEngine::is_installed(bad_title),
		"Shared health check rejects a damaged title call site");
	auto bad_bundle{ shared_rom };
	bad_bundle[fe::atlas_movie::layout::file_offset(
		12, fe::atlas_movie::layout::BUNDLE_CPU)] = 'X';
	require(!fe::AtlasMovieEngine::is_installed(bad_bundle),
		"Shared health check rejects an unreadable FMB");
	auto bad_dispatch{ shared_rom };
	const auto plain_low{ static_cast<std::uint16_t>(
		fe::atlas_movie::layout::BUNDLE_CPU + fmb.size()) };
	bad_dispatch[fe::atlas_movie::layout::file_offset(12, plain_low)] ^= 1;
	require(!fe::AtlasMovieEngine::is_installed(bad_dispatch),
		"Shared health check rejects a damaged vanilla dispatch entry");

	auto relocated_dispatch{ shared_rom };
	constexpr std::size_t relocated_count{ 26 };
	const auto relocated_low{ static_cast<std::uint16_t>(plain_low + 128) };
	const auto relocated_high{ static_cast<std::uint16_t>(
		relocated_low + relocated_count) };
	for (std::size_t i{}; i < fe::atlas_movie::layout::HANDLERS.size(); ++i) {
		const auto handler{ fe::atlas_movie::layout::HANDLERS[i] };
		relocated_dispatch[fe::atlas_movie::layout::file_offset(12, relocated_low) + i]
			= static_cast<byte>(handler);
		relocated_dispatch[fe::atlas_movie::layout::file_offset(12, relocated_high) + i]
			= static_cast<byte>(handler >> 8);
	}
	relocated_dispatch[fe::atlas_movie::layout::file_offset(12, relocated_low)
		+ relocated_count - 1] = 0x34;
	relocated_dispatch[fe::atlas_movie::layout::file_offset(12, relocated_high)
		+ relocated_count - 1] = 0x82;
	write_cpu_word(relocated_dispatch, 12,
		fe::atlas_movie::layout::DISPATCH_LOW_REF, relocated_low);
	write_cpu_word(relocated_dispatch, 12,
		fe::atlas_movie::layout::DISPATCH_HIGH_REF, relocated_high);
	require(fe::AtlasMovieEngine::is_installed(relocated_dispatch),
		"Shared health check accepts a valid generated-opcode table relocation");
	require(fe::AtlasMovieEngine::script_data_start(relocated_dispatch)
		== fe::atlas_movie::layout::file_offset(12,
			static_cast<std::uint16_t>(relocated_high + relocated_count)),
		"Shared script allocation follows the active relocated dispatch table");

	std::cout << "All Atlas Movie safety tests passed\n";
	return 0;
}
catch (const std::exception& ex) {
	std::cerr << "FAIL " << ex.what() << '\n';
	return 1;
}
