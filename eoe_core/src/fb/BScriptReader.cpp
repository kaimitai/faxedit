#include "BScriptReader.h"
#include "fb_constants.h"
#include "common/klib/Kstring.h"
#include <format>
#include <stdexcept>

fb::BScriptReader::BScriptReader(const fe::Config& p_config) :
	opcodes{ fb::parse_opcodes(p_config.bmap(c::ID_BSCRIPT_OPCODES)) },
	behavior_ops{ fb::parse_opcodes(p_config.bmap(c::ID_BSCRIPT_BEHAVIORS)) },
	bscript_ptr{ p_config.pointer(c::ID_BSCRIPT_PTR) },
	bscript_count{ p_config.constant(c::ID_SPRITE_COUNT) },
	rg_1_end{ p_config.constant(c::ID_BSCRIPT_RG1_END) },
	rg_2_start{ p_config.constant(c::ID_BSCRIPT_RG2_START) },
	rg_2_end{ p_config.constant(c::ID_BSCRIPT_RG2_END) }
{
}

void fb::BScriptReader::read_asm(const std::vector<std::string>& p_asm,
	const fe::Config& p_config) {
	defines.clear();
	instructions.clear();
	ptr_table.clear();

	std::map<fb::SectionType, std::vector<std::string>> sections;

	auto l_lines{ p_asm };
	fb::SectionType currentSection{ fb::SectionType::Defines };

	for (auto& line : l_lines) {
		line = klib::str::trim(klib::str::strip_comment(line));

		if (line == c::SECTION_DEFINES) {
			currentSection = fb::SectionType::Defines;
		}
		else if (line == c::SECTION_BSCRIPT) {
			currentSection = fb::SectionType::BScript;
		}
		else if (!line.empty())
			sections[currentSection].push_back(line);
	}

	// populate defines
	if (sections.contains(fb::SectionType::Defines))
		for (const std::string& s : sections[fb::SectionType::Defines]) {
			const auto def{ klib::str::parse_define(s) };
			if (defines.contains(def.first))
				throw std::runtime_error(std::format(
					"Duplicate define '{}'", def.first));
			defines.emplace(def.first, klib::str::parse_numeric(def.second));
		}

	if (!sections.contains(fb::SectionType::BScript))
		throw std::runtime_error(std::format("File is missing section {}", c::SECTION_BSCRIPT));

	// parse the asm code

	/*
		 this is where we statically link all jump targets and ptr table refs
		 we also bridge the gap between two safe regions if we overflow

		These terms need to be clearly separate:
		Offset = Actual byte address locations
		Instruction index = The instruction number a label or ptr references

		Here is the general linking strategy we will employ:

		Data we need:
		  A vector to hold our instructions
		  A map from entrypoint (ptr table index) to instruction index
		  A map from label to instruction index
		  A map from label to set of (instruction index, argument index) jumping to that label

		1) We will start with a byte offset of 0
		2) Parse asm line by line
		   - if entrypoint: store the instruction index it points to (current instr.size())
		   - if label: store the instruction index it points to (current instr.size())
		   - else: emit byte data

			 - make instruction
			   - if contains jump argument: store instruction index and arg index and in the label -> instrs map
			   - tentatively store the current byte offset directly in the instruction

		3) Once we have the tentative offsets for all instructions, check if we overflow
		   If we do, find the first End- or Unconditional Jump-instruction fully within
		   safe region 1. Take the first instruction after that one and calculate
		   the byte length from its offset to start of safe region 2.
		   Add this delta to the offsets for all instructions from this one onward.
		4) Once we have the final offsets for all instructions, loop over all labels
		   and ptr table entries, and assign them the byte address of the instruction
		   which indexes they already point to
		5) Once we know all label byte locations, do another pass over all instructions
		   and patch their jump target based on the final offset of the label they jump to
		6) Very final pass over all instructions and pointer table entries: Add the delta
		   between our local data-relative zero addr offset and bank zero addr offset

 */

	// make opcode mnemonic reverse lookup
	std::map<std::string, byte> op_mnemonics, bh_mnemonics;
	for (const auto& opc : opcodes)
		op_mnemonics.insert(std::make_pair(
			klib::str::to_lower(opc.second.mnemonic), opc.first
		));
	for (const auto& opc : behavior_ops)
		bh_mnemonics.insert(std::make_pair(
			klib::str::to_lower(opc.second.mnemonic), opc.first
		));

	// make a map of all arguments an opcode takes
	std::map<std::string, std::set<fb::ArgDomain>> opcode_args;
	for (const auto& kv : opcodes) {
		std::string l_mnemonic{ klib::str::to_lower(kv.second.mnemonic) };
		if (opcode_args.contains(l_mnemonic))
			throw std::runtime_error(std::format("Opcode {} defined more than once", l_mnemonic));
		else {
			for (auto templatearg : kv.second.args)
				opcode_args[l_mnemonic].insert(templatearg.domain);
		}
	}
	for (const auto& kv : behavior_ops) {
		std::string l_mnemonic{ klib::str::to_lower(kv.second.mnemonic) };
		if (opcode_args.contains(l_mnemonic))
			throw std::runtime_error(std::format("Opcode {} defined more than once", l_mnemonic));
		else {
			for (auto templatearg : kv.second.args)
				opcode_args[l_mnemonic].insert(templatearg.domain);
		}
	}

	// map string label to instruction index
	std::map<std::string, std::size_t> label_to_instr_idx;
	// map from ptr table index to instruction index
	std::map<std::size_t, std::size_t> ptr_to_instr_index;
	// map from label to all (instruction index, arg index) having it as target
	std::map<std::string, std::set<std::pair<size_t, std::size_t>>> jump_labels;
	// tentative byte offsets
	std::size_t offset{ 0 };

	for (std::size_t line_no{ 0 }; line_no < sections.at(fb::SectionType::BScript).size(); ++line_no) {
		std::string line{ sections.at(fb::SectionType::BScript).at(line_no) };
		auto rawargs{ klib::str::split_string(line, '=') };
		for (auto& str : rawargs)
			str = klib::str::trim(str);

		std::vector<std::string> args;
		for (const auto& str : rawargs) {
			auto splitargs{ klib::str::split_whitespace(str) };
			args.insert(end(args), begin(splitargs), end(splitargs));
		}

		if (is_label(line, args)) {
			const auto label{ get_label(args) };
			if (!label_to_instr_idx.emplace(label, instructions.size()).second)
				throw std::runtime_error(std::format(
					"Duplicate label '@{}' on line '{}'", label, line));
		}
		else if (is_entrypoint(line, args)) {
			const auto entrypoint{ get_entrypoint(args) };
			if (entrypoint >= bscript_count)
				throw std::runtime_error(std::format(
					"Entrypoint {} is outside the valid range 0..{} on line '{}'",
					entrypoint, bscript_count - 1, line));
			if (!ptr_to_instr_index.emplace(entrypoint, instructions.size()).second)
				throw std::runtime_error(std::format(
					"Duplicate entrypoint {} on line '{}'", entrypoint, line));
		}
		else {
			// we have an instruction - generate bytes
			std::string mnemonic{ klib::str::to_lower(args.at(0)) };
			bool real_opcode{ false };
			if (op_mnemonics.contains(mnemonic)) {
				real_opcode = true;
			}
			else if (!bh_mnemonics.contains(mnemonic))
				throw std::runtime_error(std::format("Unknown opcode '{}' on line '{}'", args[0], line));

			byte opcode_byte{ real_opcode ? op_mnemonics.at(mnemonic) : bh_mnemonics.at(mnemonic) };
			const auto& optmpl{ real_opcode ? opcodes.at(opcode_byte) : behavior_ops.at(opcode_byte) };

			std::map<fb::ArgDomain, std::string> argmap;

			// opcodes taking exactly 1 argument don't need the argument name necessarily
			if (args.size() == 2) {
				if (optmpl.args.size() == 1)
					argmap.insert(std::make_pair(optmpl.args[0].domain, args[1]));
				else throw std::runtime_error(std::format("Could not parse line '{}'", line));
			}
			else
				argmap = get_argmap(line, args);

			validate_argmap(line, mnemonic, argmap, opcode_args);

			fb::BScriptInstruction instr(real_opcode ? opcode_byte : 0x00);
			if (!real_opcode)
				instr.behavior_byte = opcode_byte;

			const auto& argtmp{ optmpl.args };

			for (std::size_t i{ 0 }; i < argtmp.size(); ++i) {
				if (argtmp[i].domain == fb::ArgDomain::Addr ||
					argtmp[i].domain == fb::ArgDomain::TrueAddr ||
					argtmp[i].domain == fb::ArgDomain::FalseAddr) {
					jump_labels[get_label_name(line, argmap, argtmp[i].domain)].insert(
						std::make_pair(instructions.size(), i)
					);
					instr.operands.push_back(fb::ArgInstance(fb::ArgDataType::Word, 0));
				}
				else {
					int value{ resolve_value(line, argmap, argtmp[i].domain) };
					fb::ArgDataType valtype{ fb::ArgDataType::Byte };
					if (argtmp[i].domain == fb::ArgDomain::RAM)
						valtype = fb::ArgDataType::Word;
					const bool signed_byte{ argtmp[i].domain == fb::ArgDomain::SignedByte };
					if (signed_byte && (value < -128 || value > 0xff))
						throw std::runtime_error(std::format(
							"Signed-byte operand {} is outside -128..255 on line '{}'", value, line));
					if (valtype == fb::ArgDataType::Byte && !signed_byte && (value < 0 || value > 0xff))
						throw std::runtime_error(std::format(
							"Byte operand {} is outside 0..255 on line '{}'", value, line));
					if (valtype == fb::ArgDataType::Word && (value < 0 || value > 0xffff))
						throw std::runtime_error(std::format(
							"Word operand {} is outside 0..65535 on line '{}'", value, line));

					const std::size_t finalvalue{ signed_byte
						? static_cast<std::size_t>(static_cast<byte>(value))
						: static_cast<std::size_t>(value) };

					instr.operands.push_back(fb::ArgInstance(valtype, finalvalue));
				}
			}

			instr.byte_offset = offset;
			instructions.push_back(instr);
			offset += instr.size();
		}
	}

	// patch all unresolved jump targets
	// and make them point to the correct instruction index
	for (const auto& jt : jump_labels) {
		if (!label_to_instr_idx.contains(jt.first))
			throw std::runtime_error(std::format("Undefined label {}", jt.first));

		std::size_t jumptoinstridx{ label_to_instr_idx.at(jt.first) };
		for (const auto& jumpargs : jt.second) {
			std::size_t instr_no{ jumpargs.first };
			std::size_t arg_no{ jumpargs.second };

			instructions.at(instr_no).operands.at(arg_no).data_value = jumptoinstridx;
		}
	}

	// Insert overflow-bridge if needed
	std::size_t split_index{ find_split_index(rg_1_end - (bscript_ptr.first + 2 * bscript_count)) };
	if (split_index < instructions.size()) {
		std::size_t rg1_data_start_rom = bscript_ptr.first + 2 * bscript_count;
		std::size_t rg2_ptr_table_relative{ rg_2_start - rg1_data_start_rom };

		std::size_t delta{ rg2_ptr_table_relative - instructions[split_index].byte_offset.value() };

		for (std::size_t i{ split_index }; i < instructions.size(); ++i)
			instructions[i].byte_offset = instructions[i].byte_offset.value() + delta;
	}

	// all entry points and jump targets point to the correct instruct idx
	// and all instructions have the correct byte offset
	// we now update with the correct values
	const std::size_t PTR_DELTA{ bscript_ptr.first + 2 * bscript_count - bscript_ptr.second };

	for (auto& instr : instructions)
		instr.byte_offset.value() += PTR_DELTA;

	for (const auto& kv : jump_labels) {
		for (const auto& jumpargs : kv.second) {
			std::size_t instr_no{ jumpargs.first };
			std::size_t arg_no{ jumpargs.second };
			auto& instrarg{ instructions.at(instr_no).operands.at(arg_no) };

			instrarg.data_value = instructions.at(instrarg.data_value.value()).byte_offset.value();
		}
	}

	for (std::size_t i{ 0 }; i < bscript_count; ++i) {
		if (!ptr_to_instr_index.contains(i))
			throw std::runtime_error(std::format("Entrypoint {} not defined", i));
		ptr_table.push_back(instructions.at(ptr_to_instr_index.at(i)).byte_offset.value());
	}
}

