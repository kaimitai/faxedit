#include "HackManager.h"
#include "common/klib/Asm6502.h"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <string_view>

// sprite speed: clear the sprite buffer with straight stores, and draw each
// visible tile without saving the tile-reader index on the stack. monster
// behavior, collisions, clipping and sprite order are left alone.
namespace {
	using Asm = klib::Asm6502;
	constexpr std::size_t CLEAR_SIZE{ 204 };

	void require_site(const std::vector<byte>& rom, word addr, std::string_view hex) {
		const auto off{ Asm::get_file_offset(15, addr) };
		const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
		if (off > rom.size() || hex.size() / 2 > rom.size() - off)
			throw std::runtime_error("AtlasDevSpriteSpeed: truncated sprite code");
		for (std::size_t i{ 0 }; i < hex.size() / 2; ++i)
			if (rom[off + i] != (nibble(hex[2 * i]) * 16 + nibble(hex[2 * i + 1])))
				throw std::runtime_error(std::format(
					"AtlasDevSpriteSpeed: incompatible sprite code at ${:04x}", addr + i));
	}

	void require_layout(const std::vector<byte>& rom) {
		// bank 15 must be the fixed bank. mirroring, battery and header padding
		// do not select the renderer; its instruction checks do.
		if (rom.size() != 0x40010 || rom[0] != 'N' || rom[1] != 'E'
			|| rom[2] != 'S' || rom[3] != 0x1a || rom[4] != 16 || rom[5] != 0
			|| (rom[6] & 0xfc) != 0x10 || (rom[7] & 0xf0) != 0
			|| ((rom[7] & 0x0c) != 0 && (rom[7] & 0x0c) != 8)
			|| ((rom[7] & 0x0c) == 8 && (rom[8] != 0 || rom[9] != 0)))
			throw std::runtime_error("AtlasDevSpriteSpeed: requires unexpanded MMC1 PRG with CHR RAM and no trainer");
	}

	void require_clear(const std::vector<byte>& rom) {
		// the shared prologue enters the loop with Y=0 or Y=4. the latter
		// leaves sprite zero intact for the HUD split.
		require_site(rom, 0xcb47,
			"a900855a855bf004a9ff855aa0008433843484358438843784398425a55a301be625a8"
			"b996cb8d0007a97f8d0107a9238d0207b998cb8d0307a004a9f0990007c8c8c8c8d0f7"
			"a51c29804980851c60");
	}

	void require_draw(const std::vector<byte>& rom) {
		for (const word addr : { 0xf0f8, 0xf1ac })
			require_site(rom, addr,
				"b13ac9fff064a539d05fa642a53c7d3df28500a53e6900d050a643a53d7d3df28501a53f6900d041");
		require_site(rom, 0xf120,
			"9848a5250a0a451caaa5019d0007e8b13a1865339d0007e8c8b13a45298501a5422901"
			"a8a5263924f2f006a50109208501a5019d0007e8a5009d00072028f268a8");
		require_site(rom, 0xf1d4,
			"9848a5250a0a451caaa5019d0007e8b13a1865339d0007e8c8b13a45298501a5422901"
			"a8a5263926f2f006a50109208501a5019d0007e8a5009d00072028f268a8");
		// blank tiles consume one byte, clipped tiles consume two. neither
		// branch may enter the replacement body. each following CMP #$ff
		// resets carry, and the bank restore defines it again on return.
		require_site(rom, 0xf161, "c8c8e642c640108f688540e643c64130034cf1f068aa201acca9008533852660");
		require_site(rom, 0xf215, "c8c8c6421091e643c64110874c75f101020201a525492085252920d00ae625a525c9209002e63960");
		require_site(rom, 0xcc1a, "8e0001a90185128a8dffff4a8dffff4a8dffff4a8dffff4a8dffffa512c901f047");
		require_site(rom, 0xcc7f, "4c1dccc61260");
	}

	Asm clear_code() {
		Asm code;
		code.tya(); code.bne("hud");
		code.lda_imm(0xf0); code.sta_abs(0x0700);
		code.label("hud"); code.lda_imm(0xf0);
		for (word addr{ 0x0704 }; addr < 0x0800; addr += 4)
			code.sta_abs(addr);
		code.ldy_imm(0); code.jmp(0xcb8d);
		return code;
	}

	Asm draw_code(bool flipped) {
		Asm code;
		code.lda_zp(0x25); code.asl_a(); code.asl_a(); code.eor_zp(0x1c); code.tax();
		code.lda_zp(1); code.sta_abs_x(0x0700);
		code.lda_ind_y(0x3a); code.clc(); code.adc_zp(0x33); code.sta_abs_x(0x0701);
		code.iny(); code.lda_ind_y(0x3a); code.eor_zp(0x29); code.sta_zp(1);
		// select the same priority bit as the normal/flipped mask tables,
		// leaving Y on the attribute byte throughout.
		code.lda_zp(0x42); code.lsr_a(); code.lda_zp(0x26);
		if (flipped) code.bcs("selected"); else code.bcc("selected");
		code.lsr_a(); code.label("selected"); code.and_imm(1); code.beq("attribute");
		code.lda_zp(1); code.ora_imm(0x20); code.sta_zp(1);
		code.label("attribute"); code.lda_zp(1); code.sta_abs_x(0x0702);
		code.lda_zp(0); code.sta_abs_x(0x0703); code.jsr(0xf228);
		code.iny(); code.jmp(flipped ? 0xf217 : 0xf163);
		while (code.size() < 65) code.nop();
		if (code.size() != 65) throw std::runtime_error("AtlasDevSpriteSpeed: draw span overflow");
		return code;
	}
}

word fh::HackManager::install_AtlasDevSpriteSpeed(const fe::Config&, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack) const {
	const auto mode{ hack.string_or("mode", "both") };
	if (mode != "clear" && mode != "draw" && mode != "both")
		throw std::runtime_error("AtlasDevSpriteSpeed: mode must be clear, draw or both");
	const bool clear{ mode != "draw" }, draw{ mode != "clear" };
	require_layout(rom);
	if (clear) require_clear(rom);
	if (draw) require_draw(rom);
	if (clear) {
		auto code{ clear_code() };
		if (code.size() != CLEAR_SIZE || std::size_t{ cpu_addr } + code.size() > 0xfffa)
			throw std::runtime_error("AtlasDevSpriteSpeed: clear helper reaches interrupt vectors");
		const auto off{ Asm::get_file_offset(15, cpu_addr) };
		// the orchestrator owns this cursor. do not search padding for space
		// or overwrite a previous occupant when installing into a patched rom.
		if (!std::all_of(rom.begin() + off, rom.begin() + off + code.size(),
			[](byte value) { return value == 0xff; }))
			throw std::runtime_error("AtlasDevSpriteSpeed: allocated clear-helper range is occupied");
		code.apply_hack_noclear(rom, 15, cpu_addr);
		Asm hook; hook.jmp(cpu_addr); hook.apply_hack_noclear(rom, 15, 0xcb84);
		cpu_addr = static_cast<word>(cpu_addr + code.size());
	}
	if (draw) {
		draw_code(false).apply_hack_noclear(rom, 15, 0xf120);
		draw_code(true).apply_hack_noclear(rom, 15, 0xf1d4);
	}
	return cpu_addr;
}
