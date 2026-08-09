#include <format>
#include <stdexcept>
#include "ScriptManager.h"
#include "fe/ROM_Manager.h"
#include "fe/fe_constants.h"
#include "fi/fi_constants.h"
#include "fm/fm_constants.h"
#include "fi/IScriptLoader.h"
#include "fi/AsmWriter.h"
#include "fi/AsmReader.h"
#include "fb/BScriptWriter.h"
#include "fb/BScriptReader.h"
#include "fm/MScriptLoader.h"
#include "fm/MMLWriter.h"
#include "fm/MMLReader.h"
#include "fv/MiscWriter.h"
#include "fh/HackManager.h"
#include "common/klib/Kfile.h"
#include "common/klib/Kstring.h"

// helpers
void fe::script::message(const MessageCallback& p_callback, const std::string& p_message) {
	if (p_callback)
		p_callback(p_message);
}

void fe::script::try_patch(const std::string& p_data_type, std::size_t p_data_size,
	std::size_t p_data_max_size, const MessageCallback& p_message) {
	message(p_message, std::format(
		"Trying to patch {}: Using {} of {} available bytes ({:.2f}%)",
		p_data_type, p_data_size, p_data_max_size,
		100.0f * static_cast<float>(p_data_size) / static_cast<float>(p_data_max_size)));

	if (p_data_size > p_data_max_size)
		throw std::runtime_error(std::format("Size limits exceeded for {}", p_data_type));
}

// iScripts
std::vector<byte> fe::script::asm_iscripts(const Config& p_config, const std::vector<byte>& p_rom,
	const std::vector<std::string>& p_asm, const fi::ScriptOpcodeInfo& p_opcode_info,
	bool p_strict, const MessageCallback& p_message) {
	auto rom{ p_rom };

	if (p_strict)
		message(p_message, "Using strict mode - Only original ROM data region will be used");

	fi::AsmReader reader;

	std::size_t l_iscript_rg2_start{ p_config.constant(fi::c::ID_ISCRIPT_RG2_START) };

	if (!p_opcode_info.required_impls.empty()) {
		if (p_strict)
			throw std::runtime_error("Strict mode cannot be used with extended script library routines");

		fh::HackManager hack_mgr;
		std::vector<fh::HackLib> required_libs;
		for (const auto& impl : p_opcode_info.required_impls) {
			try {
				required_libs.push_back(klib::str::parse_enum_ci<fh::HackLib>(impl));
			}
			catch (const std::exception&) {
				throw std::runtime_error(std::format("Unknown script implementation '{}'", impl));
			}
		}

		const auto old_rg2_start{ l_iscript_rg2_start };

		l_iscript_rg2_start = hack_mgr.apply_script_library(p_config, rom, l_iscript_rg2_start,
			required_libs, p_opcode_info.base_opcode_count);

		message(p_message, std::format("Installed new script library routines ({} bytes)",
			l_iscript_rg2_start - old_rg2_start));
	}

	reader.read_asm(p_config, p_asm, l_iscript_rg2_start);

	auto bytes{ reader.get_script_bytes(p_config) };
	auto strbytes{ reader.get_string_bytes(p_config) };

	message(p_message, std::format("Using {} unique strings out of a maximum of 255", reader.get_string_count()));

	std::size_t l_iscript_string_start{ p_config.constant(fi::c::ID_STRING_DATA_START) };
	std::size_t l_iscript_string_end{ p_config.constant(fi::c::ID_STRING_DATA_END) };

	std::size_t l_size_strings{ l_iscript_string_end - l_iscript_string_start };
	std::size_t l_iscript_rg2_size{ p_config.constant(fi::c::ID_ISCRIPT_RG2_END) - l_iscript_rg2_start };

	auto l_iscript_ptr{ p_config.pointer(fi::c::ID_ISCRIPT_PTR_LO) };

	std::size_t l_iscript_rg1_size{ p_config.constant(fi::c::ID_ISCRIPT_RG1_END) - l_iscript_ptr.first };

	try_patch("strings", strbytes.size(), l_size_strings, p_message);
	try_patch(std::format("pointer table ({} entries) and script data (region 1)",
		reader.get_entrypoint_count()), bytes.first.size(), l_iscript_rg1_size, p_message);
	try_patch("script data (region 2)", bytes.second.size(), l_iscript_rg2_size, p_message);

	if (p_strict && !bytes.second.empty())
		throw std::runtime_error("Strict mode was enabled but the original ROM region could not fit all data");

	// patch region 1 (ptr table + bytecode)
	for (std::size_t i{ 0 }; i < bytes.first.size(); ++i)
		rom.at(i + l_iscript_ptr.first) = bytes.first[i];
	// patch region 2 (bytecode)
	for (std::size_t i{ 0 }; i < bytes.second.size(); ++i) {
		rom.at(i + (!p_strict ? l_iscript_rg2_start : l_iscript_ptr.first + bytes.first.size())) = bytes.second[i];
	}
	// patch string data
	for (std::size_t i{ 0 }; i < strbytes.size(); ++i)
		rom.at(i + l_iscript_string_start) = strbytes[i];
	// zero out the remainder of string space to make sure no garbage string data is extracted from here later
	for (std::size_t i{ strbytes.size() }; i < l_size_strings; ++i)
		rom.at(i + l_iscript_string_start) = 0x00;

	std::size_t l_hi_byte_addr_bank_rel{ l_iscript_ptr.first + reader.get_entrypoint_count() -
	l_iscript_ptr.second };
	std::size_t l_rom_offset_hi_byte_ref{ p_config.constant(fi::c::ID_ISCRIPT_HI_REF_OFFSET) };

	// update the reference to iScript ptr table hi bytes start
	rom.at(l_rom_offset_hi_byte_ref) = static_cast<byte>(l_hi_byte_addr_bank_rel % 256);
	rom.at(l_rom_offset_hi_byte_ref + 1) = static_cast<byte>(l_hi_byte_addr_bank_rel / 256);

	const auto& tmchanges{ reader.get_tilemap_changes() };

	// add tilemap change subsystem if a [tilemap_changes] section was present in the asm
	if (!tmchanges.empty()) {
		fh::HackManager hack_mgr;
		std::size_t tmsub_size{ hack_mgr.apply_tilemap_change_subsystem(p_config, rom, tmchanges) };
		message(p_message, std::format("Installed tilemap change subsystem ({} bytes)", tmsub_size));
	}

	// duplicate bank 15 if needed
	if (fe::ROM_Manager::duplicate_static_bank_if_needed(p_config, rom))
		message(p_message, "Bank 15 was duplicated to bank 31 post-patch");

	message(p_message, "Verifying generated ROM contents");

	// parse it to see if disassembly succeeds without throwing
	try {
		fi::IScriptLoader staticanalysisread(p_config, rom);
	}
	catch (const std::runtime_error& ex) {
		throw std::runtime_error(std::format(
			"Invalid ROM generated. Ensure all code paths end, and that each "
			"entrypoint has a textbox context\n{}", ex.what()));
	}

	return rom;
}

