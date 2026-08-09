#include "BScriptOpcode.h"
#include <format>
#include <stdexcept>
#include "common/klib/Kstring.h"

fb::BScriptOpcode::BScriptOpcode(const std::string& p_str) :
	flow{ fb::Flow::Continue }
{
	const auto& xml_args{ klib::str::extract_keyval_str(p_str) };

	for (const auto& kv : xml_args) {
		if (klib::str::str_equals_icase(kv.first, c::XML_TYPE_MNEMONIC))
			mnemonic = kv.second;
		else if (klib::str::str_equals_icase(kv.first, c::XML_TYPE_FLOW)) {
			if (!c::STR_FLOW.contains(klib::str::to_lower(kv.second)))
				throw std::runtime_error(
					std::format("Invalid flow type '{}' in opcode definition string '{}'",
						kv.second, p_str)
				);

			flow = c::STR_FLOW.at(klib::str::to_lower(kv.second));
		}
		else if (klib::str::str_equals_icase(kv.first, c::XML_TYPE_ARG)) {
			auto str_args{ klib::str::split_string(kv.second, ':') };

			for (const auto& arg : str_args) {

				if (!c::STR_ARGDOMAIN.contains(klib::str::to_lower(arg)))
					throw std::runtime_error(
						std::format("Invalid argument type '{}' in opcode definition string '{}'",
							arg, p_str)
					);

				fb::ArgDomain argdomain{ c::STR_ARGDOMAIN.at(klib::str::to_lower(arg)) };
				fb::ArgDataType argtype{ fb::ArgDataType::Byte };

				if (argdomain == fb::ArgDomain::Addr ||
					argdomain == fb::ArgDomain::TrueAddr ||
					argdomain == fb::ArgDomain::FalseAddr ||
					argdomain == fb::ArgDomain::RAM)
					argtype = fb::ArgDataType::Word;

				args.push_back(fb::BScriptArg(argtype, argdomain));
			}
		}
	}

	if (mnemonic.empty())
		throw std::runtime_error(std::format("No mnemonic given in opcode definition string '{}'", p_str));
}

std::size_t fb::BScriptInstruction::size(void) const {
	std::size_t result{ behavior_byte.has_value() ? static_cast<std::size_t>(2) : static_cast<std::size_t>(1) };

	for (const auto& arg : operands)
		if (arg.data_type == fb::ArgDataType::Byte)
			result += 1;
		else
			result += 2;

	return result;
}

fb::Flow fb::BScriptInstruction::flow(const std::map<byte, fb::BScriptOpcode>& p_opcodes,
	const std::map<byte, fb::BScriptOpcode>& p_behavior_ops) const {
	if (behavior_byte.has_value()) {
		return p_behavior_ops.at(behavior_byte.value()).flow;
	}
	else
		return p_opcodes.at(opcode_byte).flow;
}

std::vector<byte> fb::BScriptInstruction::get_bytes(void) const {
	std::vector<byte> result;
	result.push_back(opcode_byte);
	if (behavior_byte.has_value())
		result.push_back(behavior_byte.value());

	for (const auto& arg : operands) {
		if (!arg.data_value.has_value())
			throw std::runtime_error("Missing operand value");
		if (arg.data_type == fb::ArgDataType::Byte)
			result.push_back(static_cast<byte>(arg.data_value.value()));
		else {
			result.push_back(static_cast<byte>(arg.data_value.value() % 256));
			result.push_back(static_cast<byte>(arg.data_value.value() / 256));
		}
	}

	return result;
}

std::map<byte, fb::BScriptOpcode> fb::parse_opcodes(
	const std::map<byte, std::string>& p_map) {
	std::map<byte, fb::BScriptOpcode> result;

	for (const auto& kv : p_map)
		result.insert(std::make_pair(kv.first, fb::BScriptOpcode(kv.second)));

	return result;
}
