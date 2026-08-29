#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <stdexcept>

// infected tint: rewrites sprite palette 0 entries 1 to 3 with three
// colors plus a pulse offset taken from the scheduler's frame counter,
// running as a POST role on the AtlasDevFrameScheduler. takes
// colors=<three palette values, plus separated, default $09+$19+$29>
// and pulse=<power of two mask, default $20, 0 for a steady tint>.
//
// this is the first POST chain: installed alone it takes the POST
// vector and returns; installed after another POST role its body lands
// at the next free bank 9 spot and ends by jumping to the previous
// vector, so both roles run every call. each role stays independently
// toggleable through its own slot kind, so AtlasDevArmRole 3, state
// switches the tint without touching the day cycle. switching off
// restores the three palette bytes from the engine's own shadow in one
// call, then the role goes quiet.

namespace {
	constexpr word TINTED{ 0x04e7 }, PULSE{ 0x04e8 };
	constexpr word SPR_SHADOW{ 0x02a4 };        // shadow for $3F11-$3F13
	constexpr byte KIND_TINT{ 0x03 };
	constexpr byte DEFAULT_COLORS[3]{ 0x09, 0x19, 0x29 };

	void emit_tint(klib::Asm6502& code, const byte p_colors[3], byte p_pulse_mask,
		bool p_chain, word p_chain_to) {
		using namespace fh::afs;
		// enable gate: run only while some scheduler slot holds our kind
		for (word slot : { RAM_SLOT0, RAM_SLOT1, RAM_SLOT2 }) {
			code.lda_abs(slot);
			code.cmp_imm(KIND_TINT);
			code.beq("@enabled");
		}
		// switched off: restore once from the shadow, then stay quiet
		code.lda_abs(TINTED);
		code.bne("@restore");
		code.jmp("@tail");
		code.label("@restore");
		code.lda_imm(0x00);
		code.sta_abs(TINTED);
		code.lda_imm(0x3f); code.sta_abs(0x2006);
		code.lda_imm(0x11); code.sta_abs(0x2006);
		for (word i{ 0 }; i < 3; ++i) {
			code.lda_abs(static_cast<word>(SPR_SHADOW + i));
			code.sta_abs(0x2007);
		}
		code.jmp("@park");
		code.label("@enabled");
		// pulse from the frame counter: (counter and mask) >> 1
		code.lda_abs(RAM_CNT_LO);
		code.and_imm(p_pulse_mask);
		code.lsr_a();
		code.sta_abs(PULSE);
		code.lda_imm(0x01);
		code.sta_abs(TINTED);
		code.lda_imm(0x3f); code.sta_abs(0x2006);
		code.lda_imm(0x11); code.sta_abs(0x2006);
		for (word i{ 0 }; i < 3; ++i) {
			code.lda_abs(PULSE);
			code.clc();
			code.adc_imm(p_colors[i]);
			code.sta_abs(0x2007);
		}
		code.label("@park");
		code.lda_imm(0x00);
		code.sta_abs(0x2006);
		code.sta_abs(0x2006);
		code.label("@tail");
		if (p_chain)
			code.jmp(p_chain_to);
		else
			code.rts();
	}
}

word fh::HackManager::install_AtlasDevInfectedTint(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	using namespace fh::afs;
	const word base{ find_base(p_rom) };
	if (base == 0)
		throw std::runtime_error("AtlasDevInfectedTint requires the AtlasDevFrameScheduler hack installed first");
	byte colors[3]{ DEFAULT_COLORS[0], DEFAULT_COLORS[1], DEFAULT_COLORS[2] };
	if (p_hack.has_param("colors")) {
		const auto given{ p_hack.split_byte_optional_byte("colors") };
		if (given.size() != 3)
			throw std::runtime_error("AtlasDevInfectedTint: colors takes exactly three values");
		for (std::size_t i{ 0 }; i < 3; ++i) {
			if (given[i].second.has_value() || given[i].first > 0x3f)
				throw std::runtime_error("AtlasDevInfectedTint: colors must be plain palette values");
			colors[i] = given[i].first;
		}
	}
	const byte pulse_mask{ p_hack.byte_or("pulse", 0x20) };
	const byte armed{ p_hack.byte_or("armed", 0x01) };
	if (armed > 1)
		throw std::runtime_error("AtlasDevInfectedTint: armed must be 0 or 1");

	// chain if another role already claimed the POST vector; an
	// unclaimed lane must still hold the scheduler's default stub, so
	// an unknown claimant fails the build instead of being overwritten
	const auto scheduler{ klib::Asm6502::get_file_offset(15, base) };
	const bool chain{ p_rom[scheduler + OFF_POSTARMED] != 0x00 };
	const word chain_to{ static_cast<word>(
		p_rom[scheduler + OFF_POST] | (p_rom[scheduler + OFF_POST + 1] << 8)) };
	if (!chain && chain_to != static_cast<word>(base + OFF_STUB))
		throw std::runtime_error(
			"AtlasDevInfectedTint: scheduler POST lane has an unknown claimant");

	klib::Asm6502 code;
	emit_tint(code, colors, pulse_mask, chain, chain_to);

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
		throw std::runtime_error("AtlasDevInfectedTint: no free bank 9 window");

	code.apply_hack_and_clear(p_rom, 9, org);
	p_rom[scheduler + OFF_POST] = org & 0xff;
	p_rom[scheduler + OFF_POST + 1] = org >> 8;
	p_rom[scheduler + OFF_POSTARMED] = 0x01;
	// arm the tint in the next free arm table slot; with armed=0 the
	// body stays dormant until a script arms kind 3 at runtime
	if (armed == 0)
		return cpu_addr;
	for (std::size_t i{ 0 }; i < 3; ++i) {
		if (p_rom[scheduler + OFF_ARM0 + i] == 0x00) {
			p_rom[scheduler + OFF_ARM0 + i] = KIND_TINT;
			return cpu_addr;
		}
	}
	throw std::runtime_error("AtlasDevInfectedTint: no free scheduler slot");
}