void fe::script::asm_iscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_asm_filename, const std::string& p_out_filename,
	const fi::ScriptOpcodeInfo& p_opcode_info, bool p_strict,
	const MessageCallback& p_message) {

	message(p_message, std::format("Attempting to parse assembly file {}", p_asm_filename));
	const auto asm_code{ klib::file::read_file_as_strings(p_asm_filename) };
	const auto patched_rom{ asm_iscripts(p_config, p_rom, asm_code,	p_opcode_info,p_strict,	p_message) };

	message(p_message, std::format("Attempting to patch file {}", p_out_filename));
	klib::file::write_bytes_to_file(patched_rom, p_out_filename);
	message(p_message, "File patched");
}

std::string fe::script::disasm_iscripts(const fe::Config& p_config, const std::vector<byte>& p_rom,
	bool p_shop_comments, std::size_t& p_entrypoint_count) {

	fi::IScriptLoader loader(p_config, p_rom);
	loader.parse_rom(p_rom);

	p_entrypoint_count = loader.get_script_count();

	fi::AsmWriter asmw;
	return asmw.generate_asm(
		p_config,
		loader.get_instructions(),
		loader.get_ptr_table(),
		loader.get_jump_targets(),
		loader.get_strings(),
		loader.get_shops(),
		p_shop_comments);
}

void fe::script::disasm_iscripts_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_filename, bool p_shop_comments, bool p_overwrite,
	const MessageCallback& p_message) {
	std::size_t script_count{ 0 };

	if (!p_overwrite && klib::file::file_exists(p_filename))
		throw std::runtime_error(std::format("Assembly file {} exists, and overwrite-flag is not set", p_filename));

	message(p_message, "Attempting to parse ROM scripting layer");
	const auto asm_code{ disasm_iscripts(p_config, p_rom, p_shop_comments, script_count) };
	message(p_message, std::format("Detected {} script entrypoints", script_count));
	message(p_message, std::format("Generating output file {}", p_filename));
	klib::file::write_string_to_file(asm_code, p_filename);
	message(p_message, "Extraction complete!");
}

