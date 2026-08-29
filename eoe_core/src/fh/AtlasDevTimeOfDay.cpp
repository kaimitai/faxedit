#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <stdexcept>

// time of day: an in game clock with a two digit hour readout in the
// hud, running as a POST role on the AtlasDevFrameScheduler. takes
// hourlength=<frames per in game hour, default 300>, start=<boot hour
// 0 to 23, default 12> and cell=<nametable address of the two readout
// cells, default $2038>. the readout uses the engine's own digit
// convention, tile = digit or $30, so it inherits the hud font and
// palette automatically.
//
// like the tint, the install chains when another role already holds the
// POST vector, so day cycle, tint and clock can all run per call.
// AtlasDevArmRole 4, state switches the clock from scripts; switching
// off blanks the two cells and resets the clock, so re arming starts
// the day at the start hour again. bit 7 of the tens byte marks a
// seeded clock, keeping a wrapped midnight distinct from the boot
// state so 00 advances to 01 instead of falling back to start.

namespace {
	constexpr word HR_TENS{ 0x04e9 }, HR_ONES{ 0x04ea };
	constexpr word SUB_LO{ 0x04eb }, SUB_HI{ 0x04ec };
	constexpr byte KIND_CLOCK{ 0x04 };
}

word fh::HackManager::install_AtlasDevTimeOfDay(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	using namespace fh::afs;
	const word base{ find_base(p_rom) };
	if (base == 0)
		throw std::runtime_error("AtlasDevTimeOfDay requires the AtlasDevFrameScheduler hack installed first");
	const word hourlength{ p_hack.word_or("hourlength", 300) };
	if (hourlength < 8)
		throw std::runtime_error("AtlasDevTimeOfDay: hourlength must be at least 8");
	const byte start{ p_hack.byte_or("start", 12) };
	if (start > 23)
		throw std::runtime_error("AtlasDevTimeOfDay: start must be an hour 0 to 23");
	const word cell{ p_hack.word_or("cell", 0x2038) };
	if (cell < 0x2000 || cell > 0x2fbe)
		throw std::runtime_error("AtlasDevTimeOfDay: cell must be a nametable address");

	const auto scheduler{ klib::Asm6502::get_file_offset(15, base) };
	const bool chain{ p_rom[scheduler + OFF_POSTARMED] != 0x00 };
	const word chain_to{ static_cast<word>(
		p_rom[scheduler + OFF_POST] | (p_rom[scheduler + OFF_POST + 1] << 8)) };

	klib::Asm6502 code;
	// enable gate: run only while some scheduler slot holds our kind
	for (word slot : { RAM_SLOT0, RAM_SLOT1, RAM_SLOT2 }) {
		code.lda_abs(slot);
		code.cmp_imm(KIND_CLOCK);
		code.beq("@enabled");
	}
	// switched off: if the clock ever ran, blank the cells and reset once
	code.lda_abs(SUB_LO);
	code.ora_abs(SUB_HI);
	code.ora_abs(HR_TENS);
	code.ora_abs(HR_ONES);
	code.bne("@restore");
	code.jmp("@tail");
	code.label("@restore");
	code.lda_imm(0x00);
	code.sta_abs(SUB_LO);
	code.sta_abs(SUB_HI);
	code.sta_abs(HR_TENS);
	code.sta_abs(HR_ONES);
	code.lda_imm(static_cast<byte>(cell >> 8)); code.sta_abs(0x2006);
	code.lda_imm(static_cast<byte>(cell & 0xff)); code.sta_abs(0x2006);
	code.lda_imm(0x00);
	code.sta_abs(0x2007);
	code.sta_abs(0x2007);
	code.jmp("@park");
	code.label("@enabled");
	// tens bit 7 marks a seeded clock; a wrapped midnight stays distinct
	// from the boot state, so 00 advances instead of re seeding
	code.lda_abs(HR_TENS);
	code.bmi("@seeded");
	code.lda_imm(static_cast<byte>(0x80 | (start / 10))); code.sta_abs(HR_TENS);
	code.lda_imm(static_cast<byte>(start % 10)); code.sta_abs(HR_ONES);
	code.jmp("@reload");
	code.label("@seeded");
	code.lda_abs(SUB_LO);
	code.ora_abs(SUB_HI);
	code.bne("@tick");
	// the countdown expired: advance the hour, wrap 23 to 00
	code.inc_abs(HR_ONES);
	code.lda_abs(HR_ONES);
	code.cmp_imm(10);
	code.bcc("@wrap24");
	code.lda_imm(0x00); code.sta_abs(HR_ONES);
	code.inc_abs(HR_TENS);
	code.label("@wrap24");
	code.lda_abs(HR_TENS);
	code.and_imm(0x7f);
	code.cmp_imm(2);
	code.bne("@reload");
	code.lda_abs(HR_ONES);
	code.cmp_imm(4);
	code.bne("@reload");
	code.lda_imm(0x80);
	code.sta_abs(HR_TENS);
	code.lda_imm(0x00);
	code.sta_abs(HR_ONES);
	code.label("@reload");
	code.lda_imm(static_cast<byte>(hourlength & 0xff)); code.sta_abs(SUB_LO);
	code.lda_imm(static_cast<byte>(hourlength >> 8)); code.sta_abs(SUB_HI);
	code.jmp("@draw");
	code.label("@tick");
	code.lda_abs(SUB_LO);
	code.bne("@nolo");
	code.dec_abs(SUB_HI);
	code.label("@nolo");
	code.dec_abs(SUB_LO);
	code.label("@draw");
	// hud hour readout one frame in eight: tile = digit or $30
	code.lda_abs(RAM_CNT_LO);
	code.and_imm(0x07);
	code.bne("@tail");
	code.lda_imm(static_cast<byte>(cell >> 8)); code.sta_abs(0x2006);
	code.lda_imm(static_cast<byte>(cell & 0xff)); code.sta_abs(0x2006);
	code.lda_abs(HR_TENS);
	code.and_imm(0x7f);
	code.ora_imm(0x30);
	code.sta_abs(0x2007);
	code.lda_abs(HR_ONES);
	code.ora_imm(0x30);
	code.sta_abs(0x2007);
	code.label("@park");
	code.lda_imm(0x00);
	code.sta_abs(0x2006);
	code.sta_abs(0x2006);
	code.label("@tail");
	if (chain)
		code.jmp(chain_to);
	else
		code.rts();

	// first bank 9 spot with room for the body
	const auto off9_base{ klib::Asm6502::get_file_offset(9, 0x8000) };
	word org{ 0 };
	for (word off{ 0 }; off < 0x4000 - code.size(); ++off) {
		bool free{ true };
		for (std::size_t i{ 0 }; i < code.size(); ++i)
			if (p_rom[off9_base + off + i] != 0xff) { free = false; break; }
		if (free) { org = static_cast<word>(0x8000 + off); break; }
	}
	if (org == 0)
		throw std::runtime_error("AtlasDevTimeOfDay: no free bank 9 window");

	code.apply_hack_and_clear(p_rom, 9, org);
	p_rom[scheduler + OFF_POST] = org & 0xff;
	p_rom[scheduler + OFF_POST + 1] = org >> 8;
	p_rom[scheduler + OFF_POSTARMED] = 0x01;
	for (std::size_t i{ 0 }; i < 3; ++i) {
		if (p_rom[scheduler + OFF_ARM0 + i] == 0x00) {
			p_rom[scheduler + OFF_ARM0 + i] = KIND_CLOCK;
			return cpu_addr;
		}
	}
	throw std::runtime_error("AtlasDevTimeOfDay: no free scheduler slot");
}
