#ifndef FE_STAGE_MANAGER_H
#define FE_STAGE_MANAGER_H

#include <map>
#include <optional>
#include <vector>

using byte = unsigned char;

namespace fe {

	struct Stage {
		std::size_t m_world_id, m_prev_stage, m_next_stage, m_prev_screen, m_next_screen;
		byte m_prev_requirement, m_next_requirement;
	};

	struct StageManager {

		std::vector<fe::Stage> m_stages;
		// internal map for fast reverse lookups: world -> stage id(s)
		std::map<std::size_t, std::vector<std::size_t>> m_world_to_stage;

		void recalculate_world_to_stage(void);

		std::size_t m_start_screen;
		byte m_start_x, m_start_y, m_start_hp;

		StageManager(void);
		StageManager(const std::vector<byte>& p_rom_data, std::size_t p_offset_world_mappings,
			std::size_t p_offset_stage_links, std::size_t p_stage_screens, std::size_t p_offset_requirements,
			std::size_t p_offset_start_screen, std::size_t p_offset_start_pos,
			std::size_t p_offset_start_hp);
		StageManager(byte p_x, byte p_y, byte p_screen_no, byte p_hp,
			const std::vector<fe::Stage>& p_stages);

		void set_stage_world(std::size_t p_stage_no, std::size_t p_world_no);

		std::optional<std::size_t> get_stage_idx_from_world(std::size_t p_world_no) const;
		std::optional<const fe::Stage*> get_stage_from_world(std::size_t p_world_no) const;
	};

}

#endif
