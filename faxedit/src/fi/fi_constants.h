#ifndef FI_CONSTANTS_H
#define FI_CONSTANTS_H

#include <cstddef>
#include <map>
#include <set>
#include <string>

using byte = unsigned char;

namespace fi {

	namespace c {

		constexpr char ID_ISCRIPT_PTR_LO[]{ "iscript_ptr_lo" };
		constexpr char ID_ISCRIPT_COUNT[]{ "iscript_count" };

		constexpr char ID_STRING_DATA_START[]{ "string_data_start" };
		constexpr char ID_STRING_DATA_END[]{ "string_data_end" };
		constexpr char ID_STRING_CHAR_MAP[]{ "iscript_string_characters" };

		constexpr char ID_DEFINES_TEXTBOX[]{ "defines_textbox" };
		constexpr char ID_DEFINES_ITEM[]{ "defines_item" };
		constexpr char ID_DEFINES_QUEST[]{ "defines_quest" };
		constexpr char ID_DEFINES_RANK[]{ "defines_rank" };

		constexpr byte OPCODE_SET_SPAWN{ 0x06 };

	}

}

#endif
