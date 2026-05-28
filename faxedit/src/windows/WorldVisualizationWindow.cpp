#include "MainWindow.h"
#include "./../fe/WorldVisualizer.h"
#include "./../fi/fi_constants.h"
#include <unordered_map>
#include <unordered_set>

std::unordered_map<byte, fe::ScriptSemanticInfo>
fe::MainWindow::extract_script_semantics(void) try {
	std::unordered_map<byte, fe::ScriptSemanticInfo> result;
	const auto& rom_bytes{ m_game->m_rom_data };

	fi::IScriptLoader loader(m_config, rom_bytes);

	for (std::size_t i{ 0 }; i < loader.get_script_count(); ++i) {
		const auto& instrs{ loader.parse_script_raw(rom_bytes, i) };

		for (const auto& kv : instrs) {
			if (!kv.second.operand)
				continue;

			auto op_value{ *kv.second.operand };

			byte opcode{ kv.second.opcode_byte };

			if (opcode == fi::c::OPCODE_GET_ITEM)
				result[static_cast<byte>(i)].gifts.push_back(
					static_cast<byte>(op_value)
				);
			else if (opcode == fi::c::OPCODE_SHOP_BUY ||
				opcode == fi::c::OPCODE_SHOP_SELL) {
				const auto& shops{ loader.get_shops() };


				if (op_value < shops.size())
					for (const auto& entry : shops[op_value].m_entries)
						result[static_cast<byte>(i)].shop_items.insert(entry.m_item);
			}
		}
	}

	return result;
}
catch (const std::exception& ex) {
	add_message(std::format("Script extraction failed: {}", ex.what()), 1);
	return {};
}
