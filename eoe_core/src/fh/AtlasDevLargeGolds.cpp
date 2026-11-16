#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// the stock gold count stops when the high byte is negative. take or give
// $5000 at a time until it is below $80, then let the game count the rest.
// $5000 is a multiple of ten. amounts of 65280 or more do not work.
namespace {
	constexpr byte BANK{ 12 };
	constexpr int MAX_FLAG{ 247 };

	// where each routine keeps its gold bytes and calls; payments have no sound
	struct Routine { const char* name; const char* id; std::size_t ec, ed, step, sound; };
	constexpr Routine SPEND{ "spend", fh::c::ID_ROM_PROGRESSIVELYSUBTRACTGOLD, 33, 37, 39, 0 };
	constexpr Routine ADD{ "add", fh::c::ID_ROM_PROGRESSIVELYADDGOLD, 36, 40, 42, 14 };

	// take the addresses from the rom, since they differ between regions
	void emit(klib::Asm6502& c, const std::vector<byte>& p_rom, const Routine& r, word p_at, int p_flag) {
		const auto base{ klib::Asm6502::get_file_offset(BANK, p_at) };
		auto at = [&](std::size_t off) { return p_rom.at(base + off); };
		auto operand = [&](std::size_t off) { return static_cast<word>(at(off) | at(off + 1) << 8); };
		std::vector<std::pair<std::size_t, byte>> stock{ { 0, 0x20 }, { 3, 0xad }, { 6, 0x30 }, { r.ec - 1, 0x85 }, { r.ed - 1, 0x85 }, { r.step - 1, 0x20 } };
		if (r.sound) stock.push_back({ r.sound - 1, 0x20 });
		for (const auto& [off, op] : stock)
			if (at(off) != op)
				throw std::runtime_error(std::format("AtlasDevLargeGolds: the {} routine at ${:04x} is not vanilla", r.name, p_at));

		const std::string id{ std::string("@") + r.name };
		c.label(id);
		if (p_flag >= 0) {
			c.lda_abs(static_cast<word>(fh::RAM::Flags + (p_flag >> 3)));
			c.and_imm(static_cast<byte>(1 << (p_flag & 7)));
			c.beq(id + "_go");
		}
		c.lda_abs(operand(4)); c.bpl(id + "_go");
		c.cmp_imm(0xff); c.beq(id + "_go");
		c.label(id + "_chunk");
		c.lda_imm(0); c.sta_zp(at(r.ec)); c.lda_imm(0x50); c.sta_zp(at(r.ed));      // 20480
		if (r.sound) c.jsr(operand(r.sound));
		c.jsr(operand(r.step));
		c.lda_abs(operand(4)); c.sec(); c.sbc_imm(0x50); c.sta_abs(operand(4));
		c.bmi(id + "_chunk");                                                      // $80-$fe less $50 is never $ff
		// the call we replaced, then back to the game
		c.label(id + "_go");
		c.jsr(operand(1));
		c.jmp(static_cast<word>(p_at + 3));
	}
}

word fh::HackManager::install_AtlasDevLargeGolds(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) {
	const std::string name{ "AtlasDevLargeGolds" };
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	const bool spend{ p_hack.bool_or("spend", true) }, add{ p_hack.bool_or("add", true) };
	if (!spend && !add)
		throw std::runtime_error(name + ": spend and add are both off, so there is nothing to fix");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = p_hack.word_or("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;
	const bool sram{ p_hack.bool_or("sram", false) };
	if (sram && !sram_state)
		throw std::runtime_error(name + ": sram=true requires a rom with sram support, or the SRAM hack");

	// check both before changing either
	const word spend_at{ cfg_word(p_config, SPEND.id) }, add_at{ cfg_word(p_config, ADD.id) };
	klib::Asm6502 code;
	if (spend) emit(code, p_rom, SPEND, spend_at, flag);
	if (add) emit(code, p_rom, ADD, add_at, flag);
	const std::size_t size{ code.size() };
	if (!sram) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
		for (std::size_t i{ 0 }; i < size; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
				throw std::runtime_error(std::format("{}: bank 12 space at ${:04x} is not free", name, cpu_addr + i));
	}

	// only the hooks stay in bank 12 when the code goes in sram
	const word body{ sram ? sram_hack_addr() : cpu_addr };
	const word spend_code{ static_cast<word>(body + (spend ? code.label_position("@spend") : 0)) };
	const word add_code{ static_cast<word>(body + (add ? code.label_position("@add") : 0)) };
	if (sram) install_sram_hack(p_rom, code);
	else code.apply_hack_and_clear(p_rom, BANK, cpu_addr);

	if (spend) { code.jmp(spend_code); code.apply_hack_and_clear(p_rom, BANK, spend_at); }
	if (add) { code.jmp(add_code); code.apply_hack_and_clear(p_rom, BANK, add_at); }
	return sram ? cpu_addr : static_cast<word>(cpu_addr + size);
}