std::string fb::BScriptReader::get_label_name(const std::string& p_asm, const std::map<fb::ArgDomain, std::string>& p_argmap,
	fb::ArgDomain domain) const {
	if (!p_argmap.contains(domain) || p_argmap.at(domain).size() < 2)
		throw std::runtime_error(std::format("Missing label target on line '{}'", p_asm));
	else
		return p_argmap.at(domain).substr(1);
}

int fb::BScriptReader::resolve_value(const std::string& p_asm, const std::map<fb::ArgDomain, std::string>& p_argmap,
	fb::ArgDomain domain) const {
	if (!p_argmap.contains(domain))
		return get_default_value(p_asm, domain);
	else {
		const auto& argval{ p_argmap.at(domain) };
		if (defines.contains(argval))
			return defines.at(argval);
		else
			return klib::str::parse_numeric(argval);
	}
}

int fb::BScriptReader::get_default_value(const std::string& p_asm, fb::ArgDomain domain) const {
	if (domain == fb::ArgDomain::Zero)
		return 0;
	else
		throw std::runtime_error(std::format("Missing value and no default value available on line '{}'", p_asm));
}

bool fb::BScriptReader::is_label(const std::string& p_asm, const std::vector<std::string>& p_line) const {
	if (p_line.at(0).at(0) == '@') {
		if (p_line.size() == 1 && p_line.at(0).back() == ':') {
			return true;
		}
		else {
			throw std::runtime_error(std::format("Invalid label definition on line '{}'", p_asm));
		}
	}
	else
		return false;
}

