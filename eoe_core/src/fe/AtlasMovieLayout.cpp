#include "AtlasMovieLayout.h"
#include "common/klib/Asm6502.h"
#include <stdexcept>

std::size_t fe::atlas_movie::layout::file_offset(
	byte p_bank, std::uint16_t p_cpu) {
	if (p_bank >= 16 || p_cpu < 0x8000
		|| (p_bank != 15 && p_cpu >= 0xc000))
		throw std::runtime_error("Atlas movie address is outside PRG ROM");
	// MMC1 can map bank 15 into either PRG window
	const std::uint16_t window{ static_cast<std::uint16_t>(
		p_bank == 15 && p_cpu >= 0xc000 ? 0xc000 : 0x8000) };
	return klib::Asm6502::get_file_offset(p_bank, p_cpu, window);
}
