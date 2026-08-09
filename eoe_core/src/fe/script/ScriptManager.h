#ifndef FE_SCRIPTMANAGER_H
#define FE_SCRIPTMANAGER_H

#include <functional>
#include <string>
#include "fe/Config.h"
#include "fi/Opcode.h"

using byte = unsigned char;

namespace fe::script {

	using MessageCallback = std::function<void(const std::string&)>;

	void message(const MessageCallback& p_callback, const std::string& p_message);
	void try_patch(const std::string& p_data_type, std::size_t p_data_size,
		std::size_t p_data_max_size, const MessageCallback& p_message);

	// iScripts
	std::vector<byte> asm_iscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_asm, const fi::ScriptOpcodeInfo& p_opcode_info,
		bool p_strict, const MessageCallback& p_message);
	void asm_iscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm_filename, const std::string& p_out_filename,
		const fi::ScriptOpcodeInfo& p_opcode_info, bool p_strict,
		const MessageCallback& p_message);

	std::string disasm_iscripts(const Config& p_config, const std::vector<byte>& p_rom,
		bool p_shop_comments, std::size_t& p_entrypoint_count);
	void disasm_iscripts_to_file(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_shop_comments, bool p_overwrite,
		const MessageCallback& p_message);

	// bScripts
	std::vector<byte> asm_bscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_asm, bool p_strict, const MessageCallback& p_message);
	void asm_bscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_basm_filename, const std::string& p_out_filename, bool p_strict,
		const MessageCallback& p_message);

	std::string disasm_bscripts(const Config& p_config, const std::vector<byte>& p_rom);
	void disasm_bscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_overwrite, const MessageCallback& p_message);

	// mScripts
	std::vector<byte> asm_mscripts(const Config& p_config, const std::vector<byte>& p_rom,
		const std::vector<std::string>& p_asm, const MessageCallback& p_message);
	void asm_mscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm_filename, const std::string& p_out_filename,
		const MessageCallback& p_message);

	std::string disasm_mscripts(const Config& p_config, const std::vector<byte>& p_rom, bool p_emit_notes,
		const MessageCallback& p_message);
	void disasm_mscripts_to_file(const Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_filename, bool p_emit_notes, bool p_overwrite,
		const MessageCallback& p_message);

}

#endif
