#include "Cli.h"
#include <format>
#include <iostream>
#include <stdexcept>
#include "fi/cli/application_constants.h"
#include "fi/fi_constants.h"
#include "common/klib/Kfile.h"
#include "fe/script/ScriptManager.h"

#ifdef _WIN32
#include <windows.h>
#endif

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

void fi::Cli::asm_to_nes(const std::string& p_asm_filename,
	const std::string& p_out_filename,
	const std::string& p_source_rom_filename,
	bool p_strict) {

	const auto rom{ load_rom_and_config(p_source_rom_filename) };
	const auto opcode_defs{ fe::script::get_iscript_opcode_info(m_config) };

	fe::script::asm_iscripts_to_file(m_config, rom, p_asm_filename, p_out_filename, opcode_defs, p_strict,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::basm_to_nes(const std::string& p_basm_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename,
	bool p_strict) {

	const auto rom{ load_rom_and_config(p_source_rom_filename) };
	fe::script::asm_bscripts_to_file(m_config, rom, p_basm_filename, p_nes_filename, p_strict,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});

}

void fi::Cli::masm_to_nes(const std::string& p_mml_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename) {

	const auto rom{ load_rom_and_config(p_source_rom_filename) };
	fe::script::asm_mscripts_to_file(m_config, rom, p_mml_filename, p_nes_filename,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::misc_to_nes(const std::string& p_txt_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename) {

	const auto rom{ load_rom_and_config(p_source_rom_filename) };

	fe::script::build_misc_to_file(m_config, rom, p_txt_filename, p_nes_filename,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_asm(const std::string& p_nes_filename,
	const std::string& p_asm_filename, bool p_shop_comments, bool p_overwrite) {

	// show params
	if (p_shop_comments)
		std::cout << "Will show shop data as comments where they are referenced\n";
	if (p_overwrite)
		std::cout << "Will overwrite output assembly file if it already exists\n";

	const auto rom_data{ load_rom_and_config(p_nes_filename) };
	const auto opcode_info{ fe::script::get_iscript_opcode_info(m_config) };

	fe::script::disasm_iscripts_to_file(m_config, rom_data, opcode_info.opcodes,
		p_asm_filename, p_shop_comments, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_basm(const std::string& p_nes_filename,
	const std::string& p_basm_filename, bool p_overwrite) {

	// show params
	if (p_overwrite)
		std::cout << "Will overwrite output assembly file if it already exists\n";

	const auto rom_data{ load_rom_and_config(p_nes_filename) };
	fe::script::disasm_bscripts_to_file(m_config, rom_data, p_basm_filename, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_masm(const std::string& p_nes_filename,
	const std::string& p_mml_filename, bool p_overwrite) {

	const auto rom_data{ load_rom_and_config(p_nes_filename) };

	fe::script::disasm_mscripts_to_file(m_config, rom_data, p_mml_filename, m_notes, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_misc(const std::string& p_nes_filename,
	const std::string& p_txt_filename,
	bool p_overwrite) {
	const auto rom_data{ load_rom_and_config(p_nes_filename) };

	fe::script::extract_misc_to_file(m_config, rom_data, p_txt_filename, m_strict, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::nes_to_mml(const std::string& p_nes_filename,
	const std::string& p_mml_filename,
	bool p_overwrite) {

	const auto rom_data{ load_rom_and_config(p_nes_filename) };

	fe::script::decompile_mml_to_file(m_config, rom_data, p_mml_filename, p_overwrite,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::mml_to_nes(const std::string& p_mml_filename,
	const std::string& p_nes_filename,
	const std::string& p_source_rom_filename) {

	const auto rom{ load_rom_and_config(p_source_rom_filename) };

	fe::script::compile_mml_to_file(m_config, rom, p_mml_filename, p_nes_filename,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::rom_to_midi(const std::string& p_nes_filename,
	const std::string& p_out_file_prefix) {

	const auto rom_data{ load_rom_and_config(p_nes_filename) };

	fe::script::rom_to_midi_files(m_config, rom_data, p_out_file_prefix,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::mml_to_midi(const std::string& p_mml_filename,
	const std::string& p_out_file_prefix) {
	fe::script::mml_to_midi_files(
		p_mml_filename,
		p_out_file_prefix,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::rom_to_lilypond(const std::string& p_nes_filename,
	const std::string& p_out_file_prefix) {
	const auto rom_data{ load_rom_and_config(p_nes_filename) };

	fe::script::rom_to_lilypond_files(m_config, rom_data, p_out_file_prefix, m_lilypond_percussion,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::mml_to_lilypond(const std::string& p_mml_filename,
	const std::string& p_out_file_prefix) {
	fe::script::mml_to_lilypond_files(p_mml_filename, p_out_file_prefix, m_lilypond_percussion,
		[](const std::string& p_message) {
			std::cout << p_message << '\n';
		});
}

void fi::Cli::dump_config(const std::string& p_nes_filename,
	const std::string& p_dump_filename) {
	load_rom_and_config(p_nes_filename);

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

std::vector<byte> fi::Cli::load_rom_and_config(
	const std::string& p_nes_filename) {

	std::cout << "Attempting to read " << p_nes_filename << "\n";
	const auto rom_data{ klib::file::read_file_as_bytes(p_nes_filename) };

	m_config = fe::Config(
		appc::CONFIG_XML,
		appc::CONFIG_OVERRIDE_FILE_NAME,
		rom_data,
		m_region
	);

	if (m_region.empty())
		std::cout << "ROM region resolved to '" << m_config.get_region() << "'\n";
	else
		std::cout << "ROM region specified as '" << m_region << "'\n";

	return rom_data;
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
	else throw std::runtime_error("Unknown command " + p_mode);
}

bool fi::Cli::check_mode(const std::string& p_mode,
	const std::pair<std::string, std::string>& p_cmds) {
	return (p_mode == p_cmds.first || p_mode == p_cmds.second);
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
