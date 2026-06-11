#include "UndoInterface.h"
#include <cstdint>

fe::UndoInterface::UndoInterface(fe::Game& p_game) :
	game{ p_game }
{
}

// pre-process step for multi-metatile update (clipboard paste)
void fe::UndoInterface::apply_tilemap_edit(std::size_t p_world_no, std::size_t p_screen_no,
	std::size_t p_x, std::size_t p_y,
	const std::vector<std::vector<byte>>& p_data) {
	const auto& scr{ game.m_chunks[p_world_no].m_screens[p_screen_no] };

	// trim the clipboard data down to what is actually different
	std::vector<Coord> pos;
	std::vector<byte> data;
	for (std::size_t y{ 0 }; y < p_data.size(); ++y)
		for (std::size_t x{ 0 }; x < p_data[y].size(); ++x)
			if (scr.get_mt_at_pos(p_x + x, p_y + y) != p_data[y][x]) {
				pos.push_back(std::make_pair(p_x + x, p_y + y));
				data.push_back(p_data[y][x]);
			}

	// only make an undo step if there are any changes
	if (!pos.empty())
		apply_edit(make_key(p_world_no, p_screen_no), pos, data);
}

// pre-process step for single metatile update
void fe::UndoInterface::apply_tilemap_edit(std::size_t p_world_no, std::size_t p_screen_no,
	std::size_t p_x, std::size_t p_y, byte p_value) {
	const auto& scr{ game.m_chunks[p_world_no].m_screens[p_screen_no] };

	// only add an undo entry if the value is different
	if (scr.get_mt_at_pos(p_x, p_y) != p_value) {
		std::vector<Coord> pos{ std::make_pair(p_x, p_y) };
		std::vector<byte> data{ p_value };
		apply_edit(make_key(p_world_no, p_screen_no), pos, data);
	}
}

bool fe::UndoInterface::apply_metatile_edit(std::size_t p_world_no, std::size_t p_metatile_no,
	std::size_t p_x, std::size_t p_y, byte p_value) {
	auto& mt_tilemap{ game.m_chunks[p_world_no].m_metatiles[p_metatile_no].m_tilemap };

	// only add an undo entry if the value is different
	byte old_val{ mt_tilemap.at(p_y).at(p_x) };
	if (old_val != p_value) {
		const auto key{ make_key(p_world_no, p_metatile_no) };
		TilemapEdit edit;

		edit.positions = { std::make_pair(p_x, p_y) };
		edit.after = { p_value };
		edit.before = { old_val };

		// apply new value
		mt_tilemap.at(p_y).at(p_x) = p_value;

		// Push to undo stack
		auto& undoStack = m_metatile_undo[key];
		undoStack.push_back(std::move(edit));
		trim_history(undoStack);

		// New edit invalidates redo history
		m_metatile_redo[key].clear();

		return true;
	}

	return false;
}

bool fe::UndoInterface::has_metatile_undo(std::size_t p_world_no, std::size_t p_metatile_no) const {
	const auto key{ make_key(p_world_no, p_metatile_no) };
	return m_metatile_undo.contains(key) && !m_metatile_undo.at(key).empty();
}

bool fe::UndoInterface::has_metatile_redo(std::size_t p_world_no, std::size_t p_metatile_no) const {
	const auto key{ make_key(p_world_no, p_metatile_no) };
	return m_metatile_redo.contains(key) && !m_metatile_redo.at(key).empty();
}

void fe::UndoInterface::apply_edit(const Key& p_key, const std::vector<Coord>& positions,
	const std::vector<byte>& p_data) {
	TilemapEdit edit;
	edit.positions = positions;
	edit.after = p_data;
	edit.before.reserve(positions.size());

	// Capture "before" values from the game
	auto& scr{ game.m_chunks[p_key.first].m_screens[p_key.second] };
	for (std::size_t i = 0; i < positions.size(); ++i) {
		uint8_t oldValue = scr.get_mt_at_pos(positions[i].first, positions[i].second);
		edit.before.push_back(oldValue);
	}

	// Apply new values to the game
	for (std::size_t i = 0; i < positions.size(); ++i)
		scr.m_tilemap[positions[i].second][positions[i].first] = p_data[i];

	// Push to undo stack
	auto& undoStack = m_undo[p_key];
	undoStack.push_back(std::move(edit));
	trim_history(undoStack);

	// New edit invalidates redo history
	m_redo[p_key].clear();
}

