#include "HackManager.h"
#include "fh_constants.h"
#include "fe/ROM_Manager.h"
#include "fi/fi_constants.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>

// sram/bank/addr placement for the bank 15 general hacks. place_hack checks
// and picks the address, install_placed writes the code there, and far_entry
// gives bank 15 an address to call when the code sits in another bank
// (far_entry_nmi when the caller is the nmi).

namespace {
	constexpr byte FIXED_BANK{ 15 };
	constexpr byte ValueIO{ fh::RAM::ZP_e5 };
	// the larger of the two trampolines; a reservation fits either
	constexpr std::size_t TRAMPOLINE_SIZE{ 26 };
	// find_trailing_free_cpu_addr starts this far into a trailing run of $ff
	constexpr std::size_t FREE_CUSHION{ 16 };

	void require_free(const std::vector<byte>& p_rom, const std::string& p_name, byte p_bank,
		word p_addr, std::size_t p_size) {
		const auto off{ klib::Asm6502::get_file_offset(p_bank, p_addr) };
		for (std::size_t i{ 0 }; i < p_size; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
				throw std::runtime_error(std::format("{}: bank {} space at ${:04x} is not free",
					p_name, p_bank, p_addr + i));
	}
}

fh::HackPlacement fh::HackManager::place_hack(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const fh::GeneralHack& p_hack, const std::string& p_name, word cpu_addr, std::size_t p_size,
	std::size_t p_far_entries, byte p_default_bank) const {
	HackPlacement result;
	result.sram = p_hack.bool_or("sram", false);
	result.size = p_size;

	if (cpu_addr < 0xc000)
		throw std::runtime_error(std::format("{}: only for bank 15 hacks", p_name));

	if (result.sram) {
		if (p_hack.has_param("bank") || p_hack.has_param("addr"))
			throw std::runtime_error(std::format("{}: sram cannot be combined with bank or addr", p_name));
		if (!sram_state)
			throw std::runtime_error(std::format(
				"{}: sram=true requires a rom with sram support, or the SRAM hack", p_name));

		// same limits as install_sram_hack, before anything is written
		const auto& state{ *sram_state };
		const std::size_t rom_end{ state.rom_bank == 15 || state.rom_bank == 31 ? 0x10000u : 0xc000u };
		if (static_cast<std::size_t>(state.sram_cursor) + p_size > 0x8000
			|| static_cast<std::size_t>(state.rom_cursor) + p_size > rom_end)
			throw std::runtime_error(std::format("{}: {} bytes do not fit the sram code area", p_name, p_size));

		result.bank = state.rom_bank;
		result.addr = sram_hack_addr();
		return result;
	}

	result.bank = p_hack.byte_or("bank", p_default_bank);

	if (result.bank == FIXED_BANK) {
		if (p_hack.has_param("addr"))
			throw std::runtime_error(std::format("{}: addr needs a switchable bank", p_name));
		result.addr = cpu_addr;
		require_free(p_rom, p_name, FIXED_BANK, cpu_addr, p_size);
		return result;
	}

	// the last bank is the fixed one
	const std::size_t bank_count{ p_rom[4] };
	if (result.bank + 1u >= bank_count)
		throw std::runtime_error(std::format("{}: bank must be 15 or a switchable bank below {}",
			p_name, bank_count - 1));

	// script assembly rewrites these after the hacks are installed
	const auto string_banks{ p_config.vset_as_set(fi::c::ID_STRING_BANKS) };
	if (result.bank == 12 || result.bank == 14 || string_banks.contains(result.bank))
		throw std::runtime_error(std::format("{}: bank {} is rewritten when scripts are assembled",
			p_name, result.bank));

	result.addr = p_hack.has_param("addr") ? p_hack.get_word("addr") :
		fe::ROM_Manager::find_trailing_free_cpu_addr(p_rom, result.bank, 0xff, FREE_CUSHION);
	if (result.addr < 0x8000 || result.addr + p_size > 0xc000)
		throw std::runtime_error(std::format("{}: {} bytes at ${:04x} do not fit bank {}",
			p_name, p_size, result.addr, result.bank));
	require_free(p_rom, p_name, result.bank, result.addr, p_size);

	// the sram code area is still empty here; sram hacks later in the list fill it
	if (sram_state && result.bank == sram_state->rom_bank) {
		const std::size_t area_end{ sram_state->rom_begin + (0x8000u - sram_state->sram_begin) };
		if (result.addr + p_size > sram_state->rom_begin && result.addr < area_end)
			throw std::runtime_error(std::format(
				"{}: bank {} at ${:04x} runs into the sram code area at ${:04x}; give addr, or use sram=true",
				p_name, result.bank, result.addr, sram_state->rom_begin));
	}

	require_free(p_rom, p_name, FIXED_BANK, cpu_addr, p_far_entries * TRAMPOLINE_SIZE);
	result.far_entries = p_far_entries;
	return result;
}

word fh::HackManager::install_placed(std::vector<byte>& p_rom, klib::Asm6502& p_code,
	const HackPlacement& p_place, word cpu_addr, const std::string& p_name) {
	if (p_code.size() > p_place.size)
		throw std::runtime_error(std::format("{}: {} bytes of code, {} placed", p_name, p_code.size(),
			p_place.size));

	// the code must still go where it was placed
	const word target{ p_place.sram ? sram_hack_addr() : p_place.bank == FIXED_BANK ? cpu_addr : p_place.addr };
	if (target != p_place.addr)
		throw std::runtime_error(std::format("{}: placed at ${:04x}, but the code would go at ${:04x}",
			p_name, p_place.addr, target));

	if (p_place.sram) {
		install_sram_hack(p_rom, p_code);
		return cpu_addr;
	}
	require_free(p_rom, p_name, p_place.bank, p_place.addr, p_code.size());
	if (p_place.bank == FIXED_BANK)
		return p_code.apply_hack_and_clear_get_next_cpu_addr(p_rom, FIXED_BANK, cpu_addr);

	// too much $ff at the end looks like free space to the next hack in this bank
	auto resolved{ p_rom };
	p_code.apply_hack_noclear(resolved, p_place.bank, p_place.addr);
	const auto end{ klib::Asm6502::get_file_offset(p_place.bank, p_place.addr) + p_code.size() };
	std::size_t tail{ 0 };
	while (tail < p_code.size() && resolved[end - 1 - tail] == 0xff)
		++tail;
	if (tail > FREE_CUSHION)
		throw std::runtime_error(std::format("{}: the code ends in {} $ff bytes, which read as free space",
			p_name, tail));

	p_code.apply_hack_and_clear(p_rom, p_place.bank, p_place.addr);
	return cpu_addr;
}

// a and y go in and come back, flags from a. x does not survive, and $e5 is
// used. not from the nmi
word fh::HackManager::far_entry(const fe::Config& p_config, std::vector<byte>& p_rom,
	HackPlacement& p_place, word p_entry, word& cpu_addr, const std::string& p_name) const {
	if (p_place.sram || p_place.bank == FIXED_BANK)
		return p_entry;
	if (p_place.far_entries == 0)
		throw std::runtime_error(std::format("{}: more far entries than were placed", p_name));
	--p_place.far_entries;

	const word update_rom_bank{ cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK) };
	klib::Asm6502 trampoline;
	trampoline.sta_zp(ValueIO);
	// save currently mapped switchable bank
	trampoline.lda_abs(RAM::CurrentROMBank);
	trampoline.pha();
	trampoline.ldx_imm(p_place.bank);
	trampoline.jsr(update_rom_bank);
	trampoline.lda_zp(ValueIO);
	trampoline.jsr(p_entry);
	trampoline.sta_zp(ValueIO);
	// restore previous bank
	trampoline.pla();
	trampoline.tax();
	trampoline.jsr(update_rom_bank);
	trampoline.lda_zp(ValueIO);
	trampoline.rts();

