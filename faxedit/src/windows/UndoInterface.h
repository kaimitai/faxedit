#ifndef FE_UNDO_INTERFACE_H
#define FE_UNDO_INTERFACE_H

#include <map>
#include <utility>
#include <vector>
#include "./../fe/Game.h"

using Coord = std::pair<std::size_t, std::size_t>; // metatile x, y
using Key = std::pair<std::size_t, std::size_t>;   // world, screen
using byte = unsigned char;

namespace fe {

	struct TilemapEdit {
		std::vector<Coord> positions;
		std::vector<byte> before;
		std::vector<byte> after;
	};

	class UndoInterface {

		fe::Game& game;
		std::map<Key, std::vector<TilemapEdit>> m_undo;
		std::map<Key, std::vector<TilemapEdit>> m_redo;

		void trim_history(std::vector<TilemapEdit>& p_stack);
		inline Key make_key(std::size_t p_world_no, std::size_t p_screen_no) const;

		void apply_edit(const Key& p_key, const std::vector<Coord>& positions,
			const std::vector<byte>& p_data);

	public:
		explicit UndoInterface(fe::Game& p_game);
		bool undo(std::size_t p_world_no, std::size_t p_screen_no);
		bool redo(std::size_t p_world_no, std::size_t p_screen_no);
		void clear_history(void);
		void clear_history(std::size_t p_world_no);

		void apply_tilemap_edit(std::size_t p_world_no, std::size_t p_screen_no,
			std::size_t p_x, std::size_t p_y, byte p_value);
		void apply_tilemap_edit(std::size_t p_world_no, std::size_t p_screen_no,
			std::size_t p_x, std::size_t p_y, const std::vector<std::vector<byte>>& p_data);
	};

	namespace c {
		constexpr std::size_t UNDO_HISTORY_SIZE{ 250 };
	}

}

#endif