std::string fb::BScriptReader::get_label(const std::vector<std::string>& p_line) const {
	std::string label{ p_line.at(0).substr(1) };
	label.pop_back();
	return label;
}

bool fb::BScriptReader::is_entrypoint(const std::string& p_asm, const std::vector<std::string>& p_line) const {
	if (klib::str::str_equals_icase(p_line.at(0), c::DIRECTIVE_ENTRYPOINT)) {
		if (p_line.size() == 2) {
			return true;
		}
		else {
			throw std::runtime_error(std::format("Invalid label definition on line '{}'", p_asm));
		}
	}
	else
		return false;
}

std::size_t fb::BScriptReader::get_entrypoint(const std::vector<std::string>& p_line) const {
	const int entrypoint{ klib::str::parse_numeric(p_line.at(1)) };
	if (entrypoint < 0)
		throw std::runtime_error(std::format(
			"Entrypoint {} is outside the valid non-negative range", entrypoint));
	return static_cast<std::size_t>(entrypoint);
}

std::map<fb::ArgDomain, std::string> fb::BScriptReader::get_argmap(const std::string& p_asm,
	const std::vector<std::string>& p_line) const {
	std::map<fb::ArgDomain, std::string> result;

	if (p_line.size() % 2 == 0)
		throw std::runtime_error(std::format("Can not parse line {}", p_asm));

	for (std::size_t i{ 1 }; i < p_line.size(); i += 2) {
		if (!c::STR_ARGDOMAIN.contains(klib::str::to_lower(p_line[i])))
			throw std::runtime_error(std::format("Unknown argument type {} on line {}", p_line[i], p_asm));
		fb::ArgDomain domain{ c::STR_ARGDOMAIN.at(klib::str::to_lower(p_line[i])) };

		if (!result.emplace(domain, p_line[i + 1]).second)
			throw std::runtime_error(std::format(
				"Argument type {} specified more than once on line {}", p_line[i], p_asm));
	}

	return result;
}

