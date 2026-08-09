#include "Cli.h"
#include <format>
#include <iostream>
#include "fi/cli/application_constants.h"
#include "fi/fi_constants.h"
#include "fm/fm_constants.h"
#include <stdexcept>
#include "fb/fb_constants.h"
#include "fm/MMLReader.h"
#include "common/klib/Kfile.h"
#include "common/klib/Kstring.h"
#include "fm/song/MMLSong.h"
#include "fm/song/MMLSongCollection.h"
#include "fm/song/Tokenizer.h"
#include "fm/song/Parser.h"
#include "fv/MiscWriter.h"
#include "fh/HackManager.h"
#include "fe/script/ScriptManager.h"

#ifdef _WIN32
#include <windows.h>
#endif

constexpr char ID_DUPLICATE_STATIC_BANK[]{ "duplicate_static_bank" };

void fi::Cli::print_header(void) const {
	std::cout << fi::appc::APP_NAME << " v" << fi::appc::APP_VERSION << " - Faxanadu Script Assembler and Disassembler\n";
	std::cout << "Author: Kai E. Fr";
	output_oe_on_windows();
	std::cout << "land (" << fi::appc::APP_URL << ")\n";
	std::cout << "Build date: " << __DATE__ << " " << __TIME__ << " CET\n\n";
}

void fi::Cli::print_help(void) const {
	std::cout <<
		"Usage:\n"
		"  faxiscripts <command> <input> <output> [options]\n\n"
		"Commands:\n"
		"  IScripts (interaction scripts):\n"
		"    x,   extract            - Disassemble IScripts from ROM\n"
		"    b,   build              - Assemble IScripts into ROM\n"
		"\n"
		"  BScripts (behavior scripts):\n"
		"    xb,  extract-bscript    - Disassemble BScripts from ROM\n"
		"    bb,  build-bscript      - Assemble BScripts and patch ROM\n"
		"\n"
		"  MScripts (low level music format):\n"
		"    xm,  extract-music     - Disassemble MScripts from ROM\n"
		"    bm,  build-music       - Assemble MScripts and patch ROM\n"
		"\n"
		"  MML (high level music format):\n"
		"    xmml, extract-mml      - Extract music as MML from ROM\n"
		"    bmml, build-mml        - Compile MML and patch ROM\n"
		"\n"
		"  Miscellaneous strings and constants:\n"
		"    xmisc, extract-misc    - Extract miscellaneous data from ROM\n"
		"    bmisc, build-misc      - Patch ROM with miscellaneous data\n"
		"\n"
		"  MIDI:\n"
		"    m2m, mml-to-midi       - Convert MML to MIDI files\n"
		"    r2m, rom-to-midi       - Extract music from ROM as MIDI files\n"
		"\n"
		"  LilyPond:\n"
		"    m2l, mml-to-ly         - Convert MML to LilyPond files\n"
		"    r2l, rom-to-ly         - Extract music from ROM as LilyPond files\n\n";

	std::cout << "Options:\n";
	std::cout << "  Common options:\n";
	std::cout << "    -r, --region                 ROM region which must be defined in the configuration xml (auto-detected by default)\n";
	std::cout << "    -f, --force                  Force file overwrite when extracting data (disabled by default)\n";
	std::cout << "    -s, --source-rom             Source ROM when assembling (by default the output file itself)\n";
	std::cout << "    -o, --original-size          Only patch original ROM location (disabled by default)\n";
	std::cout << "  IScript options:\n";
	std::cout << "    -p, --no-shop-comments       Disable shop comment extraction (enabled by default)\n";
	std::cout << "  MScript options:\n";
	std::cout << "    -n, --no-notes               Do not emit notes in music disassembly (notes enabled by default)\n";
	std::cout << "  MML options:\n";
	std::cout << "    -lp, --lilypond-percussion   Add percussion staff to the LilyPond output (disabled by default)\n";
}

