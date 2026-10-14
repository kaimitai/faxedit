// AtlasDevVerticalScroll
// vertical room changes scroll instead of cutting to black. the engine
// already carries a full vertical camera pipeline whose last stage, the
// ppuscroll y write in PPU_HandleOnInterrupt, always writes zero; this
// writes the real value and hooks the transition so the destination room
// streams in under the fixed hud.
//
// the slide needs the room it leaves on the right nametable page for the
// direction: page 0 for a descent, page 1 for an ascent. when it is on the
// wrong one, the room is first repainted onto the other page (33 frames,
// nothing visible moves). after the slide the landed room is repainted into
// its page-aligned slot and the nmi swaps back to vertical mirroring and
// scroll y 0, so the rest of the engine sees the state it expects.
//
// the nmi publish, the camera math, the overlay correction, the mirroring
// routine and the stubs stay in bank 15 (the resident part). the armer, the
// gate, both row stagers, the player projection and the landing repaint (the
// banked part) are reached through stubs that save the mapped bank, switch
// through MMC1_UpdateROMBank, call, and switch back. every banked call is
// main-line; the nmi reaches only the publish.
//
// the banked part goes at the start of bank 9's trailing free space; bank and
// addr move it. bank=15 and sram=true move the resident part with it, in
// front, as one piece. the whole hack is too big for the fixed bank's free
// space, so bank=15 only fits a rom with more room there. with sram=true only
// the mirroring reset stays in bank 15, since the boot burst calls it before
// sram is filled. with bank=15 or sram the stubs still map bank 9, which no
// longer matters.
//
// ram: 11 bytes at $04e2-$04ec plus the zero page mirroring shadow $9c.
// that clears the scheduler core ($04d8-$04de), AtlasDevJumpControl ($04df)
// and AtlasDevLadderCrown ($04e0-$04e1); the scheduler's POST clients
// (AtlasDevDayNightCycle, AtlasDevInfectedTint, AtlasDevTimeOfDay) and
// AtlasDevEnemyHud use the same bytes, so either side refuses the other.
#include "common/klib/Asm6502.h"
#include "AtlasDevFrameScheduler.h"
#include "AtlasDevVerticalScroll.h"
#include "fh_constants.h"
#include "GeneralHack.h"
#include "HackManager.h"
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	namespace ROM = fh::ROM;
	namespace RAM = fh::RAM;
	namespace PPU = fh::PPU;
	namespace MMC1 = fh::MMC1;

	constexpr int MAX_FLAG{ 247 };
	constexpr int NO_FLAG{ -1 };
	constexpr word ORG_LIMIT{ 0xffe0 };
	constexpr byte BANK9{ 9 };
	constexpr byte FIXED_BANK{ 15 };
	constexpr byte OVERLAY_BANK{ 14 };
	constexpr word ENEMY_HUD_HIT{ 0x8897 };

	// ram
	constexpr word MODE{ 0x04e2 };      // 0 off, 1 sliding, 2 landed and repainting, 3 swap at the next nmi
	constexpr word CAMOVR{ 0x04e3 };    // camera y while landed
	constexpr word FLIP{ 0x04e3 };      // the parity repaint; never live at the same time as CAMOVR
	constexpr word TLO{ 0x04e4 }, THI{ 0x04e5 }, ALO{ 0x04e6 }, AHI{ 0x04e7 };   // tile and attribute destination bases
	constexpr word PAYLOAD{ 0x04e8 };   // repaint payload index
	constexpr word TMP{ 0x04e9 }, TMP2{ 0x04ea };
	constexpr word DIR{ 0x04eb };       // 0 descent, 1 ascent
	constexpr word XSAVE{ 0x04ec };     // carries x or a across the bank switch
	// the mmc1 control value in force; bit 6 marks horizontal mirroring.
	// the game never uses this zero page byte
	constexpr byte CTRL{ 0x9c };

	// engine addresses inside routines that have no name of their own
	constexpr word Screen_LoadBlockDataVert_attrTail{ 0xd4b2 };
	constexpr word Game_MainLoop_smoothScroll{ 0xdbaf };
	// the room's attribute shadow, 8 rows of 8 bytes
	constexpr word ATTR_SHADOW{ 0x0242 };
	constexpr byte DRAW_BANK{ 14 }, STAGER_BANK{ 3 };

	constexpr byte LAG_DOWN{ 8 }, LAG_UP{ 16 };      // the camera trails the row stager by this many pixels
	constexpr byte TILE_ROWS{ 26 }, PAYLOADS{ 33 };  // a room is 26 tile rows and 7 attribute rows

	// bit abs, written alone to skip the next two bytes
	constexpr byte OP_BIT_ABS{ 0x2c };

	struct Settings {
		bool up{ true }, down{ true }, flip{ true };
		int bias{ 32 };
		int flag{ NO_FLAG };
	};

	// entry points one part calls in the other; zero on the sizing pass
	struct Links {
		word gate{}, arm{}, tile{}, attr{}, plya{}, end{};  // banked
		word camy{}, exit{}, draw{}, stage{};               // resident
		word update_rom_bank{};                             // the engine's bank switch
	};

	// hook sites, named after the routine they sit in or run from
	constexpr word Game_MainLoop_checkScrollDirection{ 0xdb8b };      // vertical moves take the blanked driver
	constexpr word Area_LoadScrollData{ 0xd127 };                     // arms the pan
	constexpr word Screen_LoadBlockDataVert_tileAddr{ 0xd440 };       // a tile row is staged
	constexpr word Screen_LoadBlockDataVert_attrAddr{ 0xd4a8 };       // an attribute row is staged
	constexpr word PPU_HandleOnInterrupt_scrollY{ 0xca1f };           // ppuscroll y, always 0
	constexpr word Game_MainLoop_scrollDone{ 0xdbec };                // the smooth driver returns to the loop
	constexpr word Player_DrawBody_posY{ 0xec06 };                    // the hero's y origin
	constexpr word Player_DrawBody_swingTail{ 0xec9e };               // the weapon swing sprite
	constexpr word Player_DrawShield_tail{ 0xb9d1 };                  // bank 14
	constexpr word Player_DrawWeapon_tail{ 0xb875 };                  // bank 14
	constexpr word MMC1_Init_control{ 0xcbd5 };                       // the cold boot control write
	constexpr word MMC1_UpdateROMBank_reset{ 0xcc40 };                // the retry's control reset
	constexpr word MMC1_EnsurePRG_reset{ 0xcc93 };                    // the nmi's control reset

	struct Site { byte bank; word cpu; std::vector<byte> orig; };

	// vanilla bytes at every hooked site
	const std::vector<Site> SITES{
		{ FIXED_BANK, Game_MainLoop_checkScrollDirection, { 0xa5, 0x54, 0xc9, 0x02, 0x90, 0x1e } },
		{ FIXED_BANK, Area_LoadScrollData, { 0x86, 0x54, 0xa5, 0x0d, 0x85, 0x59 } },
		{ FIXED_BANK, Screen_LoadBlockDataVert_tileAddr, { 0xa9, 0x01, 0x85, 0x73 } },
		{ FIXED_BANK, Screen_LoadBlockDataVert_attrAddr, { 0xbd, 0xcf, 0xd4 } },
		{ FIXED_BANK, PPU_HandleOnInterrupt_scrollY, { 0xa9, 0x00, 0x8d, 0x05, 0x20 } },
		{ FIXED_BANK, Game_MainLoop_scrollDone, { 0x4c, 0x45, 0xdb } },
		{ FIXED_BANK, Player_DrawBody_posY, { 0xa5, 0xa1, 0x85, 0x28 } },
		{ FIXED_BANK, Player_DrawBody_swingTail, { 0x4c, 0x39, 0xf0 } },
		{ OVERLAY_BANK, Player_DrawShield_tail, { 0x4c, 0x39, 0xf0 } },
		{ OVERLAY_BANK, Player_DrawWeapon_tail, { 0x4c, 0x39, 0xf0 } },
		{ FIXED_BANK, MMC1_Init_control, { 0xa9, 0x0e, 0x8d, 0xff, 0x9f, 0x4a, 0x8d, 0xff, 0x9f, 0x4a, 0x8d,
			0xff, 0x9f, 0x4a, 0x8d, 0xff, 0x9f, 0x4a, 0x8d, 0xff, 0x9f } },
		{ FIXED_BANK, MMC1_UpdateROMBank_reset, { 0xa9, 0x0e } },
		{ FIXED_BANK, MMC1_EnsurePRG_reset, { 0xa9, 0x0e } },
	};

	Settings parse(const fh::GeneralHack& h) {
		Settings s;
		s.up = h.bool_or("up", true);
		s.down = h.bool_or("down", true);
		s.flip = h.bool_or("flip", true);
		s.bias = static_cast<int>(h.word_or("bias", 32));
		s.flag = h.has_param("flag") ? static_cast<int>(h.get_word("flag")) : NO_FLAG;
		if (!s.up && !s.down)
			throw std::runtime_error("AtlasDevVerticalScroll: up=false down=false installs nothing");
		if (s.bias < 0 || s.bias > 255)
			throw std::runtime_error("AtlasDevVerticalScroll: bias must be 0 to 255");
		if (s.flag != NO_FLAG && (s.flag < 0 || s.flag > MAX_FLAG))
			throw std::runtime_error(std::format("AtlasDevVerticalScroll: flag must be 0 to {}", MAX_FLAG));
		return s;
	}

	void require_pristine(const std::vector<byte>& rom) {
		for (const auto& s : SITES) {
			const auto off{ klib::Asm6502::get_file_offset(s.bank, s.cpu) };
			if (off + s.orig.size() > rom.size())
				throw std::runtime_error("AtlasDevVerticalScroll: rom image too small");
			for (std::size_t i{ 0 }; i < s.orig.size(); ++i)
				if (rom[off + i] != s.orig[i])
					throw std::runtime_error(std::format(
						"AtlasDevVerticalScroll: hook site ${:04x} in bank {} is not stock", s.cpu, s.bank));
		}
	}

	// a scheduler POST client is wired when the POST operand no longer points
	// at the scheduler's own stub, or the lane is armed (the test
	// AtlasDevDayNightCycle uses before claiming it)
	bool post_client_installed(const std::vector<byte>& rom) {
		const word base{ fh::afs::find_base(rom) };
		if (base == 0)
			return false;
		const auto sched{ klib::Asm6502::get_file_offset(FIXED_BANK, base) };
		const word post{ static_cast<word>(rom[sched + fh::afs::OFF_POST] | (rom[sched + fh::afs::OFF_POST + 1] << 8)) };
		return post != static_cast<word>(base + fh::afs::OFF_STUB) || rom[sched + fh::afs::OFF_POSTARMED] != 0;
	}

	void require_free(const std::vector<byte>& rom, byte bank, word cpu, std::size_t size) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < size; ++i)
			if (off + i >= rom.size() || rom[off + i] != 0xff)
				throw std::runtime_error(std::format(
					"AtlasDevVerticalScroll: no free space for {} bytes at ${:04x} in bank {}", size, cpu, bank));
	}

	// mmc1 control = a, five serial writes after a shift register reset. a is
	// kept in CTRL, which records the mirroring, and reloaded with lda abs
	// rather than lda zp: this runs at the hud split, and the slide was tuned
	// with this timing; a cycle less moves the mirroring change onto another
	// scanline
	void emit_control_write(klib::Asm6502& a) {
		a.sta_zp(CTRL);
		a.lda_imm(0xff);
		a.sta_abs(MMC1::MMC1_PRG_BANK);
		a.lda_abs(CTRL);
		for (int i{ 0 }; i < 5; ++i) {
			a.sta_abs(MMC1::MMC1_CONTROL);
			if (i != 4)
				a.lsr_a();
		}
		a.rts();
	}

	// stub head: remember the mapped bank, map the banked part
	void emit_far_head(klib::Asm6502& a, byte bank, const Links& far) {
		a.lda_abs(RAM::CurrentROMBank);
		a.pha();
		a.ldx_imm(bank);
		a.jsr(far.update_rom_bank);
	}

	// the bank 15 part
	void emit_resident(klib::Asm6502& a, const Settings& s, byte bank, const Links& far) {
		if (s.flag != NO_FLAG) {
			// in front of the armer: flag clear runs the displaced vanilla
			// bytes, so no transaction is accepted and every other hook
			// behaves as stock at mode 0
			a.label("@gate");
			a.lda_abs(static_cast<word>(RAM::Flags + (s.flag >> 3)));
			a.and_imm(static_cast<byte>(1 << (s.flag & 7)));
			a.beq("@gate_off");
			a.jmp("@tramp_arm");
			a.label("@gate_off");
			a.stx_zp(RAM::ZP_Screen_ScrollDirection);
			a.lda_zp(RAM::ZP_CameraNametableParity);
			a.sta_zp(RAM::ZP_PPU_ScrollScreenHoriz);
			a.rts();
		}

		// the nmi's vertical scroll, at the hud split. a $2005 write alone
		// cannot move v mid-frame, so this does the whole reload: $2006 high,
		// $2005 y, $2005 x, $2006 low. they share one write toggle, so the
		// order is fixed. outside a transaction it writes the stock zero
		a.label("@vs_pub");
		a.lda_abs(MODE);
		a.beq("@p_van");
		a.cmp_imm(0x03);
		a.beq("@p_swap");
		// no hud split armed (message window, menu): the write would scroll
		// the hud as well, so decline
		a.bit_zp(RAM::ZP_Sprites_Sprite0Mode);
		a.bmi("@p_van");
		a.bit_abs(CTRL);                   // v = bit 6, horizontal mirroring
		a.bvs("@p_haveh");                 // over the jsr and the three nops
		a.jsr("@mirr_h");
		a.nop(3);                          // timing: the writes below were tuned with these
		a.label("@p_haveh");
		a.jsr("@vs_camy");                 // a = displacement, 0 to 208
		// the ring line is bias + d for a descent, bias + 240 - d for an
		// ascent; 240 and up is page 1, folded back into coarse y 0 to 29
		a.ldx_abs(DIR);
		a.beq("@p_e");
		a.sta_abs(TMP);
		a.lda_imm(0xf0);
		a.sec();
		a.sbc_abs(TMP);
		a.label("@p_e");
		a.clc();
		a.adc_imm(static_cast<byte>(s.bias));
		a.ldx_imm(0x08);                   // nametable y, page 1
		a.bcs("@p_fold");
		a.cmp_imm(0xf0);
		a.bcs("@p_fold");
		a.ldx_imm(0x00);
		a.beq("@p_line");
		a.label("@p_fold");
		a.clc();
		a.adc_imm(0x10);
		a.label("@p_line");
		a.stx_abs(TMP2);
		a.tay();
		a.and_imm(0x38);                   // coarse y << 5 for the low address byte
		a.asl_a();
		a.asl_a();
		a.sta_abs(TMP);
		a.lda_zp(RAM::ZP_PPU_ScrollX);
		a.lsr_a();
		a.lsr_a();
		a.lsr_a();
		a.ora_abs(TMP);
		a.tax();
		a.bit_abs(PPU::PPU_STATUS);        // reset the write toggle
		a.lda_zp(RAM::ZP_CameraNametableParity);
		a.and_imm(0x01);
		a.asl_a();
		a.asl_a();
		a.ora_abs(TMP2);
		a.sta_abs(PPU::PPU_ADDR);
		a.tya();
		a.sta_abs(PPU::PPU_SCROLL);
		a.lda_zp(RAM::ZP_PPU_ScrollX);
		a.sta_abs(PPU::PPU_SCROLL);
		a.txa();
		a.sta_abs(PPU::PPU_ADDR);
		a.rts();
		// the landing swap: both pages hold the destination room, so going
		// back to vertical mirroring and scroll y 0 moves no pixel
		a.label("@p_swap");
		a.jsr("@mirr_v");
		a.lda_imm(0x00);
		a.sta_abs(MODE);
		a.sta_abs(CAMOVR);                 // also clears FLIP
		a.sta_abs(DIR);
		a.sta_abs(PPU::PPU_SCROLL);
		a.rts();
		a.label("@p_van");
		a.lda_imm(0x00);
		a.sta_abs(PPU::PPU_SCROLL);
		a.rts();

		// the camera's displacement from where it started, 0 to 208. it trails
		// the pipeline counter ZP_PPU_ScrollY so a row reaches vram before it is
		// shown; vs_end's ramp pays the lag back. it counts up for a descent and down
		// for an ascent, where 0 means the first step has not happened yet
		a.label("@vs_camy");
		a.lda_abs(MODE);
		a.cmp_imm(0x02);
		a.beq("@c_ovr");
		a.lda_abs(DIR);
		a.bne("@c_up");
		a.lda_zp(RAM::ZP_PPU_ScrollY);
		a.sec();
		a.sbc_imm(LAG_DOWN);
		a.bcs("@c_ret");
		a.label("@c_zero");
		a.lda_imm(0x00);
		a.label("@c_ret");
		a.rts();
		a.label("@c_up");
		a.ldx_zp(RAM::ZP_PPU_ScrollY);
		a.beq("@c_zero");
		a.lda_imm(static_cast<byte>(208 - LAG_UP));
		a.sec();
		a.sbc_zp(RAM::ZP_PPU_ScrollY);
		a.bcs("@c_ret");
		a.bcc("@c_zero");
		a.label("@c_ovr");
		a.lda_abs(CAMOVR);
		a.rts();

		// the player's y origin in Player_DrawBody. sprite y has no scroll term, so the
		// hero is projected by hand while the room slides
		a.label("@vs_ply");
		a.jsr("@ply_far");
		a.sta_zp(RAM::ZP_Arg_DrawSprite_PosY);
		a.rts();
		// a = the projected origin, y kept. mode 0 answers the hero's y without
		// a bank switch; a transaction calls the banked part and carries a
		// through XSAVE, since the bank switch clobbers a and x
		a.label("@ply_far");
		a.lda_abs(MODE);
		a.bne("@pf_go");
		a.lda_zp(RAM::ZP_PlayerPosY);
		a.rts();
		a.label("@pf_go");
		emit_far_head(a, bank, far);
		a.jsr(far.plya);
		a.sta_abs(XSAVE);
		a.pla();
		a.tax();
		a.jsr(far.update_rom_bank);
		a.lda_abs(XSAVE);
		a.rts();

		// weapon and shield overlays, hooked at their jmp
		// Sprite_SetPlayerAppearanceAddr tails so the visibility probe before it
		// keeps the undisplaced y. the correction is the delta origin - hero y,
		// so one routine serves all three, and at mode 0
		// the delta is 0
		a.label("@vs_ovl");
		a.tay();
		a.jsr("@ply_far");
		a.sec();
		a.sbc_zp(RAM::ZP_PlayerPosY);
		a.clc();
		a.adc_zp(RAM::ZP_Arg_DrawSprite_PosY);
		a.sta_zp(RAM::ZP_Arg_DrawSprite_PosY);
		a.tya();
		a.jmp(ROM::Sprite_SetPlayerAppearanceAddr);

		// stubs: the bank switch keeps y and clobbers a, x and flags, so each carries
		// what its site needs. the gate returns carry and keeps x
		a.label("@stub_gate");
		a.txa();
		a.pha();
		emit_far_head(a, bank, far);
		a.jsr(far.gate);
		a.pla();
		a.tax();
		a.php();
		a.jsr(far.update_rom_bank);
		a.plp();
		a.pla();
		a.tax();
		a.rts();
		a.label("@stub_tile");
		emit_far_head(a, bank, far);
		a.jsr(far.tile);
		a.pla();
		a.tax();
		a.jmp(far.update_rom_bank);
		// the attribute stager needs x, the parity
		a.label("@stub_attr");
		a.stx_abs(XSAVE);
		emit_far_head(a, bank, far);
		a.ldx_abs(XSAVE);
		a.jsr(far.attr);
		a.pla();
		a.tax();
		a.jmp(far.update_rom_bank);

		// mirr_h falls into mirr_v through a bit abs that swallows its lda:
		// only the opcode is written, the lda below is its operand
		a.label("@mirr_h");
		a.lda_imm(0x4f);                   // horizontal, bit 6 set
		a.db(OP_BIT_ABS);
		a.label("@mirr_v");
		a.lda_imm(0x0e);                   // vertical, as stock
		emit_control_write(a);

		// the armer stores the direction before the switch, which needs x
		a.label("@tramp_arm");
		a.stx_zp(RAM::ZP_Screen_ScrollDirection);
		emit_far_head(a, bank, far);
		a.jsr(far.arm);
		a.pla();
		a.tax();
		a.jmp(far.update_rom_bank);
		// vs_end is a jmp entry; its own exits restore the bank
		a.label("@tramp_end");
		emit_far_head(a, bank, far);
		a.jmp(far.end);
		// the shield and weapon draws live in bank 14, and the body draw calls
		// into it (x = 14 from the caller)
		a.label("@tramp_draw");
		a.jsr(far.update_rom_bank);
		a.jsr(ROM::Player_DrawShield);
		a.jsr(ROM::Player_DrawBody);
		a.jsr(ROM::Player_DrawWeapon);
		a.jmp("@tramp_restore");
		// the row stager reads the area's block tables from bank 3 (x = 3 from
		// the caller)
		a.label("@tramp_stage");
		a.jsr(far.update_rom_bank);
		a.ldx_imm(0x00);
		a.jsr(ROM::Screen_LoadBlockDataVert);
		a.label("@tramp_restore");
		a.ldx_imm(bank);
		a.jmp(far.update_rom_bank);
		a.label("@tramp_exit");
		a.pla();
		a.tax();
		a.jsr(far.update_rom_bank);
		a.jmp(ROM::Game_MainLoop);
	}

	// the banked part
	void emit_banked(klib::Asm6502& a, const Settings& s, const Links& near) {
		const byte up_dir{ static_cast<byte>(s.up ? 0x02 : 0xff) };
		const byte down_dir{ static_cast<byte>(s.down ? 0x03 : 0xff) };
		const byte flip_dir{ static_cast<byte>(s.flip ? 0x02 : 0xff) };

		// the smooth driver's direction gate in Game_MainLoop: carry clear takes the
		// smooth driver, set the stock blanked one. accepted, it seeds the
		// player drag so the hero lands where the destination room wants him
		a.label("@vs_gate");
		a.lda_abs(MODE);
		a.beq("@g_vanilla");
		a.lda_abs(DIR);
		a.bne("@g_seeded");
		a.lda_imm(0xc0);
		a.sta_zp(RAM::ZP_Maybe_PlayerY_ForScroll);
		a.label("@g_seeded");
		a.lda_imm(0x04);
		a.sta_zp(RAM::ZP_Screen_ScrollPlayerTransitionCounter);
		a.clc();
		a.rts();
		a.label("@g_vanilla");
		a.lda_zp(RAM::ZP_Screen_ScrollDirection);
		a.cmp_imm(0x02);
		a.rts();

		// the head of Area_LoadScrollData: decides whether this transaction can
		// scroll and latches the destination bases. a descent needs the room
		// on page 0, an ascent on page 1, and both need it screen aligned
		a.label("@vs_arm");
		a.ldx_zp(RAM::ZP_Screen_ScrollDirection);
		a.lda_imm(0x00);
		a.sta_abs(MODE);
		a.sta_abs(DIR);
		a.jsr("@vs_flip");                 // before the parity snapshot, since it may flip it
		a.lda_zp(RAM::ZP_CameraNametableParity);
		a.sta_zp(RAM::ZP_PPU_ScrollScreenHoriz);
		a.lda_zp(RAM::ZP_PPU_ScrollX);
		a.bne("@a_ret");
		a.lda_zp(RAM::ZP_PPU_ScrollY);
		a.bne("@a_ret");
		a.cpx_imm(down_dir);
		a.beq("@a_down");
		a.cpx_imm(up_dir);
		a.bne("@a_ret");
		a.lda_zp(RAM::ZP_CameraNametableParity);
		a.and_imm(0x01);
		a.beq("@a_ret");
		a.lda_imm(0x01);
		a.sta_abs(DIR);
		a.ldx_imm(0x04);                   // ascent slide bases
		a.bne("@a_accept");
		a.label("@a_down");
		a.lda_zp(RAM::ZP_CameraNametableParity);
		a.and_imm(0x01);
		a.bne("@a_ret");
		a.ldx_imm(0x00);                   // descent slide bases
		a.label("@a_accept");
		a.jsr("@set_bases");
		a.inc_abs(MODE);                   // 0 to 1
		a.label("@a_ret");
		a.rts();

		// the parity repaint: when the room sits on the wrong page for the
		// direction, repaint it onto the other page and display that copy. the
		// two copies are the same, so nothing moves, and the slide is then
		// legal. $0600 still holds the source room here; the armer decodes the
		// destination later
		a.label("@vs_flip");
		a.cpx_imm(flip_dir);               // vertical only
		a.bcc("@f_ret");
		a.cpx_imm(down_dir);               // and only a direction this build accepts
		a.beq("@f_dir");
		a.cpx_imm(up_dir);
		a.bne("@f_ret");
		a.label("@f_dir");
		a.txa();                           // wrong page iff (x ^ parity) bit 0 is clear
		a.eor_zp(RAM::ZP_CameraNametableParity);
		a.lsr_a();
		a.bcs("@f_ret");
		a.lda_zp(RAM::ZP_PPU_ScrollX);
		a.ora_zp(RAM::ZP_PPU_ScrollY);
		a.bne("@f_ret");
		a.ldx_imm(0x10);                   // page 1 aligned slot
		a.lda_zp(RAM::ZP_CameraNametableParity);
		a.lsr_a();
		a.bcc("@f_go");
		a.ldx_imm(0x08);                   // page 0 aligned slot
		a.label("@f_go");
		a.jsr("@set_bases");
		a.lda_imm(0x02);
		a.sta_abs(FLIP);
		// keep the hero on screen during the repaint: park him on the edge he
		// is leaving; the caller sets the landing y once the armer returns
		a.lda_zp(RAM::ZP_Screen_ScrollDirection);
		a.and_imm(0x01);
		a.tax();
		a.lda_abs_x("@vs_park");
		a.sta_zp(RAM::ZP_PlayerPosY);
		a.jsr("@vs_repaint");
		a.lda_imm(0x00);
		a.sta_abs(FLIP);
		// both parity bytes step together, as a horizontal crossing does
		a.lda_zp(RAM::ZP_CameraNametableParity);
		a.eor_imm(0x01);
		a.sta_zp(RAM::ZP_CameraNametableParity);
		a.sta_zp(RAM::ZP_PPU_ScrollScreenHoriz);
		a.label("@f_ret");
		a.ldx_zp(RAM::ZP_Screen_ScrollDirection);
		a.rts();

		// four destination base bytes from vs_bases at x into TLO, THI, ALO, AHI
		a.label("@set_bases");
		a.ldy_imm(0x00);
		a.label("@sb_l");
		a.lda_abs_x("@vs_bases");
		a.sta_abs_y(TLO);
		a.inx();
		a.iny();
		a.cpy_imm(0x04);
		a.bcc("@sb_l");
		a.rts();

		// the row-tile destination: TLO/THI + row * 32. the ascent's
		// destination straddles the pages, so its rows 22 and up add $440
		a.label("@vs_tile");
		a.lda_abs(MODE);
		a.ora_abs(FLIP);
		a.tax();
		a.beq("@t_done");
		a.lda_imm(0x00);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.lda_zp(RAM::ZP_Screen_ScrollVertLoadCounter);
		a.and_imm(0xf8);
		a.asl_a();
		a.rol_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.asl_a();
		a.rol_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.clc();
		a.adc_abs(TLO);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_L);
		a.lda_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.adc_abs(THI);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.cpx_imm(0x01);                   // the slide only
		a.bne("@t_done");
		a.lda_abs(DIR);
		a.beq("@t_done");
		a.lda_zp(RAM::ZP_Screen_ScrollVertLoadCounter);
		a.cmp_imm(0xb0);
		a.bcc("@t_done");
		a.lda_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_L);
		a.clc();
		a.adc_imm(0x40);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_L);
		a.lda_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.adc_imm(0x04);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUTileMapAddr_U);
		a.label("@t_done");
		a.lda_imm(0x01);                   // displaced vanilla
		a.sta_zp(RAM::ZP_Screen_ScrollHandlersPending);
		a.rts();

		// the row-attribute address. the shadow stays vanilla; only
		// the vram address moves. the ascent's page seam needs one row no
		// shadow byte holds, built into the payload buffer after the stock
		// merge: (shadow row 6 >> 4) | (shadow row 7 << 4)
		a.label("@vs_attr");
		a.lda_abs(MODE);
		a.ora_abs(FLIP);
		a.beq("@r_tail");
		a.lda_abs(DIR);
		a.beq("@r_alt");
		a.lda_abs(MODE);
		a.cmp_imm(0x01);
		a.bne("@r_alt");
		a.lda_zp(RAM::ZP_Temp00);
		a.cmp_imm(0x30);                   // attribute row 6
		a.beq("@r_seam");
		a.cmp_imm(0x28);                   // attribute row 5, bottom half
		a.bne("@r_alt");
		a.lda_zp(RAM::ZP_Screen_ScrollVertLoadCounter);
		a.and_imm(0x10);
		a.bne("@r_seam");
		a.label("@r_alt");
		a.lda_zp(RAM::ZP_Temp00);
		a.clc();
		a.adc_abs(ALO);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUAttrAddr_L);
		a.lda_abs(AHI);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUAttrAddr_U);
		a.label("@r_tail");
		a.lda_abs_x(ROM::SET_BLOCKS_TILEMAP_OFFSETS_L);
		a.sta_zp(RAM::ZP_Temp08);
		a.lda_abs_x(ROM::SET_BLOCKS_TILEMAP_OFFSETS_U);
		a.sta_zp(RAM::ZP_Temp08 + 1);
		// a call, so stub_attr restores the bank
		a.jsr(Screen_LoadBlockDataVert_attrTail);
		a.rts();
		a.label("@r_seam");
		a.jsr("@r_tail");
		a.ldx_imm(0x00);
		a.label("@r_sl");
		a.lda_abs_x(static_cast<word>(ATTR_SHADOW + 56));
		a.asl_a();
		a.asl_a();
		a.asl_a();
		a.asl_a();
		a.sta_abs_x(RAM::Screen_ScrollVertPPUAttrData);
		a.lda_abs_x(static_cast<word>(ATTR_SHADOW + 48));
		a.lsr_a();
		a.lsr_a();
		a.lsr_a();
		a.lsr_a();
		a.ora_abs_x(RAM::Screen_ScrollVertPPUAttrData);
		a.sta_abs_x(RAM::Screen_ScrollVertPPUAttrData);
		a.inx();
		a.cpx_imm(0x08);
		a.bcc("@r_sl");
		a.lda_imm(0xc0);                   // $2bc0, page 1 attribute row 0
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUAttrAddr_L);
		a.lda_imm(0x2b);
		a.sta_zp(RAM::ZP_Screen_ScrollVertBlocks_PPUAttrAddr_U);
		a.rts();

		// the projected y origin: a descent shows 208 - d, an ascent the
		// engine's own drag
		a.label("@vs_plya");
		a.lda_abs(MODE);
		a.bne("@y_adj");
		a.lda_zp(RAM::ZP_PlayerPosY);
		a.rts();
		a.label("@y_adj");
		a.lda_abs(DIR);
		a.beq("@y_down");
		a.lda_zp(RAM::ZP_Maybe_PlayerY_ForScroll);
		a.rts();
		a.label("@y_down");
		a.jsr(near.camy);
		a.eor_imm(0xff);
		a.sec();
		a.adc_imm(0xd0);
		a.rts();

		// the smooth driver's exit: ramp frames pay back the camera
		// lag, the landed room is repainted into its page-aligned slot one
		// payload per frame, then mode 3 lets the nmi swap
		a.label("@vs_end");
		a.lda_abs(MODE);
		a.bne("@e_go");
		a.jmp(near.exit);
		a.label("@e_go");
		a.lda_imm(0x02);
		a.sta_abs(MODE);
		a.ldy_imm(0x00);                   // ascent ramp, 4 frames
		a.lda_abs(DIR);
		a.bne("@e_ramp");
		a.ldy_imm(0x01);                   // descent ramp, 3 frames
		a.label("@e_ramp");
		a.lda_abs_y("@vs_ramp");
		a.beq("@e_paint");
		a.sta_abs(CAMOVR);
		a.jsr(ROM::WaitForNextFrame);
		a.iny();
		a.bne("@e_ramp");
		a.label("@e_paint");
		a.ldx_imm(0x08);                   // descent repaint bases
		a.lda_abs(DIR);
		a.beq("@e_bases");
		a.ldx_imm(0x0c);                   // ascent repaint bases
		a.label("@e_bases");
		a.jsr("@set_bases");
		a.jsr("@vs_repaint");
		a.inc_abs(MODE);                   // 2 to 3
		a.jsr(ROM::WaitForNextFrame);
		a.jmp(near.exit);

		// 33 payloads, one per frame, into the latched bases, with the frame
		// kept live: the same calls the smooth driver makes every pan frame
		a.label("@vs_repaint");
		a.lda_imm(0x00);
		a.sta_abs(PAYLOAD);
		a.label("@e_loop");
		a.jsr(ROM::Screen_ResetSpritesForGamePlay);
		a.ldx_imm(DRAW_BANK);
		a.jsr(near.draw);
		a.jsr("@vs_stage");
		a.jsr(ROM::WaitForNextFrame);
		a.inc_abs(PAYLOAD);
		a.lda_abs(PAYLOAD);
		a.cmp_imm(PAYLOADS);
		a.bcc("@e_loop");
		a.rts();

		// payload n through the engine's own row stager: 0 to 25 are tile
		// rows, 26 to 32 attribute rows
		a.label("@vs_stage");
		a.lda_abs(PAYLOAD);
		a.cmp_imm(TILE_ROWS);
		a.bcs("@s_attr");
		a.asl_a();
		a.asl_a();
		a.asl_a();
		a.sta_zp(RAM::ZP_Screen_ScrollVertLoadCounter);
		a.lda_imm(0x00);
		a.sta_zp(RAM::ZP_Screen_ScrollLoadMode);
		a.jmp("@s_go");
		a.label("@s_attr");
		a.sec();
		a.sbc_imm(TILE_ROWS);
		for (int i{ 0 }; i < 5; ++i)
			a.asl_a();
		a.ora_imm(0x08);
		a.sta_zp(RAM::ZP_Screen_ScrollVertLoadCounter);
		a.lda_imm(0x01);
		a.sta_zp(RAM::ZP_Screen_ScrollLoadMode);
		a.label("@s_go");
		a.ldx_imm(STAGER_BANK);
		a.jmp(near.stage);

		// destination bases [tile lo, tile hi, attr lo, attr hi]: 0 descent
		// slide, 4 ascent slide, 8 descent repaint, 12 ascent repaint, 16 the
		// parity repaint's page 1 slot (8 serves its page 0 slot)
		a.label("@vs_bases");
		for (const byte b : { 0x00, 0x28, 0xc0, 0x2b, 0x00, 0x21, 0xd0, 0x23, 0x80, 0x20, 0xc8, 0x23,
			0x80, 0x28, 0xc8, 0x2b, 0x80, 0x24, 0xc8, 0x27 })
			a.db(b);
		// camera displacements after the slide lands, 0 ends; a descent starts at the second
		a.label("@vs_ramp");
		for (const byte b : { 196, 200, 204, 208, 0 })
			a.db(b);
		// where the hero stands during the parity repaint, by direction & 1
		a.label("@vs_park");
		a.db(0x00);
		a.db(0xc0);
	}

	// resident and banked may be the same piece, the resident part first
	void emit(klib::Asm6502& resident, klib::Asm6502& banked, const Settings& s, byte bank, const Links& links) {
		resident.clear();
		banked.clear();
		emit_resident(resident, s, bank, links);
		emit_banked(banked, s, links);
	}
}

