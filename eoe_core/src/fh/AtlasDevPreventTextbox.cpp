#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <array>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// prevent textbox: a script whose textbox byte is the chosen value runs with
// no window at all. the interpreter opens a box before the first opcode and
// closes it when the script ends, so a script that only moves things or
// changes tiles still flashes a window. with this hack nothing is drawn when
// such a script starts and nothing is erased when it ends; the script itself
// runs as before.
//
// the script's first byte is kept at $0201. the open at $8267 is a jsr to
// $81e2 and the close at $82c2 is a jmp to $81fb. both become calls to a
// short stub in bank 15 that compares $0201 with the value and either goes
// on to the stock routine or returns. both are tail positions, so every
// return address stays where the stock game left it, and a is loaded again
// right after both, so nothing is saved. the stub is 22 bytes and claims no
// ram.
//
// for a plain box the stock game only reads bit 7 of that byte, and its 152
// scripts use only $00 and $80 to $8a, so the value must be 1 to 127: $00 is
// the plain box and bit 7 selects a portrait. the default is $7f. item grants,
// the sell menu and the shops open their own window and still do inside such
// a script. text in such a script runs but cannot be seen.
//
// every site is checked against its vanilla bytes before anything is
// written, and those bytes are identical in the us, us rev a, eu and jp roms.
namespace {
	constexpr byte BANK12{ 12 }, BANK15{ 15 };
	constexpr word OPEN{ 0x8267 }, CLOSE{ 0x82c2 };
	constexpr word ENTRY{ 0x825a }, END{ 0x82b7 };
	constexpr byte DEFAULT_VALUE{ 0x7f };

	// the textbox byte stored, the portrait branch, and the open
	constexpr std::array<byte, 16> ENTRY_ORIG{ 0x8d, 0x01, 0x02, 0x10, 0x08, 0x29, 0x7f, 0x20, 0x4d, 0xf2,
		0x20, 0x1f, 0x82, 0x20, 0xe2, 0x81 };
	// the end of a script: the portrait test, the portrait close, and the close
	constexpr std::array<byte, 14> END_ORIG{ 0xad, 0x01, 0x02, 0x10, 0x06, 0x20, 0x81, 0xf2, 0x4c, 0x2b, 0x82,
		0x4c, 0xfb, 0x81 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK12, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevPreventTextbox(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevPreventTextbox" };
	const byte value{ p_hack.byte_or("textbox", DEFAULT_VALUE) };
	if (value == 0x00 || value > 0x7f)
		throw std::runtime_error(name + ": textbox must be 1 to 127; 0 is the plain box and 128 and up are portraits");

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, ENTRY, ENTRY_ORIG, name);
	require_site(p_rom, END, END_ORIG, name);

	// the open: the stock routine returns to $826a as before
	klib::Asm6502 code;
	code.lda_abs(RAM::IScriptTextBoxContext); code.cmp_imm(value);
	code.beq("@no_open");
	code.jmp(ROM::TextBox_OpenForNPC);
	code.label("@no_open"); code.rts();
	// the close: the stock routine returns to the caller of the script end as before
	const word close{ static_cast<word>(cpu_addr + code.size()) };
	code.lda_abs(RAM::IScriptTextBoxContext); code.cmp_imm(value);
	code.beq("@no_close");
	code.jmp(ROM::TextBox_Close);
	code.label("@no_close"); code.rts();

	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);

	// the jsr stays a jsr and the jmp stays a jmp, at the same address and length:
	// the stock branches at $825d and $82ba land on these two opcodes
	klib::Asm6502 hook;
	hook.jsr(cpu_addr);
	hook.apply_hack_and_clear(p_rom, BANK12, OPEN);
	hook.jmp(close);
	hook.apply_hack_and_clear(p_rom, BANK12, CLOSE);
	return static_cast<word>(cpu_addr + size);
}
