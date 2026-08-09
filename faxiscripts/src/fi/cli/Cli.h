#ifndef FI_CLI_H
#define FI_CLI_H

#include <vector>
#include <string>
#include "fe/Config.h"
#include "fm/song/MMLSongCollection.h"

namespace fi {

	enum ScriptMode {
		IScriptBuild, IScriptExtract,
		MmlExtract, MmlBuild, MmlToMidi, RomToMidi,
		MmlToLilyPond, RomToLilyPond,
		MScriptBuild, MScriptExtract,
		BScriptBuild, BScriptExtract,
		MiscBuild, MiscExtract,
		DumpConfig
	};

	class Cli {

		fi::ScriptMode m_script_mode;

		std::string m_in_file, m_out_file, m_source_rom, m_region;
		bool m_strict, m_shop_comments, m_overwrite, m_notes,
			m_lilypond_percussion;
		fe::Config m_config;

		void set_mode(const std::string& p_mode);
		void toggle_flag(std::size_t p_flag_idx);
		void set_flag(const std::string& p_flag);
		void print_header(void) const;
		void print_help(void) const;
		void parse_arguments(int arg_start, int argc, char** argv);

		void output_oe_on_windows(void) const;

		// main logic
		void asm_to_nes(const std::string& p_asm_filename,
			const std::string& p_nes_filename,
			const std::string& p_source_rom_filename,
			bool p_strict);
		void nes_to_asm(const std::string& p_nes_filename,
			const std::string& p_asm_filename,
			bool p_shop_comments, bool p_overwrite);

		// bscripts
		void basm_to_nes(const std::string& p_basm_filename,
			const std::string& p_nes_filename,
			const std::string& p_source_rom_filename,
			bool p_strict);
		void nes_to_basm(const std::string& p_nes_filename,
			const std::string& p_basm_filename, bool p_overwrite);

		// music (asm)
		void masm_to_nes(const std::string& p_mml_filename,
			const std::string& p_nes_filename,
			const std::string& p_source_rom_filename);
		void nes_to_masm(const std::string& p_nes_filename,
			const std::string& p_mml_filename,
			bool p_overwrite);

		// music (mml)
		void mml_to_nes(const std::string& p_mml_filename,
			const std::string& p_nes_filename,
			const std::string& p_source_rom_filename);
		void nes_to_mml(const std::string& p_nes_filename,
			const std::string& p_mml_filename,
			bool p_overwrite);

		// to-midi
		void rom_to_midi(const std::string& p_nes_filename,
			const std::string& p_out_file_prefix);
		void mml_to_midi(const std::string& p_mml_filename,
			const std::string& p_out_file_prefix);

		// to-lilypond
		void rom_to_lilypond(const std::string& p_nes_filename,
			const std::string& p_out_file_prefix);
		void mml_to_lilypond(const std::string& p_mml_filename,
			const std::string& p_out_file_prefix);

		// miscellaneous data
		void misc_to_nes(const std::string& p_asm_filename,
			const std::string& p_nes_filename,
			const std::string& p_source_rom_filename);
		void nes_to_misc(const std::string& p_nes_filename,
			const std::string& p_txt_filename,
			bool p_overwrite);

		// debug
		void dump_config(const std::string& p_nes_filename,
			const std::string& p_dump_filename);

		// common
		std::vector<byte> load_rom_and_determine_region(const std::string& p_nes_filename);
		fm::MMLSongCollection load_mml_file(const std::string& p_mml_file) const;
		void save_midi_files(fm::MMLSongCollection& coll,
			const std::string& p_out_file_prefix) const;
		void save_lilypond_files(fm::MMLSongCollection& coll,
			const std::string& p_out_file_prefix) const;
		bool check_mode(const std::string& p_mode,
			const std::pair<std::string, std::string>& p_cmds);
		std::vector<int> get_global_transpose(const std::vector<byte>& p_rom) const;
		void clear_rom_section(std::vector<byte>& rom, std::size_t p_start, std::size_t p_end) const;

	public:
		Cli(int argc, char** argv);
	};

}

#endif
