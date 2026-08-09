#include "MusicOpcode.h"
#include "common/klib/Kstring.h"
#include "fm_constants.h"
#include <format>
#include <stdexcept>

fm::MusicOpcode::MusicOpcode(void) :
	MusicOpcode("", fm::OpcodeType::Note,
		fm::AudioArgType::None,
		fm::AudioFlow::Continue,
		fm::AudioArgDomain::None)
{
}

fm::MusicOpcode::MusicOpcode(const std::string& p_mnemonic,
	fm::OpcodeType p_opcodetype,
	fm::AudioArgType p_argtype,
	fm::AudioFlow p_flow,
	fm::AudioArgDomain p_argdomain) :
	m_opcodetype{ p_opcodetype },
	m_mnemonic{ p_mnemonic },
	m_argtype{ p_argtype },
	m_flow{ p_flow },
	m_arg_domain{ p_argdomain }
{
}

fm::MusicOpcode::MusicOpcode(const std::string& p_params) :
	m_opcodetype{ fm::OpcodeType::Opcode },
	m_argtype{ fm::AudioArgType::None },
	m_flow{ fm::AudioFlow::Continue },
	m_arg_domain{ fm::AudioArgDomain::None }
{
	auto kvs{ klib::str::extract_keyval_str(p_params) };

	for (const auto& kv : kvs) {
		if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_MNEMONIC)) {
			m_mnemonic = kv.second;
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_ARG)) {
			m_argtype = string_to_enum_argtype(kv.second);
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_FLOW)) {
			m_flow = string_to_enum_flow(kv.second);
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_TYPE)) {
			m_opcodetype = string_to_enum_opcodetype(kv.second);
		}
		else if (klib::str::to_lower(kv.first) == klib::str::to_lower(c::XML_OPCODE_PARAM_DOMAIN)) {
			m_arg_domain = string_to_enum_argdomain(kv.second);
		}
		else
			throw std::runtime_error("Invalid opcode parameter name " + kv.first);
	}

	if (m_mnemonic.empty())
		throw std::runtime_error("Mnemonic for audio opcode not given");
}

std::size_t fm::MusicOpcode::size(void) const {
	std::size_t result{ 1 };

	if (m_argtype == fm::AudioArgType::Byte)
		++result;
	else if (m_argtype == fm::AudioArgType::Address)
		result += 2;

	return result;
}

std::size_t fm::MusicInstruction::size(void) const {
	std::size_t result{ 1 };

	if (operand.has_value())
		++result;
	if (jump_target.has_value())
		result += 2;

	return result;
}

std::vector<byte> fm::MusicInstruction::get_bytes(void) const {
	std::vector<byte> result{ opcode_byte };

	if (operand.has_value())
		result.push_back(static_cast<byte>(operand.value()));
	if (jump_target.has_value()) {
		uint16_t opval{ static_cast<uint16_t>(jump_target.value()) };
		result.push_back(static_cast<byte>(opval % 256));
		result.push_back(static_cast<byte>(opval / 256));
	}

	return result;
}

std::map<byte, fm::MusicOpcode> fm::parse_opcode_map(const std::map<byte, std::string>& p_map) {
	std::map<byte, fm::MusicOpcode> result;

	for (const auto& kv : p_map)
		result.insert(std::make_pair(kv.first, fm::MusicOpcode(kv.second)));

	return result;
}

fm::AudioFlow fm::string_to_enum_flow(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "jump")
		return fm::AudioFlow::Jump;
	else if (klib::str::to_lower(p_str) == "continue")
		return fm::AudioFlow::Continue;
	else if (klib::str::to_lower(p_str) == "end")
		return fm::AudioFlow::End;
	else
		throw std::runtime_error("Not a valid flow type: " + p_str);
}

fm::AudioArgType fm::string_to_enum_argtype(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "none")
		return fm::AudioArgType::None;
	else if (klib::str::to_lower(p_str) == "byte")
		return fm::AudioArgType::Byte;
	else if (klib::str::to_lower(p_str) == "addr")
		return fm::AudioArgType::Address;
	else
		throw std::runtime_error("Not a valid flow type: " + p_str);
}

fm::OpcodeType fm::string_to_enum_opcodetype(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "opcode")
		return fm::OpcodeType::Opcode;
	else if (klib::str::to_lower(p_str) == "note")
		return fm::OpcodeType::Note;
	else
		throw std::runtime_error("Not a valid opcode type: " + p_str);
}

fm::AudioArgDomain fm::string_to_enum_argdomain(const std::string& p_str) {
	if (klib::str::to_lower(p_str) == "none")
		return fm::AudioArgDomain::None;
	else if (klib::str::to_lower(p_str) == "pitchoffset")
		return fm::AudioArgDomain::PitchOffset;
	else if (klib::str::to_lower(p_str) == "sqcontrol")
		return fm::AudioArgDomain::SQControl;
	else if (klib::str::to_lower(p_str) == "envelope")
		return fm::AudioArgDomain::Envelope;
	else
		throw std::runtime_error("Not a valid argument domain: " + p_str);
}