fi::Cli::Cli(int argc, char** argv) :
	m_strict{ false },
	m_shop_comments{ true },
	m_overwrite{ false },
	m_notes{ true },
	m_lilypond_percussion{ false }
{
	print_header();

	if (argc < 4) {
		print_help();
		return;
	}
	else {
		set_mode(argv[1]);
		m_in_file = argv[2];
		m_out_file = argv[3];
		parse_arguments(4, argc, argv);
	}

	m_config.load_definitions(appc::CONFIG_XML, appc::CONFIG_OVERRIDE_FILE_NAME);

	// we have the info we need to execute
	// IScript dispatch
	if (m_script_mode == fi::ScriptMode::IScriptBuild) {
		asm_to_nes(m_in_file, m_out_file,
			m_source_rom.empty() ? m_out_file : m_source_rom,
			m_strict);
	}
	else if (m_script_mode == fi::ScriptMode::IScriptExtract)
		nes_to_asm(m_in_file, m_out_file, m_shop_comments, m_overwrite);
	// BScript dispatch
	else if (m_script_mode == fi::ScriptMode::BScriptBuild) {
		basm_to_nes(m_in_file, m_out_file,
			m_source_rom.empty() ? m_out_file : m_source_rom,
			m_strict);
	}
	else if (m_script_mode == fi::ScriptMode::BScriptExtract)
		nes_to_basm(m_in_file, m_out_file, m_overwrite);
	// MScript dispatch
	else if (m_script_mode == fi::ScriptMode::MScriptBuild)
		masm_to_nes(m_in_file, m_out_file, m_source_rom.empty() ? m_out_file : m_source_rom);
	else if (m_script_mode == fi::ScriptMode::MScriptExtract)
		nes_to_masm(m_in_file, m_out_file, m_overwrite);
	// MML dispatch
	else if (m_script_mode == fi::ScriptMode::MmlBuild)
		mml_to_nes(m_in_file, m_out_file, m_source_rom.empty() ? m_out_file : m_source_rom);
	else if (m_script_mode == fi::ScriptMode::MmlExtract)
		nes_to_mml(m_in_file, m_out_file, m_overwrite);
	// midi dispatch
	else if (m_script_mode == fi::ScriptMode::MmlToMidi)
		mml_to_midi(m_in_file, m_out_file);
	else if (m_script_mode == fi::ScriptMode::RomToMidi)
		rom_to_midi(m_in_file, m_out_file);
	// Lilypond dispatch
	else if (m_script_mode == fi::ScriptMode::MmlToLilyPond)
		mml_to_lilypond(m_in_file, m_out_file);
	else if (m_script_mode == fi::ScriptMode::RomToLilyPond)
		rom_to_lilypond(m_in_file, m_out_file);
	// miscellaneous data dispatch
	else if (m_script_mode == fi::ScriptMode::MiscBuild)
		misc_to_nes(m_in_file, m_out_file, m_source_rom.empty() ? m_out_file : m_source_rom);
	else if (m_script_mode == fi::ScriptMode::MiscExtract)
		nes_to_misc(m_in_file, m_out_file, m_overwrite);
	// debug
	else if (m_script_mode == fi::ScriptMode::DumpConfig)
		dump_config(m_in_file, m_out_file);
	// can't really happen
	else
		throw(std::runtime_error("Invalid script mode"));
}

static void try_patch_msg(const std::string& p_data_type,
	std::size_t p_data_size, std::size_t p_data_max_size) {
	std::cout << std::format("Trying to patch {}: Using {} of {} available bytes ({:.2f}%)\n",
		p_data_type, p_data_size, p_data_max_size,
		100.0f * static_cast<float>(p_data_size) / static_cast<float>(p_data_max_size));
	if (p_data_size > p_data_max_size)
		throw std::runtime_error(std::format("Size limits exceeded for {}",
			p_data_type));
}

