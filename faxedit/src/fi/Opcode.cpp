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

std::map<std::string, fi::Opcode> fi::implementation_opcodes;

namespace {

	const fi::Opcode NONE_CONTINUE{
		"",
		fi::ArgType::None,
		fi::Flow::Continue,
		fi::ArgDomain::None,
		false
	};

	struct ParsedOpcodeDef {
		fi::Opcode opcode;
		std::optional<std::string> impl;
	};
}

static ParsedOpcodeDef parse_opcode_properties(const std::string& p_definition) {
	auto kv{ klib::str::extract_keyval_str(p_definition, ',', '=') };

	// default
	fi::Opcode result{ NONE_CONTINUE };

	std::optional<std::string> impl;

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
		else if (k == "impl") {
			impl = klib::str::trim(value);
		}

		else
			throw std::runtime_error(std::format("Unknown opcode property: {}", key));
	}

	return { result, impl };
}

static fi::Opcode parse_opcode_def(const std::string& p_definition, std::vector<std::string>& p_required_impls,
	bool p_impl_allowed) {
	auto parsed{ parse_opcode_properties(p_definition) };

	if (parsed.impl) {
		if (!p_impl_allowed)
			throw std::runtime_error("Vanilla opcodes may not specify Impl");

		p_required_impls.push_back(*parsed.impl);
	}
	else {
		// once an Impl-opcode has been seen, we cannot easily allow non-Impl opcodes
		if (!p_required_impls.empty())
			throw std::runtime_error(std::format(
				"Opcode '{}' did not specify Impl, but a previous one did",
				parsed.opcode.name));
	}

	if (parsed.impl) {
		auto opcode_name{ parsed.opcode.name };
		const auto key{ klib::str::to_lower(*parsed.impl) };

		const auto it{ fi::implementation_opcodes.find(key) };
		if (it == fi::implementation_opcodes.end())
			throw std::runtime_error(std::format(
				"Unknown script opcode implementation '{}'", *parsed.impl));

		parsed.opcode = it->second;

		if (!opcode_name.empty())
			parsed.opcode.name = opcode_name;
	}

	return parsed.opcode;
}

static void load_opcode_implementations(const std::map<byte, std::string>& p_impl_defs) {
	fi::implementation_opcodes.clear();

	for (const auto& [_, definition] : p_impl_defs) {
		auto parsed{ parse_opcode_properties(definition) };

		if (!parsed.impl)
			throw std::runtime_error("Opcode implementation definition missing Impl");

		parsed.opcode.name = *parsed.impl;

		const auto key{ klib::str::to_lower(*parsed.impl) };
		fi::implementation_opcodes.emplace(key, parsed.opcode);
	}
}

std::vector<std::string> fi::load_iscript_opcodes_from_config(const std::map<byte, std::string>& p_opcode_defs,
	const std::map<byte, std::string>& p_impl_defs) {
	constexpr bool THROW_ON_OPCODE_DIFFS{ false };

	std::vector<std::string> required_impls;

	if (p_opcode_defs.empty())
		return required_impls;

	load_opcode_implementations(p_impl_defs);

	std::map<byte, fi::Opcode> l_opcodes;

	byte expected{ 0 };

	for (const auto& kv : p_opcode_defs) {

		// ensure the map is dense
		if (kv.first != expected)
			throw std::runtime_error(std::format("Expected opcode ${:02X}, found ${:02X}", expected, kv.first));

		++expected;

		auto parsed{ parse_opcode_def(kv.second, required_impls, kv.first >= 0x18) };
		l_opcodes.insert(std::make_pair(kv.first, parsed));
	}

	if constexpr (THROW_ON_OPCODE_DIFFS) {
		if (l_opcodes != fi::opcodes)
			throw std::runtime_error("Vanilla iScript opcodes do not match config");
	}

	fi::opcodes = l_opcodes;

	return required_impls;
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