// bScripts
std::vector<byte> fe::script::asm_bscripts(const Config& p_config, const std::vector<byte>& p_rom,
	const std::vector<std::string>& p_asm, bool p_strict, const MessageCallback& p_message) {
	auto rom{ p_rom };

	if (p_strict)
		message(p_message, "Using strict mode - Only original ROM data region will be used");

	fb::BScriptReader reader(p_config);
	reader.read_asm(p_asm, p_config);

	const auto bytes{ reader.to_bytes() };

	message(p_message, std::format("Total script byte size (including ptr table): {}",
		bytes.first.size() + bytes.second.size()));

	auto bscriptptr{ p_config.pointer(fb::c::ID_BSCRIPT_PTR) };

	std::size_t l_bscript_rg1_end{ p_config.constant(fb::c::ID_BSCRIPT_RG1_END) };
	std::size_t l_bscript_rg1_size{ l_bscript_rg1_end - bscriptptr.first };

	std::size_t l_rg2_start{ p_config.constant(fb::c::ID_BSCRIPT_RG2_START) };
	std::size_t l_rg2_end{ p_config.constant(fb::c::ID_BSCRIPT_RG2_END) };
	std::size_t l_bscript_rg2_size{ l_rg2_end - l_rg2_start };

	try_patch("bscript pointer table and data (region 1)", bytes.first.size(), l_bscript_rg1_size, p_message);
	try_patch("bscript data (region 2)", bytes.second.size(), l_bscript_rg2_size, p_message);

	if (p_strict && !bytes.second.empty())
		throw std::runtime_error("Strict mode was enabled but the original ROM region could not fit all data");

	// clearing out the first section (TODO: this can probably be removed)
	fe::ROM_Manager::clear_rom_section(rom, bscriptptr.first, l_bscript_rg1_end);

	for (std::size_t i{ 0 }; i < bytes.first.size(); ++i)
		rom.at(bscriptptr.first + i) = bytes.first[i];
	for (std::size_t i{ 0 }; i < bytes.second.size(); ++i)
		rom.at(l_rg2_start + i) = bytes.second[i];

	message(p_message, "Verifying generated ROM contents");
	// parse generated ROM to verify the behavior script layer is valid
	try {
		fb::BScriptLoader staticanalysisread(p_config, rom);
	}
	catch (const std::runtime_error& ex) {
		throw std::runtime_error(std::format("Invalid ROM generated. Ensure all code paths end\n{}",
			ex.what()));
	}

	return rom;
}

void fe::script::asm_bscripts_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_basm_filename, const std::string& p_out_filename, bool p_strict,
	const MessageCallback& p_message) {

	message(p_message,
		std::format("Attempting to parse assembly file {}", p_basm_filename));

	const auto asm_code{ klib::file::read_file_as_strings(p_basm_filename) };
	const auto patched_rom{ asm_bscripts(p_config,p_rom,asm_code,p_strict,p_message) };

	message(p_message, std::format("Attempting to patch file {}", p_out_filename));
	klib::file::write_bytes_to_file(patched_rom, p_out_filename);
	message(p_message, "File patched");
}

std::string fe::script::disasm_bscripts(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	fb::BScriptLoader loader(p_config, p_rom);
	loader.parse_rom();

	fb::BScriptWriter asmw(p_config);
	return asmw.generate_asm(loader);
}

void fe::script::disasm_bscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_filename, bool p_overwrite, const MessageCallback& p_message) {

	if (!p_overwrite && klib::file::file_exists(p_filename))
		throw std::runtime_error(std::format("Assembly file {} exists, and overwrite-flag is not set", p_filename));

	message(p_message, "Attempting to parse ROM behavior script layer");
	const auto asm_code{ disasm_bscripts(p_config, p_rom) };
	message(p_message, std::format("Generating output file {}", p_filename));
	klib::file::write_string_to_file(asm_code, p_filename);
	message(p_message, "Extraction complete!");
}

// mScripts
std::vector<byte> fe::script::asm_mscripts(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::vector<std::string>& p_asm, const MessageCallback& p_message) {
	auto rom{ p_rom };

	fm::MMLReader reader(p_config);
	reader.read_mscript_asm(p_asm, p_config);

	const auto bytes{ reader.get_bytes() };
	const auto& musicptr{ p_config.pointer(fm::c::ID_MUSIC_PTR) };

	try_patch("Music", bytes.size(), p_config.constant(fm::c::ID_MUSIC_DATA_END) - musicptr.first, p_message);
	for (std::size_t i{ 0 }; i < bytes.size(); ++i)
		rom.at(musicptr.first + i) = bytes[i];

	return rom;
}

