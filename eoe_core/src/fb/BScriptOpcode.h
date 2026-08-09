#ifndef FB_BSCRIPT_OPCODE_H
#define FB_BSCRIPT_OPCODE_H

#include "fb_constants.h"
#include <map>
#include <optional>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fb {

	struct BScriptOpcode {
		std::string mnemonic;
		fb::Flow flow;
		std::vector<BScriptArg> args;

		BScriptOpcode(const std::string& p_str);
	};

	struct BScriptInstruction {
		byte opcode_byte;
		std::optional<byte> behavior_byte;
		std::vector<ArgInstance> operands;
		std::optional<std::size_t> byte_offset;

		fb::Flow flow(const std::map<byte, fb::BScriptOpcode>& p_opcodes,
			const std::map<byte, fb::BScriptOpcode>& p_behavior_ops) const;
		std::size_t size(void) const;
		std::vector<byte> get_bytes(void) const;
	};

	std::map<byte, BScriptOpcode> parse_opcodes(const std::map<byte,
		std::string>& p_map);

}

#endif
