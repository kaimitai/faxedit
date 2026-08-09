#ifndef FB_CONSTANTS_H
#define FB_CONSTANTS_H

#include <map>
#include <optional>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fb {

	enum class ArgDataType { Byte, Word };
	enum class ArgDomain {
		Zero, Byte, SignedByte, Addr, Direction, HopMode, Action,
		RAM, TrueAddr, FalseAddr, Ticks, Pixels, Blocks,
		PixelsY, BlocksY, Phase
	};
	enum class Flow { Continue, Jump, End };

	struct BScriptArg {
		ArgDataType data_type;
		ArgDomain domain;
	};

	struct ArgInstance {
		ArgDataType data_type;
		std::optional<std::size_t> data_value;
	};

	namespace c {
		constexpr byte OPCODE_BEHAVIOR{ 0x00 };

		constexpr char ID_BSCRIPT_PTR[]{ "bscript_ptr" };
		constexpr char ID_BSCRIPT_RG1_END[]{ "bscript_data_rg1_end" };
		constexpr char ID_BSCRIPT_RG2_START[]{ "bscript_data_rg2_start" };
		constexpr char ID_BSCRIPT_RG2_END[]{ "bscript_data_rg2_end" };
		constexpr char ID_SPRITE_COUNT[]{ "sprite_count" };
		constexpr char ID_BSCRIPT_OPCODES[]{ "bscript_opcodes" };
		constexpr char ID_BSCRIPT_BEHAVIORS[]{ "bscript_behaviors" };
		constexpr char ID_SPRITE_LABELS[]{ "sprite_labels" };

		constexpr char ID_BSCRIPT_DEF_ACTIONS[]{ "bscript_defines_actions" };
		constexpr char ID_BSCRIPT_DEF_HOPMODES[]{ "bscript_defines_hopmodes" };
		constexpr char ID_BSCRIPT_DEF_DIRECTIONS[]{ "bscript_defines_directions" };
		constexpr char ID_BSCRIPT_DEF_RAM[]{ "bscript_defines_ram" };

		constexpr char SECTION_DEFINES[]{ "[defines]" };
		constexpr char SECTION_BSCRIPT[]{ "[bscript]" };
		constexpr char DIRECTIVE_ENTRYPOINT[]{ ".entrypoint" };

		constexpr char XML_TYPE_MNEMONIC[]{ "mnemonic" };
		constexpr char XML_TYPE_ARG[]{ "arg" };
		constexpr char XML_TYPE_FLOW[]{ "flow" };

		inline std::map<std::string, fb::ArgDomain> STR_ARGDOMAIN{
			// byte operands
			{"zero", fb::ArgDomain::Zero},
			{"byte", fb::ArgDomain::Byte},
			{"value", fb::ArgDomain::SignedByte},
			{"direction", fb::ArgDomain::Direction},
			{"mode", fb::ArgDomain::HopMode},
			{"action", fb::ArgDomain::Action},
			{"ticks", fb::ArgDomain::Ticks},
			{"pixels", fb::ArgDomain::Pixels},
			{"blocks", fb::ArgDomain::Blocks},
			{"pixels_y", fb::ArgDomain::PixelsY},
			{"blocks_y", fb::ArgDomain::BlocksY},
			{"phase", fb::ArgDomain::Phase},
			// word operands
			{"addr", fb::ArgDomain::Addr},
			{"ram", fb::ArgDomain::RAM},
			{"true", fb::ArgDomain::TrueAddr},
			{"false", fb::ArgDomain::FalseAddr}
		};

		inline std::map<std::string, fb::Flow> STR_FLOW{
			{"continue", fb::Flow::Continue},
			{"jump", fb::Flow::Jump},
			{"end", fb::Flow::End}
		};
	}

}

#endif
