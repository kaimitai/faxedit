#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <stdexcept>

// day and night: a palette dim cycle running as a POST role on the
// AtlasDevFrameScheduler. takes length=<frames per full day cycle,
// multiple of 8, default 2048, max 65528>; without the scheduler
// installed first the install refuses. the body dims the three
// background palette rows from the engine's own palette shadow and
// leaves the hud row untouched.
//
// the role is on at boot (the scheduler's arm table seeds slot 0 with
// the day/night kind) and stays script controllable: the
// AtlasDevDayNight and AtlasDevArmRole opcodes write the slot bytes, and
// the body checks them every call. when the role is switched off while
// the screen is dim, an 8 call sweep rewrites the three rows at full
// daylight before the role goes quiet, so switching off at midnight
// does not strand a dark screen.

namespace {
	constexpr word DN_ORG{ 0x8000 };            // bank 9 window
	constexpr word LEVEL{ 0x04e2 }, PHASE{ 0x04e3 };
	constexpr word DIV_LO{ 0x04e4 }, DIV_HI{ 0x04e5 }, RESTORE{ 0x04e6 };
	constexpr word PAL_SHADOW{ 0x0293 };
	constexpr byte KIND_DAYNIGHT{ 0x02 };
	constexpr byte DAY_TABLE[8]{ 0x00, 0x00, 0x10, 0x10, 0x20, 0x10, 0x10, 0x00 };
}

word fh::HackManager::install_AtlasDevDayNightCycle(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	using namespace fh::afs;
	const word base{ find_base(p_rom) };
	if (base == 0)
		throw std::runtime_error("AtlasDevDayNightCycle requires the AtlasDevFrameScheduler hack installed first");
	const word length{ p_hack.word_or("length", 2048) };
	if (length < 8 || length % 8)
		throw std::runtime_error("AtlasDevDayNightCycle: length must be a multiple of 8");
	const word reload{ static_cast<word>(length / 8 - 1) };

	klib::Asm6502 code;
	// enable gate: run only while some scheduler slot holds our kind
	for (word slot : { RAM_SLOT0, RAM_SLOT1, RAM_SLOT2 }) {
		code.lda_abs(slot);
		code.cmp_imm(KIND_DAYNIGHT);
		code.beq("@enabled");
	}
	// switched off: if the screen is still dim, arm an 8 call restore
	// sweep. 8 calls is one full row selector cycle, so every background
	// row is rewritten from the shadow at daylight, then the role goes
	// quiet. the level byte alone cannot gate this, because the body
	// zeroes it on the first restore call and two rows would stay dim.
	code.lda_abs(LEVEL);
	code.beq("@chkrest");
	code.lda_imm(0x08); code.sta_abs(RESTORE);
	code.lda_imm(0x00); code.sta_abs(LEVEL); code.sta_abs(PHASE);
	code.label("@chkrest");
	code.lda_abs(RESTORE);
	code.bne("@sweep");
	code.rts();
	code.label("@sweep");
	code.dec_abs(RESTORE);
	code.jmp("@body");
	code.label("@enabled");
	// divider: countdown per call; on zero reload and advance the phase
	code.lda_abs(DIV_LO);
	code.ora_abs(DIV_HI);
	code.bne("@tick");
	code.lda_imm(static_cast<byte>(reload & 0xff)); code.sta_abs(DIV_LO);
	code.lda_imm(static_cast<byte>(reload >> 8)); code.sta_abs(DIV_HI);
	code.inc_abs(PHASE);
	code.jmp("@body");
	code.label("@tick");
	code.lda_abs(DIV_LO);
	code.bne("@nolo");
	code.dec_abs(DIV_HI);
	code.label("@nolo");
	code.dec_abs(DIV_LO);
	// body: one background palette row per call, dimmed by the phase level
	code.label("@body");
	code.lda_abs(PHASE);
	code.and_imm(0x07);
	code.tax();
	code.lda_abs_x("@table");
	code.sta_abs(LEVEL);
	code.lda_abs(RAM_CNT_LO);
	code.lsr_a();
	code.and_imm(0x03);
	code.cmp_imm(0x03);                        // group 3 is the hud row: skip
	code.beq("@done");
	code.asl_a(); code.asl_a();
	code.tax();
	code.lda_imm(0x3f); code.sta_abs(0x2006);
	code.txa(); code.sta_abs(0x2006);
	code.label("@col");
	code.lda_abs_x(PAL_SHADOW);
	code.cmp_imm(0x10);
	code.bcc("@wr");                           // already darkest: keep it
	code.sec();
	code.sbc_abs(LEVEL);
	code.bcs("@wr");
	code.lda_imm(0x0f);
	code.label("@wr");
	code.sta_abs(0x2007);
	code.inx();
	code.txa(); code.and_imm(0x03);
	code.bne("@col");
	code.lda_imm(0x00);
	code.sta_abs(0x2006); code.sta_abs(0x2006);
	code.label("@done");
	code.rts();
	code.label("@table");
	for (byte b : DAY_TABLE) code.db(b);

	// the bank 9 window must be free; then wire through the published abi
	const auto off9{ klib::Asm6502::get_file_offset(9, DN_ORG) };
	for (std::size_t i{ 0 }; i < code.size(); ++i)
		if (p_rom[off9 + i] != 0xff)
			throw std::runtime_error("AtlasDevDayNightCycle: bank 9 window at $8000 is not free");
	const auto scheduler{ klib::Asm6502::get_file_offset(15, base) };
	code.apply_hack_and_clear(p_rom, 9, DN_ORG);
	p_rom[scheduler + OFF_POST] = DN_ORG & 0xff;
	p_rom[scheduler + OFF_POST + 1] = DN_ORG >> 8;
	p_rom[scheduler + OFF_POSTARMED] = 0x01;
	p_rom[scheduler + OFF_ARM0] = KIND_DAYNIGHT;

	return cpu_addr;
}
