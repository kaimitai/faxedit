#include "BScriptLoader.h"
#include "fb_constants.h"
#include "BScriptOpcode.h"
#include <format>
#include <stdexcept>

fb::BScriptLoader::BScriptLoader(const fe::Config& p_config,
	const std::vector<byte>& p_rom) :
	m_rom{ p_rom },
	m_bscript_ptr{ p_config.pointer(c::ID_BSCRIPT_PTR) },
	m_bscript_count{ p_config.constant(c::ID_SPRITE_COUNT) },
	m_rg2_rom_offset { p_config.constant(c::ID_BSCRIPT_RG2_START) },
	opcodes{ fb::parse_opcodes(p_config.bmap(c::ID_BSCRIPT_OPCODES)) },
	behavior_ops{ fb::parse_opcodes(p_config.bmap(c::ID_BSCRIPT_BEHAVIORS)) }
{
	// extract ptr table
	for (std::size_t i{ 0 }; i < m_bscript_count; ++i)
		m_ptr_table.push_back(static_cast<std::size_t>(m_rom.at(m_bscript_ptr.first + 2 * i))
			+ 256 * static_cast<std::size_t>(m_rom.at(m_bscript_ptr.first + 2 * i + 1))
			+ m_bscript_ptr.second);
}

void fb::BScriptLoader::parse_rom(void) {
	for (std::size_t ep : m_ptr_table)
		parse_blob_from_entrypoint(ep, m_bscript_ptr.second);
}

void fb::BScriptLoader::parse_blob_from_entrypoint(std::size_t offset, std::size_t p_zero_addr) {
	std::size_t cursor = offset;

	while (cursor < m_rom.size()) {
		if (m_instrs.find(cursor) != end(m_instrs))
			return;

		std::size_t instr_offset{ cursor };
		byte opcode_byte{ read_byte(cursor) };
		std::optional<byte> behavior_op;

		if (opcode_byte == c::OPCODE_BEHAVIOR) {
			behavior_op = read_byte(cursor);
			if (!behavior_ops.contains(behavior_op.value()))
				throw std::runtime_error(
					std::format("Unknown behavior code: ${:02x} at ROM offset 0x{:06x}", behavior_op.value(), cursor)
				);
		}
		else if (!opcodes.contains(opcode_byte)) {
			throw std::runtime_error(
				std::format("Unknown opcode: ${:02x} at ROM offset 0x{:06x}", opcode_byte, cursor)
			);
		}

		const auto& opcode{ behavior_op ? behavior_ops.at(behavior_op.value()) :
		opcodes.at(opcode_byte) };

		std::vector<fb::ArgInstance> operands;
		for (const auto& templarg : opcode.args) {
			if (templarg.data_type == fb::ArgDataType::Byte)
				operands.push_back(fb::ArgInstance(fb::ArgDataType::Byte, read_byte(cursor)));
			else if (templarg.domain == fb::ArgDomain::Addr ||
				templarg.domain == fb::ArgDomain::TrueAddr ||
				templarg.domain == fb::ArgDomain::FalseAddr) {
				std::size_t jumptarget{ p_zero_addr + read_short(cursor) };
				m_jump_targets.insert(jumptarget);
				operands.push_back(fb::ArgInstance(fb::ArgDataType::Word, jumptarget));
			}
			else
				operands.push_back(fb::ArgInstance(fb::ArgDataType::Word, read_short(cursor)));
		}

		auto instr{ fb::BScriptInstruction(opcode_byte, behavior_op, operands) };
		m_instrs.insert(std::make_pair(instr_offset, instr));

		// take all jumps and parse them as entrypoints
		for (std::size_t i{ 0 }; i < opcode.args.size(); ++i) {
			if (opcode.args[i].domain == fb::ArgDomain::Addr ||
				opcode.args[i].domain == fb::ArgDomain::TrueAddr ||
				opcode.args[i].domain == fb::ArgDomain::FalseAddr) {
				std::size_t temp_cursor{ cursor };
				cursor = instr.operands[i].data_value.value();

				parse_blob_from_entrypoint(cursor, p_zero_addr);

				cursor = temp_cursor;
			}
		}

		if (opcode.flow == fb::Flow::End || opcode.flow == fb::Flow::Jump)
			break;

	}
}

byte fb::BScriptLoader::read_byte(std::size_t& offset) const {
	return m_rom.at(offset++);
}

uint16_t fb::BScriptLoader::read_short(std::size_t& offset) const {
	byte lo = read_byte(offset);
	byte hi = read_byte(offset);
	return static_cast<uint16_t>(hi << 8 | lo);
}
