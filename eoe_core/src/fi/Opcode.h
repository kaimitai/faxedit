#ifndef FI_OPCODE_H
#define FI_OPCODE_H

#include <cstdint>
#include <initializer_list>
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

	struct Argument {
		ArgType type;
		ArgDomain domain;

		bool operator==(const Argument&) const = default;
	};

	struct Opcode {
		std::string name;
		fi::Flow flow;
		bool ends_stream;
		std::vector<Argument> args;

		Opcode(const std::string& name, std::initializer_list<Argument> args,
			Flow flow, bool ends_stream);
		bool operator==(const Opcode&) const = default;
		std::size_t token_count(void) const;
		std::size_t size(void) const;
	};

	enum Instruction_type { OpCode, Directive };

	struct Instruction {
		Instruction_type type;
		byte opcode_byte;
		std::size_t size;
		std::optional<std::size_t> jump_target;
		std::optional<std::size_t> byte_offset;
		std::vector<uint16_t> operands;
		std::optional<std::size_t> shop_index;

		std::vector<byte> get_bytes(const std::map<byte, fi::Opcode>& p_opcodes) const;
	};

	struct ScriptOpcodeInfo {
		std::map<byte, fi::Opcode> opcodes;
		std::vector<std::string> required_impls;
		std::size_t base_opcode_count{ 0 };
	};

	ScriptOpcodeInfo load_iscript_opcodes_from_config(
		const std::map<byte, std::string>& p_opcode_defs,
		const std::map<std::string, std::string>& p_impl_defs);

	ScriptOpcodeInfo load_vanilla_opcodes(void);

}

#endif
