#include "Opcode.h"
#include "./../common/klib/Kstring.h"

std::map<byte, fi::Opcode> fi::opcodes{
	{0x00, fi::Opcode("End", {}, fi::Flow::End, true)},
	{0x01, fi::Opcode("MsgNoskip", {{fi::ArgType::Byte, fi::ArgDomain::TextString}}, fi::Flow::Continue, false)},
	{0x02, fi::Opcode("MsgPrompt", {{fi::ArgType::Byte, fi::ArgDomain::TextString}}, fi::Flow::Continue, false)},
	{0x03, fi::Opcode("Msg", {{fi::ArgType::Byte, fi::ArgDomain::TextString}}, fi::Flow::Continue, false)},
	{0x04, fi::Opcode("IfTitleChange", {}, fi::Flow::Jump, false)},
	{0x05, fi::Opcode("LoseGold", {{fi::ArgType::Short, fi::ArgDomain::None}}, fi::Flow::Continue, false)},
	{0x06, fi::Opcode("SetSpawn", {{fi::ArgType::Byte, fi::ArgDomain::None}}, fi::Flow::Continue, false)},
	{0x07, fi::Opcode("GetItem", {{fi::ArgType::Byte, fi::ArgDomain::Item}}, fi::Flow::Continue, false)},
	{0x08, fi::Opcode("OpenShopBuy", {}, fi::Flow::Read, false)},
	{0x09, fi::Opcode("GetGold", {{fi::ArgType::Short, fi::ArgDomain::None}}, fi::Flow::Continue, false)},
	{0x0a, fi::Opcode("GetMana", {{fi::ArgType::Byte, fi::ArgDomain::None}}, fi::Flow::Continue, false)},
	{0x0b, fi::Opcode("IfQuest", {{fi::ArgType::Byte, fi::ArgDomain::Quest}}, fi::Flow::Jump, false)},
	{0x0c, fi::Opcode("IfRank", {{fi::ArgType::Byte, fi::ArgDomain::Rank}}, fi::Flow::Jump, false)},
	{0x0d, fi::Opcode("IfGold", {}, fi::Flow::Jump, false)},
	{0x0e, fi::Opcode("SetQuest", {{fi::ArgType::Byte, fi::ArgDomain::Quest}}, fi::Flow::Continue, false)},
	{0x0f, fi::Opcode("IfBuy", {}, fi::Flow::Jump, false)},
	{0x10, fi::Opcode("LoseItem", {{fi::ArgType::Byte, fi::ArgDomain::Item}}, fi::Flow::Continue, false)},
	{0x11, fi::Opcode("OpenShopSell", {}, fi::Flow::Read, false)},
	{0x12, fi::Opcode("IfItem", {{fi::ArgType::Byte, fi::ArgDomain::Item}}, fi::Flow::Jump, false)},
	{0x13, fi::Opcode("GetHealth", {{fi::ArgType::Byte, fi::ArgDomain::None}}, fi::Flow::Continue, false)},
	{0x14, fi::Opcode("ShowMantra", {}, fi::Flow::Continue, false)},
	{0x15, fi::Opcode("EndGame", {}, fi::Flow::End, true)},
	{0x16, fi::Opcode("IfMsgPrompt", {{fi::ArgType::Byte, fi::ArgDomain::TextString}}, fi::Flow::Jump, false) },
	{0x17, fi::Opcode("Jump", {}, fi::Flow::Jump, true)}
};

std::map<std::string, fi::Opcode> fi::implementation_opcodes;

namespace {

	const fi::Opcode NONE_CONTINUE{
		"",
		{},
		fi::Flow::Continue,
		false
	};

	struct ParsedOpcodeDef {
		fi::Opcode opcode;
		std::optional<std::string> impl;
	};
}

static std::vector<fi::Argument> parse_arguments(const std::string& p_value) {
	constexpr char TYPE_SERIALIZE{ '+' };
	constexpr char TYPE_DOMAIN_DELIM{ ':' };

	std::vector<fi::Argument> result;

	for (const auto& raw_token : klib::str::split_string(p_value, TYPE_SERIALIZE)) {
		const auto parts{
			klib::str::split_string(klib::str::trim(raw_token), TYPE_DOMAIN_DELIM)
		};

		if (parts.empty() || parts.size() > 2)
			throw std::runtime_error(std::format(
				"Invalid opcode argument '{}'", raw_token));

		fi::Argument arg{
			.type = klib::str::parse_enum_ci<fi::ArgType>(
				klib::str::trim(parts[0])),
			.domain = fi::ArgDomain::None
		};

		if (parts.size() == 2)
			arg.domain = klib::str::parse_enum_ci<fi::ArgDomain>(
				klib::str::trim(parts[1]));

		result.push_back(arg);
	}

	return result;
}

