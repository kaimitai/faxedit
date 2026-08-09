#ifndef FB_BSCRIPTLOADER_H
#define FB_BSCRIPTLOADER_H

#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include "fe/Config.h"
#include "BScriptOpcode.h"

using byte = unsigned char;

namespace fb {

	class BScriptLoader {
		// TODO: Change visibility
	public:
		std::vector<byte> m_rom;
		std::size_t m_bscript_count;
		std::pair<std::size_t, std::size_t> m_bscript_ptr;
		std::vector<std::size_t> m_ptr_table;

		std::map<byte, fb::BScriptOpcode> opcodes;
		std::map<byte, fb::BScriptOpcode> behavior_ops;

		std::map<std::size_t, fb::BScriptInstruction> m_instrs;
		std::set<std::size_t> m_jump_targets;

		std::size_t m_rg2_rom_offset;

		void parse_blob_from_entrypoint(std::size_t p_ep, std::size_t p_zero_addr);
		uint16_t read_short(std::size_t& offset) const;
		byte read_byte(std::size_t& offset) const;

	public:
		BScriptLoader(const fe::Config& p_config,
			const std::vector<byte>& p_rom);
		void parse_rom(void);

	};

}

#endif
