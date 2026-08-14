#ifndef FE_ATLASMOVIELAYOUT_H
#define FE_ATLASMOVIELAYOUT_H

#include <array>
#include <cstddef>
#include <cstdint>
#include "fh/fh_constants.h"

namespace fe::atlas_movie::layout {

	inline constexpr byte BANK{ 12 };
	inline constexpr std::size_t HEADER_BYTES{ 16 };
	inline constexpr std::size_t PRG_BANK_BYTES{ 0x4000 };
	inline constexpr std::uint16_t CORE_CPU{ 0xa708 };
	inline constexpr std::uint16_t CORE_LIMIT{ 0xaa83 };
	inline constexpr std::uint16_t HANDLER_CPU{ 0xad8b };
	inline constexpr std::uint16_t TAIL_CPU{ 0xad91 };
	inline constexpr std::uint16_t BUNDLE_CPU{ 0xb264 };
	inline constexpr std::uint16_t DISPATCH_HIGH_REF{
		fh::ROM::IScripts_JumpTable_Ref_U };
	inline constexpr std::uint16_t DISPATCH_LOW_REF{
		fh::ROM::IScripts_JumpTable_Ref_L };
	inline constexpr std::size_t CORE_BYTES{ 782 };
	inline constexpr std::size_t TAIL_BYTES{ 1235 };
	inline constexpr std::size_t DISPATCH_ENTRIES{ 25 };
	inline constexpr std::size_t DISPATCH_BYTES{ DISPATCH_ENTRIES * 2 };

	inline constexpr std::array<std::uint16_t, DISPATCH_ENTRIES> HANDLERS{
		0x82b3, 0x82c4, 0x82ee, 0x82d8, 0x8725, 0x835a,
		0x8390, 0x839e, 0x83d7, 0x8524, 0x857f, 0x85d0,
		0x85f5, 0x861e, 0x85e5, 0x862f, 0x8656, 0x865f,
		0x8717, 0x85ae, 0x8736, 0x82ad, 0x8307, 0x82aa,
		static_cast<std::uint16_t>(HANDLER_CPU - 1)
	};

	std::size_t file_offset(byte p_bank, std::uint16_t p_cpu);

}

#endif