void fi::Cli::asm_to_nes(const std::string& p_asm_filename,
	const std::string& p_out_filename,
	const std::string& p_source_rom_filename,
	bool p_strict) {

	auto rom{ load_rom_and_determine_region(p_source_rom_filename) };
	auto opcode_defs{ fi::load_iscript_opcodes_from_config(m_config.bmap_dense(fi::c::ID_ISCRIPT_OPCODES),
			m_config.str_map(fi::c::ID_ISCRIPT_OPCODE_IMPLS)) };

	fe::script::asm_iscripts_to_file(m_config, rom, p_asm_filename, p_out_filename, opcode_defs, p_strict,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::basm_to_nes(const std::string& p_basm_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename,
	bool p_strict) {

	auto rom{ load_rom_and_determine_region(p_source_rom_filename) };
	fe::script::asm_bscripts_to_file(m_config, rom, p_basm_filename, p_nes_filename, p_strict,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});

}

void fi::Cli::masm_to_nes(const std::string& p_mml_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename) {

	std::cout << "Attempting to read ROM contents from " << p_source_rom_filename << "\n";
	auto rom{ klib::file::read_file_as_bytes(p_source_rom_filename) };

	if (m_region.empty()) {
		m_config.determine_region(rom);
		std::cout << "ROM region resolved to '" << m_config.get_region() << "'\n";
	}
	else {
		m_config.set_region(m_region);
		std::cout << "ROM region specified as '" << m_region << "'\n";
	}

	m_config.load_config_data(appc::CONFIG_XML, appc::CONFIG_OVERRIDE_FILE_NAME, rom);

	fm::MMLReader reader(m_config);

	std::cout << "Attempting to parse assembly file " << p_mml_filename << "\n";
	reader.read_mml_file(p_mml_filename, m_config);

	auto bytes{ reader.get_bytes() };

	const auto& musicptr{ m_config.pointer(fm::c::ID_MUSIC_PTR) };

	try_patch_msg("Music", bytes.size(),
		m_config.constant(fm::c::ID_MUSIC_DATA_END) - musicptr.first);

	for (std::size_t i{ 0 }; i < bytes.size(); ++i)
		rom.at(musicptr.first + i) = bytes[i];

	std::cout << "Attempting to patch file " << p_nes_filename << "\n";
	klib::file::write_bytes_to_file(rom, p_nes_filename);
	std::cout << "File patched\n";
}

void fi::Cli::misc_to_nes(const std::string& p_txt_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename) {
	auto rom{ load_rom_and_determine_region(p_source_rom_filename) };
	fv::MiscWriter reader(rom, m_config);

	std::cout << "Attempting to parse " << p_txt_filename << "\n";
	reader.load_txt_file(p_txt_filename);

	std::cout << "Attempting to ptach " << p_nes_filename << "\n";
	int itemcnt{ reader.patch_rom(rom, m_config) };

	// bank 15 was mutated - duplicate to bank 31 post-patch for expanded roms
	if (m_config.boolean_or(ID_DUPLICATE_STATIC_BANK, false)) {
		constexpr std::size_t BANK_BYTE_SIZE{ 0x4000 };
		std::size_t source_idx{ 0x10 + BANK_BYTE_SIZE * 0x0f };
		std::size_t target_idx{ 0x10 + BANK_BYTE_SIZE * 0x1f };

		for (std::size_t i{ 0 }; i < BANK_BYTE_SIZE; ++i)
			rom.at(target_idx + i) = rom[source_idx + i];

		std::cout << "Bank 15 was duplicated to bank 31 post-patch\n";
	}

	klib::file::write_bytes_to_file(rom, p_nes_filename);
	std::cout << std::format("Misc data ({} items) written to file ", itemcnt) << p_nes_filename << "!\n";
}

void fi::Cli::nes_to_asm(const std::string& p_nes_filename,
	const std::string& p_asm_filename, bool p_shop_comments, bool p_overwrite) {

	// show params
	if (p_shop_comments)
		std::cout << "Will show shop data as comments where they are referenced\n";
	if (p_overwrite)
		std::cout << "Will overwrite output assembly file if it already exists\n";

	const auto rom_data{ load_rom_and_determine_region(p_nes_filename) };

	fi::load_iscript_opcodes_from_config(m_config.bmap_dense(fi::c::ID_ISCRIPT_OPCODES),
		m_config.str_map(fi::c::ID_ISCRIPT_OPCODE_IMPLS));

	fe::script::disasm_iscripts_to_file(m_config, rom_data, p_asm_filename, p_shop_comments, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_basm(const std::string& p_nes_filename,
	const std::string& p_basm_filename, bool p_overwrite) {

	// show params
	if (p_overwrite)
		std::cout << "Will overwrite output assembly file if it already exists\n";

	const auto rom_data{ load_rom_and_determine_region(p_nes_filename) };
	fe::script::disasm_bscripts_to_file(m_config, rom_data, p_basm_filename, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_masm(const std::string& p_nes_filename,
	const std::string& p_mml_filename, bool p_overwrite) {

	const auto rom_data{ load_rom_and_determine_region(p_nes_filename) };

	fe::script::disasm_mscripts_to_file(m_config, rom_data, p_mml_filename, m_notes, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_misc(const std::string& p_nes_filename,
	const std::string& p_txt_filename,
	bool p_overwrite) {

	if (m_strict)
		std::cout << "Will output misc data for all sprites (not only enemies and bosses)\n";

	// fail early if output file already exists and we do not overwrite
	if (!p_overwrite && klib::file::file_exists(p_txt_filename))
		throw std::runtime_error(std::format("txt file {} exists, and overwrite-flag is not set", p_txt_filename));

	const auto rom_data{ load_rom_and_determine_region(p_nes_filename) };

	fv::MiscWriter writer(rom_data, m_config, m_strict);
	writer.load_rom(rom_data, m_config);
	writer.write_txt_file(p_txt_filename);

	std::cout << std::format("Extraction to {} complete!\n", p_txt_filename);
}

void fi::Cli::nes_to_mml(const std::string& p_nes_filename,
	const std::string& p_mml_filename,
	bool p_overwrite) {

	// fail early if output file already exists and we do not overwrite
	if (!p_overwrite && klib::file::file_exists(p_mml_filename))
		throw std::runtime_error(std::format("mml file {} exists, and overwrite-flag is not set", p_mml_filename));

	auto rom_data{ load_rom_and_determine_region(p_nes_filename) };
	fm::MScriptLoader loader(m_config, rom_data);

	fm::MMLSongCollection coll(get_global_transpose(rom_data));
	coll.extract_bytecode_collection(loader);

	klib::file::write_string_to_file(coll.to_string(), p_mml_filename);

	std::cout << "MML extracted to " << p_mml_filename << "!\n";
}

void fi::Cli::mml_to_nes(const std::string& p_mml_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename) {

	auto rom{ load_rom_and_determine_region(p_source_rom_filename) };
	auto coll{ load_mml_file(p_mml_filename) };

	auto bytes{ coll.to_bytecode(m_config) };

	const auto& musicptr{ m_config.pointer(fm::c::ID_MUSIC_PTR) };

	try_patch_msg("Music", bytes.size(),
		m_config.constant(fm::c::ID_MUSIC_DATA_END) - musicptr.first);

	for (std::size_t i{ 0 }; i < bytes.size(); ++i)
		rom.at(musicptr.first + i) = bytes[i];

	std::cout << "Attempting to patch file " << p_nes_filename << "\n";
	klib::file::write_bytes_to_file(rom, p_nes_filename);
	std::cout << "File patched\n";
}

void fi::Cli::rom_to_midi(const std::string& p_nes_filename,
	const std::string& p_out_file_prefix) {

	auto rom_data{ load_rom_and_determine_region(p_nes_filename) };
	fm::MScriptLoader loader(m_config, rom_data);
	fm::MMLSongCollection coll(get_global_transpose(rom_data));
	coll.extract_bytecode_collection(loader);
	save_midi_files(coll, p_out_file_prefix);
}

void fi::Cli::mml_to_midi(const std::string& p_mml_filename,
	const std::string& p_out_file_prefix) {
	auto coll{ load_mml_file(p_mml_filename) };
	save_midi_files(coll, p_out_file_prefix);
}

void fi::Cli::rom_to_lilypond(const std::string& p_nes_filename,
	const std::string& p_out_file_prefix) {

	auto rom_data{ load_rom_and_determine_region(p_nes_filename) };
	fm::MScriptLoader loader(m_config, rom_data);
	fm::MMLSongCollection coll(get_global_transpose(rom_data));
	coll.extract_bytecode_collection(loader);
	save_lilypond_files(coll, p_out_file_prefix);
}

void fi::Cli::mml_to_lilypond(const std::string& p_mml_filename,
	const std::string& p_out_file_prefix) {
	auto coll{ load_mml_file(p_mml_filename) };
	save_lilypond_files(coll, p_out_file_prefix);
}

void fi::Cli::save_midi_files(fm::MMLSongCollection& coll,
	const std::string& p_out_file_prefix) const {
	std::cout << "Attempting to write midi files...\n";

	auto midis{ coll.to_midi() };

	for (std::size_t i{ 0 }; i < midis.size(); ++i) {
		std::string l_filename{ std::format("{}-{:02}.mid", p_out_file_prefix, i + 1) };
		midis[i].write(l_filename);
		std::cout << "Wrote " << l_filename << "!\n";
	}
}

void fi::Cli::save_lilypond_files(fm::MMLSongCollection& coll,
	const std::string& p_out_file_prefix) const {
	std::cout << std::format("LilyPond percussion staff {}\n\n",
		m_lilypond_percussion ? "enabled" : "disabled");

	std::cout << "Attempting to write LilyPond files...\n";

	auto lps{ coll.to_lilypond(m_lilypond_percussion) };

	for (std::size_t i{ 0 }; i < lps.size(); ++i) {
		std::string l_filename{ std::format("{}-{:02}.ly", p_out_file_prefix, i + 1) };
		klib::file::write_string_to_file(lps[i], l_filename);
		std::cout << "Wrote " << l_filename << "!\n";
	}
}

void fi::Cli::dump_config(const std::string& p_nes_filename,
	const std::string& p_dump_filename) {
	const auto rom_data{ load_rom_and_determine_region(p_nes_filename) };

	klib::file::write_string_to_file(m_config.to_string(), p_dump_filename);
	std::cout << "Wrote resolved configuration dump to " << p_dump_filename << "!\n";
}

void fi::Cli::parse_arguments(int arg_start, int argc, char** argv) {
	for (int i{ arg_start }; i < argc; ++i) {
		std::string argvi{ argv[i] };
		if (argvi == appc::CLI_SOURCE_ROM.first ||
			argvi == appc::CLI_SOURCE_ROM.second) {
			if (i + 1 >= argc)
				throw std::runtime_error("Source ROM option was set, but no source ROM file was specified");
			else
				m_source_rom = argv[++i];
		}
		else if (argvi == appc::CLI_REGION.first ||
			argvi == appc::CLI_REGION.second) {
			if (i + 1 >= argc)
				throw std::runtime_error("Region option was used, but no ROM region was specified");
			else
				m_region = argv[++i];
		}
		else
			set_flag(argvi);
	}
}

std::vector<byte> fi::Cli::load_rom_and_determine_region(
	const std::string& p_nes_filename) {

	std::cout << "Attempting to read " << p_nes_filename << "\n";
	const auto rom_data{ klib::file::read_file_as_bytes(p_nes_filename) };

	if (m_region.empty()) {
		m_config.determine_region(rom_data);
		std::cout << "ROM region resolved to '" << m_config.get_region() << "'\n";
	}
	else {
		m_config.set_region(m_region);
		std::cout << "ROM region specified as '" << m_region << "'\n";
	}

	m_config.load_config_data(appc::CONFIG_XML, appc::CONFIG_OVERRIDE_FILE_NAME, rom_data);

	return rom_data;
}

fm::MMLSongCollection fi::Cli::load_mml_file(const std::string& p_mml_file) const {
	std::cout << "Attempting to parse mml file " << p_mml_file << "\n";

	std::string mml_string;
	{
		const auto strs{ klib::file::read_file_as_strings(p_mml_file) };

		for (const auto& str : strs)
			mml_string += std::format("{}\n", klib::str::strip_comment(str));
	}

	fm::Tokenizer tokenizer(mml_string);
	const auto tokens{ tokenizer.tokenize() };

	fm::Parser parser(tokens);

	auto coll{ parser.parse() };
	coll.sort();

	return coll;
}

void fi::Cli::set_mode(const std::string& p_mode) {
	if (check_mode(p_mode, appc::CMD_BUILD)) {
		m_script_mode = fi::ScriptMode::IScriptBuild;
	}
	else if (check_mode(p_mode, appc::CMD_EXTRACT)) {
		m_script_mode = fi::ScriptMode::IScriptExtract;
	}
	else if (check_mode(p_mode, appc::CMD_BUILD_BSCRIPTS)) {
		m_script_mode = fi::ScriptMode::BScriptBuild;
	}
	else if (check_mode(p_mode, appc::CMD_EXTRACT_BSCRIPTS)) {
		m_script_mode = fi::ScriptMode::BScriptExtract;
	}
	else if (check_mode(p_mode, appc::CMD_BUILD_MUSIC)) {
		m_script_mode = fi::ScriptMode::MScriptBuild;
	}
	else if (check_mode(p_mode, appc::CMD_EXTRACT_MUSIC)) {
		m_script_mode = fi::ScriptMode::MScriptExtract;
	}
	else if (check_mode(p_mode, appc::CMD_BUILD_MML)) {
		m_script_mode = fi::ScriptMode::MmlBuild;
	}
	else if (check_mode(p_mode, appc::CMD_EXTRACT_MML)) {
		m_script_mode = fi::ScriptMode::MmlExtract;
	}
	else if (check_mode(p_mode, appc::CMD_MML_TO_MIDI)) {
		m_script_mode = fi::ScriptMode::MmlToMidi;
	}
	else if (check_mode(p_mode, appc::CMD_ROM_TO_MIDI)) {
		m_script_mode = fi::ScriptMode::RomToMidi;
	}
	else if (check_mode(p_mode, appc::CMD_MML_TO_LILYPOND)) {
		m_script_mode = fi::ScriptMode::MmlToLilyPond;
	}
	else if (check_mode(p_mode, appc::CMD_ROM_TO_LILYPOND)) {
		m_script_mode = fi::ScriptMode::RomToLilyPond;
	}
	else if (check_mode(p_mode, appc::CMD_BUILD_MISC)) {
		m_script_mode = fi::ScriptMode::MiscBuild;
	}
	else if (check_mode(p_mode, appc::CMD_EXTRACT_MISC)) {
		m_script_mode = fi::ScriptMode::MiscExtract;
	}
	else if (check_mode(p_mode, appc::CMD_DUMP_CONFIG)) {
		m_script_mode = fi::ScriptMode::DumpConfig;
	}
	else throw std::runtime_error("Unknown commad " + p_mode);
}

bool fi::Cli::check_mode(const std::string& p_mode,
	const std::pair<std::string, std::string>& p_cmds) {
	return (p_mode == p_cmds.first || p_mode == p_cmds.second);
}

std::vector<int> fi::Cli::get_global_transpose(const std::vector<byte>& p_rom) const {
	std::vector<int> result;

	auto transp{ m_config.constant(fm::c::ID_CHAN_PITCH_OFFSET) };

	for (std::size_t i{ 0 }; i < 4; ++i)
		result.push_back(static_cast<int>(static_cast<char>(p_rom.at(i + transp))));

	return result;
}

// TODO: Streamline flag lookup with const map
void fi::Cli::set_flag(const std::string& p_flag) {
	for (std::size_t i{ 0 }; i < appc::CLI_FLAGS.size(); ++i) {
		if (p_flag == appc::CLI_FLAGS[i].first || p_flag == appc::CLI_FLAGS[i].second) {
			toggle_flag(i);
			return;
		}
	}

	throw std::runtime_error("Unknown option " + p_flag);
}

void fi::Cli::toggle_flag(std::size_t p_flag_idx) {
	if (p_flag_idx == 0)
		m_shop_comments = !m_shop_comments;
	else if (p_flag_idx == 1)
		m_strict = !m_strict;
	else if (p_flag_idx == 2)
		m_overwrite = !m_overwrite;
	else if (p_flag_idx == 3)
		m_notes = !m_notes;
	else if (p_flag_idx == 4)
		m_lilypond_percussion = !m_lilypond_percussion;
}

void fi::Cli::clear_rom_section(std::vector<byte>& rom,
	std::size_t p_start, std::size_t p_end) const {
	for (std::size_t i{ p_start }; i < p_end; ++i)
		rom.at(i) = 0xff;
}

// sad that this is needed in 2026
void fi::Cli::output_oe_on_windows(void) const {

#ifdef _WIN32
	UINT old_cp = GetConsoleOutputCP();
	SetConsoleOutputCP(CP_UTF8);
#endif

	std::cout << "ø";

#ifdef _WIN32
	SetConsoleOutputCP(old_cp);
#endif
}
