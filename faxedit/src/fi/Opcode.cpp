#include "Opcode.h"
#include "./../common/klib/Kstring.h"

std::map<byte, fi::Opcode> fi::opcodes{
	{0x00, fi::Opcode("End", fi::ArgType::None, fi::Flow::End, fi::ArgDomain::None, true)},
	{0x01, fi::Opcode("MsgNoskip", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::TextString, false)},
	{0x02, fi::Opcode("MsgPrompt", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::TextString, false)},
	{0x03, fi::Opcode("Msg", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::TextString, false)},
	{0x04, fi::Opcode("IfTitleChange", fi::ArgType::None, fi::Flow::Jump, fi::ArgDomain::None, false)},
	{0x05, fi::Opcode("LoseGold", fi::ArgType::Short, fi::Flow::Continue, fi::ArgDomain::None, false)},
	{0x06, fi::Opcode("SetSpawn", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::None, false)},
	{0x07, fi::Opcode("GetItem", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::Item, false)},
	{0x08, fi::Opcode("OpenShopBuy", fi::ArgType::None, fi::Flow::Read, fi::ArgDomain::None, false)},
	{0x09, fi::Opcode("GetGold", fi::ArgType::Short, fi::Flow::Continue, fi::ArgDomain::None, false)},
	{0x0a, fi::Opcode("GetMana", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::None, false)},
	{0x0b, fi::Opcode("IfQuest", fi::ArgType::Byte, fi::Flow::Jump, fi::ArgDomain::Quest, false)},
	{0x0c, fi::Opcode("IfRank", fi::ArgType::Byte, fi::Flow::Jump, fi::ArgDomain::Rank, false)},
	{0x0d, fi::Opcode("IfGold", fi::ArgType::None, fi::Flow::Jump, fi::ArgDomain::None, false)},
	{0x0e, fi::Opcode("SetQuest", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::Quest, false)},
	{0x0f, fi::Opcode("IfBuy", fi::ArgType::None, fi::Flow::Jump, fi::ArgDomain::None, false)},
	{0x10, fi::Opcode("LoseItem", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::Item, false)},
	{0x11, fi::Opcode("OpenShopSell", fi::ArgType::None, fi::Flow::Read, fi::ArgDomain::None, false)},
	{0x12, fi::Opcode("IfItem", fi::ArgType::Byte, fi::Flow::Jump, fi::ArgDomain::Item, false)},
	{0x13, fi::Opcode("GetHealth", fi::ArgType::Byte, fi::Flow::Continue, fi::ArgDomain::None, false)},
	{0x14, fi::Opcode("ShowMantra", fi::ArgType::None, fi::Flow::Continue, fi::ArgDomain::None, false)},
	{0x15, fi::Opcode("EndGame", fi::ArgType::None, fi::Flow::End, fi::ArgDomain::None, true)},
	{0x16, fi::Opcode("IfMsgPrompt", fi::ArgType::Byte, fi::Flow::Jump, fi::ArgDomain::TextString, false)},
	{0x17, fi::Opcode("Jump", fi::ArgType::None, fi::Flow::Jump, fi::ArgDomain::None, true)}
};

static fi::Opcode parse_opcode_def(const std::string& p_definition) {
	auto kv{ klib::str::extract_keyval_str(p_definition, ',', '=') };

	fi::Opcode result{
		"",
		fi::ArgType::None,
		fi::Flow::Continue,
		fi::ArgDomain::None,
		false
	};

	for (const auto& [key, value] : kv) {
		const auto k{ klib::str::to_lower(klib::str::trim(key)) };

		if (k == "mnemonic")
			result.name = klib::str::trim(value);
		else if (k == "argtype")
			result.arg_type = klib::str::parse_enum_ci<fi::ArgType>(value);
		else if (k == "flow")
			result.flow = klib::str::parse_enum_ci<fi::Flow>(value);
		else if (k == "argdomain")
			result.domain = klib::str::parse_enum_ci<fi::ArgDomain>(value);
		else if (k == "terminal")
			result.ends_stream = klib::str::parse_bool_ci(value);
		else
			throw std::runtime_error(std::format("Unknown opcode property: {}", key));
	}

	if (result.name.empty())
		throw std::runtime_error("Opcode definition missing Mnemonic");

	return result;
}

void fi::load_iscript_opcodes_from_config(const std::map<byte, std::string>& p_opcode_defs) {
	constexpr bool THROW_ON_OPCODE_DIFFS{ false };

	if (p_opcode_defs.empty())
		return;

	std::map<byte, fi::Opcode> l_opcodes;

	for (const auto& kv : p_opcode_defs)
		l_opcodes.insert(std::make_pair(kv.first, parse_opcode_def(kv.second)));

	if constexpr (THROW_ON_OPCODE_DIFFS) {
		if (l_opcodes != fi::opcodes)
			throw std::runtime_error("Vanilla iScript opcodes do not match config");
	}

	fi::opcodes = l_opcodes;
}

std::vector<byte> fi::Instruction::get_bytes(void) const {
	std::vector<byte> result{ opcode_byte };
	if (type == Instruction_type::Directive)
		return result;

	const auto& op{ fi::opcodes.at(opcode_byte) };

	if (op.arg_type == fi::ArgType::Byte)
		result.push_back(static_cast<byte>(operand.value()));
	else if (op.arg_type == fi::ArgType::Short) {
		uint16_t opval{ operand.value() };
		result.push_back(static_cast<byte>(opval % 256));
		result.push_back(static_cast<byte>(opval / 256));
	}

	if (op.flow == fi::Flow::Jump || op.flow == fi::Flow::Read) {
		uint16_t opval{ static_cast<uint16_t>(jump_target.value()) };
		result.push_back(static_cast<byte>(opval % 256));
		result.push_back(static_cast<byte>(opval / 256));
	}

	return result;
}