bool fe::UndoInterface::undo(std::size_t p_world, std::size_t p_screen) {
	Key key = make_key(p_world, p_screen);
	auto it = m_undo.find(key);
	if (it == m_undo.end() || it->second.empty())
		return false;

	auto& undoStack = it->second;
	TilemapEdit edit = std::move(undoStack.back());
	undoStack.pop_back();

	// Apply "before" values
	auto& scr{ game.m_chunks[key.first].m_screens[key.second] };
	for (std::size_t i = 0; i < edit.positions.size(); ++i)
		scr.m_tilemap[edit.positions[i].second][edit.positions[i].first] = edit.before[i];

	// Move this edit to the redo stack
	m_redo[key].push_back(std::move(edit));
	return true;
}

bool fe::UndoInterface::redo(std::size_t p_world, std::size_t p_screen) {
	Key key = make_key(p_world, p_screen);
	auto it = m_redo.find(key);
	if (it == m_redo.end() || it->second.empty())
		return false;

	auto& redoStack = it->second;
	TilemapEdit edit = std::move(redoStack.back());
	redoStack.pop_back();

	// Apply "after" values
	auto& scr{ game.m_chunks[key.first].m_screens[key.second] };
	for (std::size_t i = 0; i < edit.positions.size(); ++i) {
		scr.m_tilemap[edit.positions[i].second][edit.positions[i].first] = edit.after[i];
	}

	// Move this edit back to the undo stack
	m_undo[key].push_back(std::move(edit));
	return true;
}

bool fe::UndoInterface::undo_metatile(std::size_t p_world, std::size_t p_metatile) {
	Key key = make_key(p_world, p_metatile);
	auto it = m_metatile_undo.find(key);
	if (it == m_metatile_undo.end() || it->second.empty())
		return false;

	auto& undoStack = it->second;
	TilemapEdit edit = std::move(undoStack.back());
	undoStack.pop_back();

	// Apply "before" values
	auto& mt{ game.m_chunks[key.first].m_metatiles[key.second] };
	for (std::size_t i = 0; i < edit.positions.size(); ++i)
		mt.m_tilemap[edit.positions[i].second][edit.positions[i].first] = edit.before[i];

	// Move this edit to the redo stack
	m_metatile_redo[key].push_back(std::move(edit));
	return true;
}

bool fe::UndoInterface::redo_metatile(std::size_t p_world, std::size_t p_metatile) {
	Key key = make_key(p_world, p_metatile);
	auto it = m_metatile_redo.find(key);
	if (it == m_metatile_redo.end() || it->second.empty())
		return false;

	auto& redoStack = it->second;
	TilemapEdit edit = std::move(redoStack.back());
	redoStack.pop_back();

	// Apply "after" values
	auto& mt{ game.m_chunks[key.first].m_metatiles[key.second] };
	for (std::size_t i = 0; i < edit.positions.size(); ++i) {
		mt.m_tilemap[edit.positions[i].second][edit.positions[i].first] = edit.after[i];
	}

	// Move this edit back to the undo stack
	m_metatile_undo[key].push_back(std::move(edit));
	return true;
}

Key fe::UndoInterface::make_key(std::size_t p_world_no,
	std::size_t p_screen_no) const {
	return std::make_pair(p_world_no, p_screen_no);
}

// clear all history for all types
void fe::UndoInterface::clear_history(void) {
	m_undo.clear();
	m_redo.clear();
	m_palette_undo.clear();
	m_palette_redo.clear();
	clear_metatile_history();
}

// clear all undo and redo for a world
void fe::UndoInterface::clear_history(std::size_t p_world_no) {
	for (auto& kv : m_undo)
		if (kv.first.first == p_world_no)
			kv.second.clear();
	for (auto& kv : m_redo)
		if (kv.first.first == p_world_no)
			kv.second.clear();

	clear_metatile_history(p_world_no);
}

void fe::UndoInterface::clear_metatile_history(void) {
	m_metatile_undo.clear();
	m_metatile_redo.clear();
}

void fe::UndoInterface::clear_metatile_history(std::size_t p_world_no) {
	for (auto& kv : m_metatile_undo)
		if (kv.first.first == p_world_no)
			kv.second.clear();
	for (auto& kv : m_metatile_redo)
		if (kv.first.first == p_world_no)
			kv.second.clear();
}

