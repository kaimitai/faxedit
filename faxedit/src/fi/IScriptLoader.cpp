#include "IScriptLoader.h"
#include "fi_constants.h"
#include <algorithm>
#include <format>

using byte = unsigned char;

fi::IScriptLoader::IScriptLoader(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	parse_strings(p_config, p_rom);

	auto l_iscript_ptr{ p_config.pointer(c::ID_ISCRIPT_PTR_LO) };
	std::size_t l_iscript_hi_ref_offset{ p_config.constant(c::ID_ISCRIPT_HI_REF_OFFSET) };
	std::size_t l_iscript_hi_addr{ static_cast<std::size_t>(p_rom.at(l_iscript_hi_ref_offset)) +
		256 * static_cast<std::size_t>(p_rom.at(l_iscript_hi_ref_offset + 1)) };

	std::size_t l_iscript_count{ l_iscript_hi_addr - (l_iscript_ptr.first - l_iscript_ptr.second) };

	m_ptr_zero_addr = l_iscript_ptr.second;
	m_defines_item = p_config.bmap(c::ID_DEFINES_ITEM);
	m_defines_quest = p_config.bmap(c::ID_DEFINES_QUEST);
	m_defines_rank = p_config.bmap(c::ID_DEFINES_RANK);
	m_defines_textbox = p_config.bmap(c::ID_DEFINES_TEXTBOX);

	for (std::size_t i{ 0 }; i < l_iscript_count; ++i)
		m_ptr_table.push_back(static_cast<std::size_t>(p_rom.at(l_iscript_ptr.first + i))
			+ 256 * static_cast<std::size_t>(p_rom.at(l_iscript_ptr.first + l_iscript_count + i))
			+ l_iscript_ptr.second);
}

std::size_t fi::IScriptLoader::get_script_count(void) const {
	return m_ptr_table.size();
}

std::vector<fi::AsmToken> fi::IScriptLoader::parse_script(const std::vector<byte>& p_rom, std::size_t p_script_no) {
	m_instructions.clear();
	m_jump_targets.clear();

	parse_blob_from_entrypoint(p_rom, m_ptr_table[p_script_no], true);

	return get_asm_code(p_script_no);
}

void fi::IScriptLoader::parse_strings(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	auto l_char_map{ p_config.bmap(c::ID_STRING_CHAR_MAP) };
	std::size_t l_string_offset{ p_config.constant(c::ID_STRING_DATA_START) };
	std::size_t l_string_end{ p_config.constant(c::ID_STRING_DATA_END) };

	m_strings.clear();
	std::string encodedstring;

	for (std::size_t i{ l_string_offset }; i < l_string_end; ++i) {

		if (p_rom.at(i) == 0xff) {
			m_strings.push_back(encodedstring);
			encodedstring.clear();
		}
		else {
			byte b{ p_rom.at(i) };
			auto iter{ l_char_map.find(b) };
			if (iter == end(l_char_map))
				encodedstring += std::format("<${:02x}>", b);
			else
				encodedstring += iter->second;
		}

	}
}

byte fi::IScriptLoader::read_byte(const std::vector<byte>& p_rom, size_t& offset) const {
	if (offset >= p_rom.size()) throw std::out_of_range("ROM read out of bounds");
	return p_rom[offset++];
}

uint16_t fi::IScriptLoader::read_short(const std::vector<byte>& p_rom, size_t& offset) const {
	uint8_t lo = read_byte(p_rom, offset);
	uint8_t hi = read_byte(p_rom, offset);
	return static_cast<uint16_t>(hi << 8 | lo);
}

void fi::IScriptLoader::parse_blob_from_entrypoint(const std::vector<byte>& p_rom,
	size_t offset, bool at_entrypoint) {
	if (m_instructions.find(offset) != end(m_instructions))
		return;

	size_t cursor = offset;

	if (at_entrypoint)
		m_instructions.insert(std::make_pair(offset,
			fi::Instruction({ Instruction_type::Directive, read_byte(p_rom, cursor), 1 })
		));

	while (cursor < p_rom.size()) {
		if (m_instructions.find(cursor) != end(m_instructions))
			return;

		size_t instr_offset = cursor;
		uint8_t opcode_byte = read_byte(p_rom, cursor);

		auto it = opcodes.find(opcode_byte);
		if (it == opcodes.end()) {
			throw std::runtime_error(std::format("Unknown opcode {:2x} at offset {:2x}",
				opcode_byte, instr_offset));
		}

		const Opcode& op = it->second;
		std::optional<uint16_t> arg;

		if (op.arg_type == ArgType::Byte) {
			arg = read_byte(p_rom, cursor);
		}
		else if (op.arg_type == ArgType::Short) {
			arg = read_short(p_rom, cursor);
		}

		std::optional<std::size_t> target_addr;

		// track jump targets, extract shops
		if (op.flow == Flow::Jump || op.flow == Flow::Read) {
			target_addr = static_cast<std::size_t>(read_short(p_rom, cursor))
				+ m_ptr_zero_addr;

			if (op.flow == Flow::Jump)
				m_jump_targets.insert(target_addr.value());
			else {
				const auto& shop_iter{ m_shop_addresses.find(target_addr.value()) };

				if (shop_iter == end(m_shop_addresses)) {
					// new shop, parse it and assign index
					fi::Shop newshop;
					std::size_t shop_offset{ target_addr.value() };
					while (p_rom.at(shop_offset) != 0xff) {
						newshop.add_entry(p_rom.at(shop_offset),
							p_rom.at(shop_offset + 1),
							p_rom.at(shop_offset + 2));
						shop_offset += 3;
					}

					arg = static_cast<uint16_t>(m_shops.size());
					m_shop_addresses[target_addr.value()] = static_cast<std::size_t>(arg.value());
					m_shops.push_back(newshop);

				}
				else {
					// already seen shop, use its index
					arg = static_cast<uint16_t>(shop_iter->second);
				}
			}
		}

		m_instructions.insert(
			std::make_pair(instr_offset,
				fi::Instruction(fi::Instruction_type::OpCode, opcode_byte, it->second.size(),
					arg, target_addr)));

		if (op.flow == Flow::Jump) {
			// parse all branches recursively, but store and restore cursors
			std::size_t temp_cursor{ cursor };
			cursor = target_addr.value();

			parse_blob_from_entrypoint(p_rom, cursor, false);

			cursor = temp_cursor;
		}

		if (op.ends_stream)
			break;

	}
}

