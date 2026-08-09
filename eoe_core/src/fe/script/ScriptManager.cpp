#include <format>
#include "ScriptManager.h"
#include "fi/IScriptLoader.h"
#include "fi/AsmWriter.h"
#include "common/klib/Kfile.h"

void fe::script::message(const MessageCallback& p_callback, const std::string& p_message) {
	if (p_callback)
		p_callback(p_message);
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