static ParsedOpcodeDef parse_opcode_properties(const std::string& p_definition) {
	// stay backward compatible with definitions that assume one argument
	std::optional<fi::ArgType> legacy_type;
	std::optional<fi::ArgDomain> legacy_domain;
	bool saw_new_arg{ false };

	auto kv{ klib::str::extract_keyval_str(p_definition, ',', '=') };

	// default
	fi::Opcode result{ NONE_CONTINUE };

	std::optional<std::string> impl;

	for (const auto& [key, value] : kv) {
		const auto k{ klib::str::to_lower(klib::str::trim(key)) };

		if (k == "mnemonic")
			result.name = klib::str::trim(value);
		else if (k == "argtype")
			legacy_type = klib::str::parse_enum_ci<fi::ArgType>(value);
		else if (k == "flow")
			result.flow = klib::str::parse_enum_ci<fi::Flow>(value);
		else if (k == "argdomain")
			legacy_domain = klib::str::parse_enum_ci<fi::ArgDomain>(value);
		else if (k == "terminal")
			result.ends_stream = klib::str::parse_bool_ci(value);
		else if (k == "impl") {
			impl = klib::str::trim(value);
		}
		else if (k == "args") {
			result.args = parse_arguments(value);
			saw_new_arg = true;
		}

		else
			throw std::runtime_error(std::format("Unknown opcode property: {}", key));
	}

	if (saw_new_arg && (legacy_type || legacy_domain))
		throw std::runtime_error("Cannot specify both Args and legacy ArgType/ArgDomain");

	if (legacy_type) {
		if (*legacy_type != fi::ArgType::None)
			result.args.push_back({
				*legacy_type,
				legacy_domain.value_or(fi::ArgDomain::None)
				});
	}
	else if (legacy_domain) {
		throw std::runtime_error(
			"ArgDomain requires ArgType");
	}

	return { result, impl };
}

static fi::Opcode parse_opcode_def(const std::string& p_definition, std::vector<std::string>& p_required_impls) {
	auto parsed{ parse_opcode_properties(p_definition) };

	if (parsed.impl)
		p_required_impls.push_back(*parsed.impl);
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

static void load_opcode_implementations(const std::map<std::string, std::string>& p_impl_defs) {
	fi::implementation_opcodes.clear();

	for (const auto& [impl_name, definition] : p_impl_defs) {
		auto parsed{ parse_opcode_properties(definition) };

		if (parsed.impl)
			throw std::runtime_error(std::format(
				"Opcode implementation '{}' must not specify Impl", impl_name));

		parsed.opcode.name = impl_name;

		const auto key{ klib::str::to_lower(impl_name) };
		fi::implementation_opcodes.emplace(key, parsed.opcode);
	}
}

fi::ScriptOpcodeInfo fi::load_iscript_opcodes_from_config(const std::map<byte, std::string>& p_opcode_defs,
	const std::map<std::string, std::string>& p_impl_defs) {
	constexpr bool THROW_ON_OPCODE_DIFFS{ false };

	fi::ScriptOpcodeInfo result;

	if (p_opcode_defs.empty())
		return result;

	load_opcode_implementations(p_impl_defs);

	std::map<byte, fi::Opcode> l_opcodes;

	byte expected{ 0 };

	for (const auto& kv : p_opcode_defs) {

		// ensure the map is dense
		if (kv.first != expected)
			throw std::runtime_error(std::format("Expected opcode ${:02X}, found ${:02X}", expected, kv.first));

		++expected;

		auto parsed{ parse_opcode_def(kv.second, result.required_impls) };
		l_opcodes.insert(std::make_pair(kv.first, parsed));
	}

	result.base_opcode_count = p_opcode_defs.size() - result.required_impls.size();

	if constexpr (THROW_ON_OPCODE_DIFFS) {
		if (l_opcodes != fi::opcodes)
			throw std::runtime_error("Vanilla iScript opcodes do not match config");
	}

	fi::opcodes = l_opcodes;

	return result;
}

// opcode members
fi::Opcode::Opcode(const std::string& p_name, std::initializer_list<Argument> p_args,
	Flow p_flow, bool p_ends_stream) :
	name{ p_name },
	args{ p_args },
	flow{ p_flow },
	ends_stream{ p_ends_stream }
{
}

std::size_t fi::Opcode::token_count(void) const {
	std::size_t result{ 1 + args.size() };

	if (flow == Flow::Jump || flow == Flow::Read)
		++result;

	return result;
}

std::size_t fi::Opcode::size(void) const {
	std::size_t result{ 1 }; // the opcode itself
	if (flow == fi::Flow::Jump || flow == fi::Flow::Read)
		result += 2;
	for (const auto& arg : args) {
		if (arg.type == fi::ArgType::Short)
			result += 2;
		else if (arg.type == fi::ArgType::Byte)
			++result;
	}
	return result;
}

// instruction members
std::vector<byte> fi::Instruction::get_bytes(void) const {
	std::vector<byte> result{ opcode_byte };
	if (type == Instruction_type::Directive)
		return result;

	const auto& op{ fi::opcodes.at(opcode_byte) };

	if (operands.size() != op.args.size())
		throw std::runtime_error(std::format("Opcode '{}' expects {} operand(s), got {}",
			op.name, op.args.size(), operands.size()));

	for (std::size_t i{ 0 }; i < op.args.size(); ++i) {
		const auto& arg{ op.args[i] };
		const auto opval{ operands[i] };

		if (arg.type == fi::ArgType::Byte)
			result.push_back(static_cast<byte>(opval));
		else if (arg.type == fi::ArgType::Short) {
			result.push_back(static_cast<byte>(opval % 256));
			result.push_back(static_cast<byte>(opval / 256));
		}
	}

	if (op.flow == fi::Flow::Jump || op.flow == fi::Flow::Read) {
		uint16_t opval{ static_cast<uint16_t>(jump_target.value()) };
		result.push_back(static_cast<byte>(opval % 256));
		result.push_back(static_cast<byte>(opval / 256));
	}

	return result;
}