	const word result{ cpu_addr };
	cpu_addr = trampoline.apply_hack_and_clear_get_next_cpu_addr(p_rom, FIXED_BANK, cpu_addr);
	return result;
}

// the vanilla nmi's own bank switch: MMC1_EnsurePRG tells an interrupted
// MMC1_UpdateROMBank to redo its write. only from the nmi, since nothing
// would redo this one. y goes in and comes back; a, x and the flags do not.
// MMC1_EnsurePRG has no config entry; this is the US, EU and JP address
word fh::HackManager::far_entry_nmi(std::vector<byte>& p_rom, HackPlacement& p_place, word p_entry,
	word& cpu_addr, const std::string& p_name) const {
	if (p_place.sram || p_place.bank == FIXED_BANK)
		return p_entry;
	if (p_place.far_entries == 0)
		throw std::runtime_error(std::format("{}: more far entries than were placed", p_name));
	--p_place.far_entries;

	klib::Asm6502 trampoline;
	trampoline.lda_abs(RAM::CurrentROMBank);
	trampoline.pha();
	trampoline.ldx_imm(p_place.bank);
	trampoline.jsr(ROM::MMC1_EnsurePRG);
	trampoline.jsr(p_entry);
	trampoline.pla();
	trampoline.tax();
	trampoline.jsr(ROM::MMC1_EnsurePRG);
	trampoline.rts();

	const word result{ cpu_addr };
	cpu_addr = trampoline.apply_hack_and_clear_get_next_cpu_addr(p_rom, FIXED_BANK, cpu_addr);
	return result;
}
