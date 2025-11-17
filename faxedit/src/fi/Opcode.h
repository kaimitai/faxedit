#ifndef FI_OPCODE_H
#define FI_OPCODE_H

#include <map>
#include <optional>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fi {

	enum class ArgType {
		None,
		Byte,
		Short
	};

	enum class Flow {
		Continue, Jump, Read, End
	};

	enum class ArgDomain {
		None,
		Item,
		Quest,
		Rank,
		TextBox,
		TextString
	};

	struct Opcode {
		std::string name;
		fi::ArgType arg_type;
		fi::Flow flow;
		fi::ArgDomain domain;
		bool ends_stream;

		std::size_t size(void) const {
			std::size_t result{ 1 }; // the opcode itself
			if (flow == fi::Flow::Jump || flow == fi::Flow::Read)
				result += 2;
			if (arg_type == fi::ArgType::Short)
				result += 2;
			else if (arg_type == fi::ArgType::Byte)
				++result;
			return result;
		}
	};

	extern const std::map<byte, fi::Opcode> opcodes;

	enum Instruction_type { OpCode, Directive };

	struct Instruction {
		Instruction_type type;
		byte opcode_byte;
		std::size_t size;
		std::optional<uint16_t> operand;
		std::optional<std::size_t> jump_target;
		std::optional<std::size_t> byte_offset;

		std::vector<byte> get_bytes(void) const;
	};

}

#endif
