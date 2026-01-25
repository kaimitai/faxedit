#include "UndoInterface.h"

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

Key fe::UndoInterface::make_key(std::size_t p_world_no,
	std::size_t p_screen_no) const {
	return std::make_pair(p_world_no, p_screen_no);
}

// clear all history
void fe::UndoInterface::clear_history(void) {
	m_undo.clear();
	m_redo.clear();
}

// clear all undo and redo for a world
void fe::UndoInterface::clear_history(std::size_t p_world_no) {
	for (auto& kv : m_undo)
		if (kv.first.first == p_world_no)
			kv.second.clear();

	for (auto& kv : m_redo)
		if (kv.first.first == p_world_no)
			kv.second.clear();
}

void fe::UndoInterface::trim_history(std::vector<TilemapEdit>& p_stack) {
	while (p_stack.size() > c::UNDO_HISTORY_SIZE)
		p_stack.erase(begin(p_stack));
}
