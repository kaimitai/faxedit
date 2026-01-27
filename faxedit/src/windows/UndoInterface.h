#ifndef FE_UNDO_INTERFACE_H
#define FE_UNDO_INTERFACE_H

#include <map>
#include <utility>
#include <vector>
#include "./../fe/Game.h"

// tilemap edits
using Coord = std::pair<std::size_t, std::size_t>; // metatile x, y
using Key = std::pair<std::size_t, std::size_t>;   // world, screen
using byte = unsigned char;
// palette edits
using Palette = std::vector<byte>;

namespace fe {

	struct TilemapEdit {
		std::vector<Coord> positions;
		std::vector<byte> before;
		std::vector<byte> after;
	};

	struct PaletteEdit {
		std::vector<std::size_t> indexes;
		std::vector<byte> before;
		std::vector<byte> after;
	};

	class UndoInterface {

		fe::Game& game;

		// tilemap edits
		std::map<Key, std::vector<TilemapEdit>> m_undo;
		std::map<Key, std::vector<TilemapEdit>> m_redo;
		// palette edits
		std::map<std::size_t, std::vector<PaletteEdit>> m_palette_undo;
		std::map<std::size_t, std::vector<PaletteEdit>> m_palette_redo;

		// tilemap edits
		void trim_history(std::vector<TilemapEdit>& p_stack);
		inline Key make_key(std::size_t p_world_no, std::size_t p_screen_no) const;
		void apply_edit(const Key& p_key, const std::vector<Coord>& positions,
			const std::vector<byte>& p_data);
		// palette edits
		void trim_palette_history(std::vector<PaletteEdit>& p_stack);
		void apply_palette_edit(std::size_t p_pal_key, Palette& p_palette,
			const std::vector<std::size_t>& p_indexes, const std::vector<byte>& p_values);

	public:
		explicit UndoInterface(fe::Game& p_game);

		// tilemap edits
		bool undo(std::size_t p_world_no, std::size_t p_screen_no);
		bool redo(std::size_t p_world_no, std::size_t p_screen_no);
		void clear_history(void);
		void clear_history(std::size_t p_world_no);

		void apply_tilemap_edit(std::size_t p_world_no, std::size_t p_screen_no,
			std::size_t p_x, std::size_t p_y, byte p_value);
		void apply_tilemap_edit(std::size_t p_world_no, std::size_t p_screen_no,
			std::size_t p_x, std::size_t p_y, const std::vector<std::vector<byte>>& p_data);

		// palette edits
		bool undo_palette(std::size_t p_pal_key, Palette& p_palette);
		bool redo_palette(std::size_t p_pal_key, Palette& p_palette);
		void clear_palette_history(void);

		bool apply_palette_edit(std::size_t p_pal_key, Palette& p_palette,
			std::size_t p_index, byte p_value);
		bool apply_palette_edit(std::size_t p_pal_key, Palette& p_palette,
			const Palette& p_new_palette);

		bool has_palette_undo(std::size_t p_pal_key) const;
		bool has_palette_redo(std::size_t p_pal_key) const;
	};

	namespace c {
		constexpr std::size_t UNDO_HISTORY_SIZE{ 250 };
	}

}

#endif
