#ifndef FI_ISCRIPTLOADER_H
#define FI_ISCRIPTLOADER_H

#include <vector>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include "Opcode.h"
#include "FaxString.h"
#include "Shop.h"

using byte = unsigned char;

namespace fi {

	struct AsmToken {
		std::string text;
		byte color_idx;
		bool newline;
	};

	class IScriptLoader {
	public:
		IScriptLoader(const std::vector<byte>& p_rom);

		// TODO change visibility
	public:
		std::vector<std::size_t> m_ptr_table;

		std::vector<fi::AsmToken> get_asm_code(std::size_t p_script_no);
		std::string get_define(const std::map<byte, std::string>& p_map, byte arg) const;
		std::string get_define(fi::ArgDomain domain, byte arg) const;
		std::string serialize_shop_as_string(const fi::Shop& p_shop) const;

		std::map<std::size_t, fi::Instruction> m_instructions;
		std::vector<fi::Shop> m_shops;
		std::vector<fi::FaxString> m_strings;

		std::set<std::size_t> m_jump_targets;
		// map from ROM address -> shop index
		// we parse shops when we see them and assign an index if it is unique
		std::map<std::size_t, std::size_t> m_shop_addresses;

		// map from spawn point to script no where the spawn is set
		// we need this later for spawn point deduction
		std::map<byte, byte> m_spawn_scripts;

		std::vector<fi::AsmToken> parse_script(const std::vector<byte>& p_rom, std::size_t p_script_no);
		void parse_strings(const std::vector<byte>& p_rom);
		void parse_blob_from_entrypoint(const std::vector<byte>& p_rom, 
			size_t offset, bool at_entrypoint);

		byte read_byte(const std::vector<byte>& p_rom, std::size_t& offset) const;
		uint16_t read_short(const std::vector<byte>& p_rom, std::size_t& offset) const;
	};

}

#endif