void fe::UndoInterface::trim_history(std::vector<TilemapEdit>& p_stack) {
	while (p_stack.size() > c::UNDO_HISTORY_SIZE)
		p_stack.erase(begin(p_stack));
}

// apply a palette edit - we know all the incoming values are different from the current
// due to pre-processing
void fe::UndoInterface::apply_palette_edit(std::size_t p_pal_key, Palette& p_palette,
	const std::vector<std::size_t>& p_indexes, const std::vector<byte>& p_values) {
	PaletteEdit edit;
	edit.indexes = p_indexes;
	edit.after = p_values;
	edit.before.reserve(p_indexes.size());

	// Capture "before" values from the palette
	for (std::size_t i = 0; i < p_indexes.size(); ++i) {
		byte oldValue = p_palette[p_indexes[i]];
		edit.before.push_back(oldValue);
	}

	// Apply new values to the palette
	for (std::size_t i = 0; i < p_indexes.size(); ++i)
		p_palette[p_indexes[i]] = p_values[i];

	// Push to undo stack
	auto& undoStack = m_palette_undo[p_pal_key];
	undoStack.push_back(std::move(edit));
	trim_palette_history(undoStack);

	// New edit invalidates redo history
	m_palette_redo[p_pal_key].clear();
}

// pre-process for single color update
bool fe::UndoInterface::apply_palette_edit(std::size_t p_pal_key, Palette& p_palette,
	std::size_t p_index, byte p_value) {
	if (p_palette.at(p_index) != p_value) {
		std::vector<std::size_t> idxs{ p_index };
		std::vector<byte> vals{ p_value };
		apply_palette_edit(p_pal_key, p_palette, idxs, vals);
		return true;
	}
	else
		return false;
}

// pre-process for multi colors update (clipboard paste/bg color update)
bool fe::UndoInterface::apply_palette_edit(std::size_t p_pal_key, Palette& p_palette,
	const Palette& p_new_palette) {
	std::vector<std::size_t> idxs;
	std::vector<byte> vals;

	for (std::size_t i{ 0 }; i < p_palette.size(); ++i)
		if (p_palette[i] != p_new_palette.at(i)) {
			idxs.push_back(i);
			vals.push_back(p_new_palette.at(i));
		}

	if (!idxs.empty()) {
		apply_palette_edit(p_pal_key, p_palette, idxs, vals);
		return true;
	}
	else
		return false;
}

// palette undo and redo
bool fe::UndoInterface::undo_palette(std::size_t p_pal_key, Palette& p_palette) {
	auto it = m_palette_undo.find(p_pal_key);
	if (it == m_palette_undo.end() || it->second.empty())
		return false;

	auto& undoStack = it->second;
	PaletteEdit edit = std::move(undoStack.back());
	undoStack.pop_back();

	// Apply "before" values
	for (std::size_t i = 0; i < edit.indexes.size(); ++i)
		p_palette[edit.indexes[i]] = edit.before[i];

	// Move this edit to the redo stack
	m_palette_redo[p_pal_key].push_back(std::move(edit));
	return true;
}

bool fe::UndoInterface::redo_palette(std::size_t p_pal_key, Palette& p_palette) {
	auto it = m_palette_redo.find(p_pal_key);
	if (it == m_palette_redo.end() || it->second.empty())
		return false;

	auto& redoStack = it->second;
	PaletteEdit edit = std::move(redoStack.back());
	redoStack.pop_back();

	// Apply "after" values
	for (std::size_t i = 0; i < edit.indexes.size(); ++i) {
		p_palette[edit.indexes[i]] = edit.after[i];
	}

	// Move this edit back to the undo stack
	m_palette_undo[p_pal_key].push_back(std::move(edit));
	return true;
}

// palette editing history
void fe::UndoInterface::clear_palette_history(void) {
	m_palette_undo.clear();
	m_palette_redo.clear();
}

void fe::UndoInterface::trim_palette_history(std::vector<PaletteEdit>& p_stack) {
	while (p_stack.size() > c::UNDO_HISTORY_SIZE)
		p_stack.erase(begin(p_stack));
}

bool fe::UndoInterface::has_palette_undo(std::size_t p_pal_key) const {
	return m_palette_undo.contains(p_pal_key) &&
		!m_palette_undo.at(p_pal_key).empty();
}

bool fe::UndoInterface::has_palette_redo(std::size_t p_pal_key) const {
	return m_palette_redo.contains(p_pal_key) &&
		!m_palette_redo.at(p_pal_key).empty();
}
