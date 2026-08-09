#ifndef FI_ASM_READER_H
#define FI_ASM_READER_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include "FaxString.h"
#include "Shop.h"
#include "Opcode.h"
#include "fe/Config.h"
#include "fh/TilemapChanges.h"

namespace fi {

	enum class SectionType { Defines, Strings, Shops, IScript, TilemapChanges };

	class AsmReader {

		// set of reserved string indexes first,
		// then becomes full set of strings during parsing
		std::map<int, fi::FaxString> m_strings;
		std::map<std::string, std::size_t> m_defines;
		std::map<std::size_t, fi::Shop> m_shops;
		std::vector<fi::Instruction> m_instructions;
		// map from entrypoint no to offset
		std::map<std::size_t, std::size_t> m_ptr_table;

		std::map<SectionType, std::vector<std::string>> m_sections;

		fh::TilemapChanges m_tilemap_changes;

		std::string strip_comment(const std::string& line) const;
		std::string trim(const std::string& line) const;

		int parse_numeric(const std::string& token) const;

		void parse_section_strings(void);
		void parse_section_defines(void);
		void parse_section_shops(void);
		void parse_section_tilemap_changes(void);
		void parse_section_iscript(const fe::Config& p_config, std::size_t script_rg2_offset);

		std::map<std::string, int> relocate_strings(const std::set<std::string>& p_strings);

		std::size_t resolve_token(const std::string& token) const;

		bool is_string_token(const std::string& p_token) const;
		bool contains_label(const std::string& p_line) const;
		std::string extract_label(const std::string& p_line) const;

		bool contains_entrypoint(const std::string& p_line) const;
		std::size_t extract_entrypoint(const std::string& p_line) const;

		bool contains_textbox(const std::string& p_line) const;
		byte extract_textbox(const std::string& p_line) const;
		bool str_begins_with(const std::string& p_line, const std::string& p_start) const;
		std::vector<std::string> split_whitespace(const std::string& p_line) const;

		std::string to_lower(const std::string& str);

	public:
		AsmReader(void) = default;
		void read_asm_file(const fe::Config& p_config,
			const std::string& p_filename, std::size_t script_rg2_offset);
		std::size_t get_entrypoint_count(void) const;

		// get ROM bytes
		std::pair<std::vector<byte>, std::vector<byte>> get_script_bytes(const fe::Config& p_config) const;
		std::vector<byte> get_string_bytes(const fe::Config& p_config) const;
		std::size_t get_string_count(void) const;

		// get optional tilemap changes
		const fh::TilemapChanges& get_tilemap_changes() const;
	};

}

#endif
