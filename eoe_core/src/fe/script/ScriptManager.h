#ifndef FE_SCRIPTMANAGER_H
#define FE_SCRIPTMANAGER_H

#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include "ScriptTypes.h"
#include "fe/MessageCallback.h"
#include "fe/Config.h"
#include "fi/Opcode.h"
#include "fm/song/MMLSongCollection.h"
#include "common/midifile/MidiFile.h"

using byte = unsigned char;

namespace fe::script {

	// analyzers
	std::unordered_map<byte, ScriptSemanticInfo> extract_script_semantics(
		const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::map<byte, fi::Opcode>& p_opcodes);
	std::unordered_map<byte, ScriptSemanticInfo> extract_script_semantics(
		const fe::Config& p_config, const std::vector<byte>& p_rom);

	std::map<byte, byte> extract_set_spawn_scripts(
		const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::map<byte, fi::Opcode>& p_opcodes);
	std::map<byte, byte> extract_set_spawn_scripts(
		const fe::Config& p_config, const std::vector<byte>& p_rom);

	// helpers
	void try_patch(const std::string& p_data_type, std::size_t p_data_size,
		std::size_t p_data_max_size, const MessageCallback& p_message);
	std::vector<int> get_global_transpose(const Config& p_config, const std::vector<byte>& p_rom);
	fi::ScriptOpcodeInfo get_iscript_opcode_info(const Config& p_config);
	void validate_iscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::map<byte, fi::Opcode>& p_opcodes);

	// iScripts
	std::vector<byte> asm_iscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_asm, const fi::ScriptOpcodeInfo& p_opcode_info,
		bool p_strict, const MessageCallback& p_message = nullptr);
	void asm_iscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm_filename, const std::string& p_out_filename,
		const fi::ScriptOpcodeInfo& p_opcode_info, bool p_strict,
		const MessageCallback& p_message = nullptr);

	std::string disasm_iscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::map<byte, fi::Opcode>& p_opcodes, bool p_shop_comments, std::size_t& p_entrypoint_count);
	void disasm_iscripts_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::map<byte, fi::Opcode>& p_opcodes, const std::string& p_filename,
		bool p_shop_comments, bool p_overwrite,
		const MessageCallback& p_message = nullptr);

	// bScripts
	std::vector<byte> asm_bscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_asm, bool p_strict, const MessageCallback& p_message = nullptr);
	void asm_bscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_basm_filename, const std::string& p_out_filename, bool p_strict,
		const MessageCallback& p_message = nullptr);

	std::string disasm_bscripts(const Config& p_config, const std::vector<byte>& p_rom);
	void disasm_bscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_overwrite, const MessageCallback& p_message = nullptr);

	// mScripts
	std::vector<byte> asm_mscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_asm, const MessageCallback& p_message = nullptr);
	void asm_mscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm_filename, const std::string& p_out_filename,
		const MessageCallback& p_message = nullptr);

	std::string disasm_mscripts(const Config& p_config, const std::vector<byte>& p_rom, bool p_emit_notes,
		const MessageCallback& p_message = nullptr);
	void disasm_mscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_emit_notes, bool p_overwrite,
		const MessageCallback& p_message = nullptr);

	// misc interface
	std::vector<byte> build_misc(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_txt, const MessageCallback& p_message = nullptr);
	void build_misc_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_txt_filename, const std::string& p_out_filename,
		const MessageCallback& p_message = nullptr);

	std::string extract_misc(const Config& p_config, const std::vector<byte>& p_rom, bool p_include_all_sprites);
	void extract_misc_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_include_all_sprites, bool p_overwrite,
		const MessageCallback& p_message = nullptr);

	// MML interface
	std::string decompile_mml(const Config& p_config, const std::vector<byte>& p_rom);
	void decompile_mml_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_overwrite, const MessageCallback& p_message = nullptr);

	fm::MMLSongCollection parse_mml(const std::vector<std::string>& p_mml);
	std::vector<byte> compile_mml(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_mml, const MessageCallback& p_message = nullptr);
	void compile_mml_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_mml_filename, const std::string& p_out_filename, const MessageCallback& p_message = nullptr);

	// midi
	std::vector<smf::MidiFile> mml_to_midi(const std::vector<std::string>& p_mml);
	std::vector<smf::MidiFile> rom_to_midi(const Config& p_config, const std::vector<byte>& p_rom);

	void write_midi_files(std::vector<smf::MidiFile>& p_midis, const std::string& p_out_file_prefix,
		const MessageCallback& p_message = nullptr);
	void mml_to_midi_files(const std::string& p_mml_filename, const std::string& p_out_file_prefix,
		const MessageCallback& p_message = nullptr);
	void rom_to_midi_files(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_out_file_prefix, const MessageCallback& p_message = nullptr);

	// lilypond
	std::vector<std::string> mml_to_lilypond(const std::vector<std::string>& p_mml, bool p_percussion);
	std::vector<std::string> rom_to_lilypond(const Config& p_config, const std::vector<byte>& p_rom,
		bool p_percussion);

	void write_lilypond_files(const std::vector<std::string>& p_lilypond, const std::string& p_out_file_prefix,
		const MessageCallback& p_message = nullptr);
	void mml_to_lilypond_files(const std::string& p_mml_filename, const std::string& p_out_file_prefix,
		bool p_percussion, const MessageCallback& p_message = nullptr);
	void rom_to_lilypond_files(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_out_file_prefix, bool p_percussion, const MessageCallback& p_message = nullptr);
}

#endif
