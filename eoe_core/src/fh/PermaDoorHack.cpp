#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Game.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>
#include <vector>

namespace {
	constexpr byte DOOR_KEY_REQ_MIN{ 0x01 };
	constexpr byte DOOR_KEY_REQ_MAX{ 0x05 };
	constexpr byte DOOR_FLAG_MAX{ static_cast<byte>(fh::c::FlagsByteCount * 8 - 1) };

	struct DoorFlag {
		byte screen;
		byte yx;
		byte flag;
	};

	using DoorFlagTable = std::vector<std::vector<DoorFlag>>;

	DoorFlagTable make_door_flag_table(const fe::Game* p_game) {
		DoorFlagTable table(p_game->m_chunks.size());
		std::size_t flag_count{ 0 };

		for (std::size_t world{ 0 }; world < p_game->m_chunks.size(); ++world) {
			const auto stage{ p_game->m_stages.get_stage_from_world(world) };

			for (std::size_t screen{ 0 };
				screen < p_game->m_chunks[world].m_screens.size(); ++screen) {

				for (const auto& door : p_game->m_chunks[world].m_screens[screen].m_doors) {
					byte requirement{};

					if (door.m_door_type == fe::DoorType::NextWorld ||
						door.m_door_type == fe::DoorType::PrevWorld) {
						if (!stage)
							continue; // world belongs to multiple stages -> ambiguous

						requirement = door.m_door_type == fe::DoorType::NextWorld
							? (*stage)->m_next_requirement
							: (*stage)->m_prev_requirement;
					}
					else {
						requirement = static_cast<byte>(door.m_requirement & 0x0f);
					}

					if (requirement < DOOR_KEY_REQ_MIN || requirement > DOOR_KEY_REQ_MAX)
						continue;

					if (flag_count > DOOR_FLAG_MAX)
						throw std::runtime_error("Too many key-locked doors");

					table[world].push_back({
						.screen = static_cast<byte>(screen),
						.yx = static_cast<byte>(
							(door.m_coords.second << 4) | door.m_coords.first),
						.flag = static_cast<byte>(DOOR_FLAG_MAX - flag_count++)
						});
				}
			}
		}

		return table;
	}

	word install_PermaDoors_LookupTable(std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
		const DoorFlagTable& table) {
		klib::Asm6502 code;

		// emit world ptrs
		for (std::size_t world{ 0 }; world < table.size(); ++world)
			code.dw(std::format("door_flags_world_{}", world));

		// emit lookup array per world
		for (std::size_t world{ 0 }; world < table.size(); ++world) {
			code.label(std::format("door_flags_world_{}", world));

			for (const auto& door : table[world]) {
				code.db(door.screen);
				code.db(door.yx);
				code.db(door.flag);
			}

			code.db(0xff);
		}

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);
	}

	word install_PermaDoors_TableWalker(std::vector<byte>& p_rom, byte p_bank, word cpu_addr,
		word table_addr) {
		constexpr byte PTR_LO{ fh::RAM::ZP_e2 };
		constexpr byte PTR_HI{ fh::RAM::ZP_e3 };

		klib::Asm6502 code;

		code.lda_zp(fh::RAM::ZP_CurrentWorld);
		code.asl_a();
		code.tax();

		code.lda_abs_x(table_addr);
		code.sta_zp(PTR_LO);
		code.lda_abs_x(table_addr + 1);
		code.sta_zp(PTR_HI);

		code.ldy_imm(0x00);
		code.label("@next");
		code.lda_ind_y(PTR_LO);
		code.cmp_imm(0xff);
		code.beq("@not_found");

		code.cmp_zp(fh::RAM::ZP_CurrentScreen);
		code.bne("@skip");

		code.iny();
		code.lda_ind_y(PTR_LO);
		code.cmp_zp(fh::RAM::ZP_DoorBlockPos);
		code.beq("@found");

		// screen matched, but Y is now +1
		code.iny();
		code.iny();
		code.jmp("@next");

		code.label("@skip");
		code.iny();
		code.iny();
		code.iny();
		code.jmp("@next");

		code.label("@found");
		code.iny();
		code.lda_ind_y(PTR_LO);
		// flag no stored in A
		code.rts();

		code.label("@not_found");
		code.lda_imm(0xff);
		// $ff denotes no flag selection
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, p_bank, cpu_addr);
	}
}

word fh::HackManager::install_PermaDoors(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack, const fe::Game* p_game) const {
	if (!p_game)
		throw std::runtime_error("PermaDoors: Door data not available");

	klib::Asm6502 code;


	return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 15, cpu_addr);
}