// return the script as vector of asm token to be parsed by the renderer
std::vector<fi::AsmToken> fi::IScriptLoader::get_asm_code(std::size_t p_script_no) {
	std::vector<fi::AsmToken> result;
	std::map<std::size_t, std::string> labels;
	int last_label{ 0 };

	const auto& get_next_label = [](std::size_t p_offset, int& p_lastlabel,
		std::map<std::size_t, std::string>& p_labels) -> std::string {
			auto labiter{ p_labels.find(p_offset) };
			if (labiter != end(p_labels))
				return labiter->second;

			std::string tmp_label{ std::format("@iscript_{:03}",
				p_lastlabel++) };
			p_labels[p_offset] = tmp_label;
			return tmp_label;
		};

	for (const auto& instrs : m_instructions) {
		std::size_t offset{ instrs.first };
		const auto& instr{ instrs.second };
		// add label
		if (m_jump_targets.find(offset) != end(m_jump_targets))
			result.push_back(fi::AsmToken(std::format("{}:", get_next_label(offset, last_label, labels)), 1, true));
		// add .textbox if script starts here
		if (instr.type == fi::Instruction_type::Directive) {
			result.push_back(fi::AsmToken(".textbox", 3, false));
			result.push_back(fi::AsmToken(get_define(fi::ArgDomain::TextBox, instr.opcode_byte), 0, true));
		}
		// any other opcode
		else {
			const auto& op{ fi::opcodes.find(instr.opcode_byte)->second };
			result.push_back(fi::AsmToken(std::format("  {}", op.name), 4, false));

			// text commands - show string or index
			if (op.domain == fi::ArgDomain::TextString) {
				std::size_t str_ind{ static_cast<std::size_t>(instr.operand.value()) };
				const auto opval{ instr.operand.value() };

				if (str_ind == 0 || str_ind > m_strings.size()) {
					result.push_back(fi::AsmToken(get_define(op.domain, static_cast<byte>(opval)), 5, false));
					result.push_back(fi::AsmToken("; undefined string index", 2, true));
				}
				else
					result.push_back(fi::AsmToken(
						std::format("\"{}\"", m_strings[opval - 1].get_string())
						, 3, true));
			}
			else if (op.arg_type != fi::ArgType::None) {
				const auto opval{ instr.operand.value() };
				if (op.arg_type == fi::ArgType::Byte)
					result.push_back(fi::AsmToken(get_define(op.domain, static_cast<byte>(opval)), 5, false));
				else
					result.push_back(fi::AsmToken(std::format("{}", opval), 5, false));

				// extract possible setspawns here to use for
				// spawn point deduction later
				if (instr.opcode_byte == c::OPCODE_SET_SPAWN)
					m_spawn_scripts[static_cast<byte>(opval)] = static_cast<byte>(p_script_no);
			}

			if (op.flow == fi::Flow::Jump) {
				result.push_back(fi::AsmToken(get_next_label(instr.jump_target.value(), last_label, labels), 1, true));
			}
			else if (op.flow == fi::Flow::Read) {
				const auto opval{ instr.operand.value() };

				if (opval >= m_shops.size())
					result.push_back(fi::AsmToken("; invalid shop index", 2, true));
				else {
					result.push_back(fi::AsmToken(std::format("{}", opval), 5, false));
					result.push_back(fi::AsmToken(std::format("; {}",
						serialize_shop_as_string(m_shops[opval])), 2, true));
				}
			}
		}

		result.back().newline = true;
	}

	return result;
}

std::string fi::IScriptLoader::get_define(fi::ArgDomain domain, byte arg) const {
	if (domain == fi::ArgDomain::Item)
		return get_define(m_defines_item, arg);
	else if (domain == fi::ArgDomain::Quest)
		return get_define(m_defines_quest, arg);
	else if (domain == fi::ArgDomain::Rank)
		return get_define(m_defines_rank, arg);
	else if (domain == fi::ArgDomain::TextBox)
		return get_define(m_defines_textbox, arg);
	else
		return std::format("{}", arg);
}

std::string fi::IScriptLoader::get_define(const std::map<byte, std::string>& p_map, byte arg) const {
	const auto iter{ p_map.find(arg) };
	if (iter != end(p_map))
		return iter->second;
	else
		return std::format("${:02x}", arg);
}

std::string fi::IScriptLoader::serialize_shop_as_string(const fi::Shop& p_shop) const {
	std::string l_shop_string;

	for (std::size_t j{ 0 }; j < p_shop.m_entries.size(); ++j) {
		l_shop_string += std::format("({} {})",
			get_define(fi::ArgDomain::Item, p_shop.m_entries[j].m_item),
			p_shop.m_entries[j].m_price);
		if (j != p_shop.m_entries.size() - 1)
			l_shop_string.push_back(' ');
	}

	return l_shop_string;
}
