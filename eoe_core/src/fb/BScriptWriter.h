#ifndef FB_BSCRIPT_WRITER_H
#define FB_BSCRIPT_WRITER_H

#include "BScriptLoader.h"
#include "fe/Config.h"
#include <map>
#include <string>

using byte = unsigned char;

namespace fb {

	class BScriptWriter {

		std::map<byte, std::string> sprite_labels;
		std::map<fb::ArgDomain, std::string> arg2str;
		std::map<fb::ArgDomain, std::map<byte, std::string>> defines;
		std::map<std::size_t, std::string> ram_defines;

		void add_defines(std::string& p_asm) const;
		void add_defines(std::string& p_asm, const std::string& p_type,
			fb::ArgDomain p_domain) const;

		void emit_operand(std::string& p_asm, std::size_t p_value,
			fb::ArgDomain domain, std::size_t p_ptr_table_idx) const;
		std::string get_label_name(std::size_t p_ptr_table_idx, std::size_t p_address) const;

		std::string operand_value(std::size_t p_value, fb::ArgDomain domain) const;

	public:
		BScriptWriter(const fe::Config& p_config);
		void write_asm(const std::string& p_filename,
			const fb::BScriptLoader& loader) const;

	};

}

#endif