void fe::script::asm_mscripts_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_asm_filename, const std::string& p_out_filename,
	const MessageCallback& p_message) {

	message(p_message, std::format("Attempting to parse assembly file {}", p_asm_filename));
	const auto asm_code{ klib::file::read_file_as_strings(p_asm_filename) };
	const auto patched_rom{ asm_mscripts(p_config, p_rom, asm_code, p_message) };

	message(p_message, std::format("Attempting to patch file {}", p_out_filename));
	klib::file::write_bytes_to_file(patched_rom, p_out_filename);
	message(p_message, "File patched");
}

std::string fe::script::disasm_mscripts(const fe::Config& p_config, const std::vector<byte>& p_rom,
	bool p_emit_notes, const MessageCallback& p_message) {
	fm::MScriptLoader loader(p_config, p_rom);
	loader.parse_rom();

	message(p_message, std::format("Detected {} music tracks", loader.get_song_count()));

	fm::MMLWriter writer(p_config);
	return writer.generate_mml(
		loader.m_instrs,
		loader.m_opcodes,
		loader.m_ptr_table,
		loader.m_jump_targets,
		loader.m_chan_pitch_offsets,
		p_emit_notes);
}

void fe::script::disasm_mscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_filename, bool p_emit_notes, bool p_overwrite,
	const MessageCallback& p_message) {

	if (!p_overwrite && klib::file::file_exists(p_filename))
		throw std::runtime_error(std::format(
			"Music asm file {} exists, and overwrite-flag is not set",
			p_filename));

	message(p_message, std::format("Note value emission {}", p_emit_notes ? "enabled" : "disabled"));

	message(p_message, "Attempting to parse ROM music layer");
	const auto mscript_asm{ disasm_mscripts(p_config, p_rom, p_emit_notes, p_message) };
	message(p_message, std::format("Generating output file {}", p_filename));
	klib::file::write_string_to_file(mscript_asm, p_filename);
	message(p_message, "Extraction complete!");
}

// misc interface
std::vector<byte> fe::script::build_misc(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::vector<std::string>& p_txt, const MessageCallback& p_message) {
	auto rom{ p_rom };

	fv::MiscWriter reader(rom, p_config);
	reader.load_txt(p_txt);

	const int item_count{ reader.patch_rom(rom, p_config) };

	if (fe::ROM_Manager::duplicate_static_bank_if_needed(p_config, rom))
		message(p_message, "Bank 15 was duplicated to bank 31 post-patch");

	message(p_message, std::format("Patched {} miscellaneous data items", item_count));

	return rom;
}

void fe::script::build_misc_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_txt_filename, const std::string& p_out_filename,
	const MessageCallback& p_message) {
	message(p_message, std::format("Attempting to parse {}", p_txt_filename));

	const auto txt{ klib::file::read_file_as_strings(p_txt_filename) };
	const auto patched_rom{ build_misc(p_config, p_rom, txt, p_message) };

	message(p_message, std::format("Attempting to patch {}", p_out_filename));
	klib::file::write_bytes_to_file(patched_rom, p_out_filename);
	message(p_message, std::format("Misc data written to file {}!", p_out_filename));
}

std::string fe::script::extract_misc(const fe::Config& p_config, const std::vector<byte>& p_rom,
	bool p_include_all_sprites) {
	fv::MiscWriter writer(p_rom, p_config, p_include_all_sprites);
	writer.load_rom(p_rom, p_config);
	return writer.generate_txt();
}

void fe::script::extract_misc_to_file(const Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_filename, bool p_include_all_sprites, bool p_overwrite,
	const MessageCallback& p_message) {

	if (!p_overwrite && klib::file::file_exists(p_filename))
		throw std::runtime_error(std::format("Txt file {} exists, and overwrite-flag is not set", p_filename));

	if (p_include_all_sprites)
		message(p_message, "Will output misc data for all sprites (not only enemies and bosses)");

	message(p_message, "Extracting miscellaneous ROM data");

	const auto txt{ extract_misc(p_config, p_rom, p_include_all_sprites) };

	message(p_message, std::format("Generating output file {}", p_filename));
	klib::file::write_string_to_file(txt, p_filename);
	message(p_message, std::format("Extraction to {} complete!", p_filename));
}