bool fh::vertical_scroll_installed(const std::vector<byte>& p_rom) {
	// the publish hook replaces lda #$00 / sta $2005 with jsr / nop / nop
	const auto off{ klib::Asm6502::get_file_offset(FIXED_BANK, PPU_HandleOnInterrupt_scrollY) };
	return off + 5 <= p_rom.size() && p_rom[off] == 0x20 && p_rom[off + 3] == 0xea && p_rom[off + 4] == 0xea;
}

word fh::HackManager::install_AtlasDevVerticalScroll(const fe::Config& p_config, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) {
	const std::string name{ "AtlasDevVerticalScroll" };
	const Settings s{ parse(p_hack) };
	if (p_hack.has_param("mode") && p_hack.get_string("mode") != "vanilla")
		throw std::runtime_error("AtlasDevVerticalScroll: mode must be vanilla");
	// the settings are checked first, so mode=vanilla still catches a typo in them
	if (p_hack.string_or("mode", "") == "vanilla")
		return cpu_addr;
	require_pristine(p_rom);
	if (post_client_installed(p_rom))
		throw std::runtime_error("AtlasDevVerticalScroll: shares ram $04e2-$04ec with the scheduler POST clients "
			"(AtlasDevDayNightCycle, AtlasDevInfectedTint, AtlasDevTimeOfDay)");
	if (p_rom[klib::Asm6502::get_file_offset(OVERLAY_BANK, ENEMY_HUD_HIT)] == 0x20)
		throw std::runtime_error("AtlasDevVerticalScroll: shares ram $04e3-$04e9 with AtlasDevEnemyHud");

	// placed is what bank, addr and sram move: the banked part, or with
	// bank=15 or sram both parts, the resident part first. fixed stays at the
	// bank 15 cursor: the resident part, or with sram the mirroring reset
	const bool sram{ p_hack.bool_or("sram", false) };
	const bool together{ sram || p_hack.byte_or("bank", BANK9) == FIXED_BANK };
	klib::Asm6502 fixed, placed;
	klib::Asm6502& resident{ together ? placed : fixed };

	// no size depends on an address in the other part, so a first pass gives
	// the layout and a second one the code
	emit(resident, placed, s, BANK9, Links{});
	if (sram) {
		fixed.lda_imm(0x0e);
		emit_control_write(fixed);
	}

	// the bank 15 cursor takes fixed, or with bank=15 the placed piece
	const std::size_t bank15_size{ together && !sram ? placed.size() : fixed.size() };
	if (cpu_addr + bank15_size > ORG_LIMIT)
		throw std::runtime_error(std::format(
			"AtlasDevVerticalScroll: {} bytes at ${:04x} do not fit the bank 15 free space", bank15_size, cpu_addr));
	require_free(p_rom, FIXED_BANK, cpu_addr, fixed.size());
	// no far entries: the stubs carry what each site needs across the switch
	const auto place{ place_hack(p_config, p_rom, p_hack, name, cpu_addr, placed.size(), 0, BANK9) };

	const word body{ together ? place.addr : cpu_addr };
	const auto at = [&](const std::string& p_name) { return resident.label_addr(p_name, body); };
	const auto far_at = [&](const std::string& p_name) { return placed.label_addr(p_name, place.addr); };
	const Links links{ far_at("@vs_gate"), far_at("@vs_arm"), far_at("@vs_tile"), far_at("@vs_attr"), far_at("@vs_plya"),
		far_at("@vs_end"), at("@vs_camy"), at("@tramp_exit"), at("@tramp_draw"), at("@tramp_stage"),
		cfg_word(p_config, c::ID_ROM_MMC1_UPDATEROMBANK) };
	const word armer{ at(s.flag == NO_FLAG ? "@tramp_arm" : "@gate") };
	const word gate{ at("@stub_gate") }, tile{ at("@stub_tile") }, attr{ at("@stub_attr") }, publish{ at("@vs_pub") };
	const word end{ at("@tramp_end") }, player{ at("@vs_ply") }, overlay{ at("@vs_ovl") }, mirror{ at("@mirr_v") };
	emit(resident, placed, s, together ? BANK9 : place.bank, links);

	// every check is complete; mutate
	const word cursor{ install_placed(p_rom, placed, place, cpu_addr, name) };
	const word next{ fixed.apply_hack_and_clear_get_next_cpu_addr(p_rom, FIXED_BANK, cursor) };

	klib::Asm6502 hook;
	const auto write_hook = [&](byte p_bank, word p_site, bool p_call, word p_target, std::size_t p_nops) {
		if (p_call)
			hook.jsr(p_target);
		else
			hook.jmp(p_target);
		if (p_nops)
			hook.nop(p_nops);
		hook.apply_hack_and_clear(p_rom, p_bank, p_site);
	};
	// the gate keeps its bcc to the smooth driver
	hook.jsr(gate);
	hook.bcc(static_cast<sbyte>(Game_MainLoop_smoothScroll - (Game_MainLoop_checkScrollDirection + 5)));
	hook.nop();
	hook.apply_hack_and_clear(p_rom, FIXED_BANK, Game_MainLoop_checkScrollDirection);
	write_hook(FIXED_BANK, Area_LoadScrollData, true, armer, 3);
	write_hook(FIXED_BANK, Screen_LoadBlockDataVert_tileAddr, false, tile, 1);
	write_hook(FIXED_BANK, Screen_LoadBlockDataVert_attrAddr, false, attr, 0);
	write_hook(FIXED_BANK, PPU_HandleOnInterrupt_scrollY, true, publish, 2);
	write_hook(FIXED_BANK, Game_MainLoop_scrollDone, false, end, 0);
	write_hook(FIXED_BANK, Player_DrawBody_posY, true, player, 1);
	write_hook(FIXED_BANK, Player_DrawBody_swingTail, false, overlay, 0);
	write_hook(OVERLAY_BANK, Player_DrawShield_tail, false, overlay, 0);
	write_hook(OVERLAY_BANK, Player_DrawWeapon_tail, false, overlay, 0);
	write_hook(FIXED_BANK, MMC1_Init_control, true, sram ? cpu_addr : mirror, 18);
	// the bank switch's two mmc1 resets restore the mirroring in force
	hook.lda_zp(CTRL);
	hook.apply_hack_and_clear(p_rom, FIXED_BANK, MMC1_UpdateROMBank_reset);
	hook.lda_zp(CTRL);
	hook.apply_hack_and_clear(p_rom, FIXED_BANK, MMC1_EnsurePRG_reset);

	return next;
}