// find a split index
std::size_t fb::BScriptReader::find_split_index(std::size_t region1_capacity_bytes) const {
	std::optional<std::size_t> last_safe_boundary;

	for (std::size_t i = 0; i < instructions.size(); ++i) {
		const auto& instr = instructions[i];

		bool fits = instr.byte_offset.value() + instr.size() <= region1_capacity_bytes;
		if (!fits) {
			if (!last_safe_boundary)
				return 0;                      // nothing safely in region 1

			return *last_safe_boundary + 1;    // first region-2 instr
		}

		auto flow{ instr.flow(opcodes, behavior_ops) };
		if (flow == fb::Flow::End || flow == fb::Flow::Jump)
			last_safe_boundary = i;
	}

	// everything fits
	return instructions.size();
}

void fb::BScriptReader::validate_argmap(const std::string& p_asm,
	const std::string& p_mnemonic,
	const std::map<fb::ArgDomain, std::string>& p_argmap,
	const std::map<std::string, std::set<fb::ArgDomain>>& p_opcode_args) const {

	// we know the opcode given is lowercase, and that the opcode args map
	// has an entry for this opcode

	for (const auto& kv : p_argmap)
		if (!p_opcode_args.at(p_mnemonic).contains(kv.first))
			throw std::runtime_error(std::format(
				"Invalid argument to opcode '{}' on line '{}'",
				p_mnemonic, p_asm));
}

std::pair<std::vector<byte>, std::vector<byte>> fb::BScriptReader::to_bytes(void) const {
	std::vector<byte> result_a, result_b;

	// bank relative addr of ptr table start
	std::size_t rg1_start_bank_relative = bscript_ptr.first - bscript_ptr.second;
	// size of rg1 ptr table + script data
	std::size_t rg1_byte_size{ rg_1_end - bscript_ptr.first };
	// bank offset beyond the safe region 1
	std::size_t rg1_end_bank_relative{ rg1_start_bank_relative + rg1_byte_size };

	for (const std::size_t ep : ptr_table) {
		result_a.push_back(static_cast<byte>(ep % 256));
		result_a.push_back(static_cast<byte>(ep / 256));
	}

	for (const auto& instr : instructions) {
		const auto bytes{ instr.get_bytes() };
		if (instr.byte_offset.value() < rg1_end_bank_relative)
			result_a.insert(end(result_a), begin(bytes), end(bytes));
		else
			result_b.insert(end(result_b), begin(bytes), end(bytes));
	}

	return std::make_pair(result_a, result_b);
}
