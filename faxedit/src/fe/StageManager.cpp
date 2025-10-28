#include "StageManager.h"
#include "fe_constants.h"

// m_world_id, m_prev_stage, m_next_stage, m_prev_screen, m_next_screen,
// m_prev_requirement, m_next_requirement;

fe::StageManager::StageManager(const std::vector<byte>& p_rom_data, std::size_t p_offset_world_mappings,
	std::size_t p_offset_stage_links, std::size_t p_offset_stage_screens, std::size_t p_offset_stage_requirements,
	std::size_t p_offset_start_screen, std::size_t p_offset_start_pos,
	std::size_t p_offset_start_hp) :
	m_start_screen{ p_rom_data.at(p_offset_start_screen) },
	m_start_x{ static_cast<byte>(p_rom_data.at(p_offset_start_pos) % 16) },
	m_start_y{ static_cast<byte>(p_rom_data.at(p_offset_start_pos) / 16) },
	m_start_hp{ p_rom_data.at(p_offset_start_hp) }
{
	// the game has six stages
	for (std::size_t i{ 0 }; i < 6; ++i) {
		m_stages.push_back(fe::Stage(
			p_rom_data.at(p_offset_world_mappings + i),
			p_rom_data.at(p_offset_stage_links + 2 * i),
			p_rom_data.at(p_offset_stage_links + 2 * i + 1),
			p_rom_data.at(p_offset_stage_screens + 2 * i),
			p_rom_data.at(p_offset_stage_screens + 2 * i + 1),
			p_rom_data.at(p_offset_stage_requirements + 2 * i),
			p_rom_data.at(p_offset_stage_requirements + 2 * i + 1))
		);
	}

	recalculate_world_to_stage();
}

fe::StageManager::StageManager(byte p_screen_no, byte p_x, byte p_y, byte p_hp,
	const std::vector<fe::Stage>& p_stages) :
	m_start_x{ p_x }, m_start_y{ p_y }, m_start_screen{ p_screen_no },
	m_start_hp{ p_hp }, m_stages{ p_stages }
{
	recalculate_world_to_stage();
}

fe::StageManager::StageManager(void) :
	m_start_x{ 0x09 }, m_start_y{ 0x05 },
	m_start_screen{ 0x00 }, m_start_hp{ 0x10 }
{
}

void fe::StageManager::recalculate_world_to_stage(void) {
	m_world_to_stage.clear();

	for (std::size_t i{ 0 }; i < m_stages.size(); ++i)
		m_world_to_stage[m_stages[i].m_world_id].push_back(i);
}

void fe::StageManager::set_stage_world(std::size_t p_stage_no, std::size_t p_world_no) {
	m_stages[p_stage_no].m_world_id = p_world_no;

	recalculate_world_to_stage();
}

std::optional<std::size_t> fe::StageManager::get_stage_idx_from_world(std::size_t p_world_no) const {
	const auto iter{ m_world_to_stage.find(p_world_no) };
	if (iter == end(m_world_to_stage) ||
		iter->second.size() > 1)
		return std::nullopt;

	return iter->second.front();
}

std::optional<const fe::Stage*> fe::StageManager::get_stage_from_world(std::size_t p_world_no) const {
	auto idx_opt = get_stage_idx_from_world(p_world_no);
	if (!idx_opt || *idx_opt >= m_stages.size())
		return std::nullopt;

	return &m_stages[*idx_opt];
}
