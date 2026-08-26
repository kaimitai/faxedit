#ifndef FE_NES_CONSTANTS_H
#define FE_NES_CONSTANTS_H

#include <cstddef>

using byte = unsigned char;

namespace fe {

	namespace nc {

		constexpr std::size_t INES_HEADER_SIZE{ 0x10 };
		constexpr std::size_t PRG_BANK_SIZE{ 0x4000 };
		constexpr byte VANILLA_BANK_COUNT{ 0x10 };
		constexpr byte EXPANDED_BANK_COUNT{ 0x20 };

		constexpr std::size_t VANILLA_ROM_SIZE{ INES_HEADER_SIZE + VANILLA_BANK_COUNT * PRG_BANK_SIZE };
		constexpr std::size_t EXPANDED_ROM_SIZE{ INES_HEADER_SIZE + EXPANDED_BANK_COUNT * PRG_BANK_SIZE };
	}

}

#endif

