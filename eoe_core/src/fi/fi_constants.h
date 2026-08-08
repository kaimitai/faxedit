#ifndef FI_CONSTANTS_H
#define FI_CONSTANTS_H

#include <cstddef>
#include <map>
#include <set>
#include <string>

using byte = unsigned char;

namespace fi {

	namespace c {

		constexpr char ID_ISCRIPT_OPCODES[]{ "iscript_opcodes" };
		constexpr char ID_ISCRIPT_OPCODE_IMPLS[]{ "iscript_opcode_impls" };
		constexpr char ID_ISCRIPT_PTR_LO[]{ "iscript_ptr_lo" };
		constexpr char ID_ISCRIPT_HI_REF_OFFSET[]{ "iscript_ptr_hi_ref_offset" };
		constexpr char ID_ISCRIPT_MIN_COUNT[]{ "iscript_min_count" };

		constexpr char ID_ISCRIPT_RG1_END[]{ "iscript_data_rg1_end" };
		constexpr char ID_ISCRIPT_RG2_START[]{ "iscript_data_rg2_start" };
		constexpr char ID_ISCRIPT_RG2_END[]{ "iscript_data_rg2_end" };

		constexpr char ID_STRING_DATA_START[]{ "string_data_start" };
		constexpr char ID_STRING_DATA_END[]{ "string_data_end" };
		constexpr char ID_STRING_CHAR_MAP[]{ "iscript_string_characters" };
		constexpr char ID_STRING_RESERVED[]{ "reserved_script_string_indexes" };

		constexpr char ID_DEFINES_TEXTBOX[]{ "defines_textbox" };
		constexpr char ID_DEFINES_ITEM[]{ "defines_item" };
		constexpr char ID_DEFINES_QUEST[]{ "defines_quest" };
		constexpr char ID_DEFINES_RANK[]{ "defines_rank" };

		constexpr char SECTION_DEFINES[]{ "[defines]" };
		constexpr char SECTION_STRINGS[]{ "[reserved_strings]" };
		constexpr char SECTION_SHOPS[]{ "[shops]" };
		constexpr char SECTION_ISCRIPT[]{ "[iscript]" };
		constexpr char SECTION_TILEMAP_CHANGES[]{ "[tilemap_changes]" };
		constexpr char DIRECTIVE_ENTRYPOINT[]{ ".entrypoint" };
		constexpr char PSEUDO_OPCODE_TEXTBOX[]{ ".textbox" };

		constexpr byte OPCODE_SET_SPAWN{ 0x06 };
		constexpr byte OPCODE_GET_ITEM{ 0x07 };
		constexpr byte OPCODE_SHOP_BUY{ 0x08 };
		constexpr byte OPCODE_SHOP_SELL{ 0x11 };

	}

}

#endif
