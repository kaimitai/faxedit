#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <array>
#include <format>
#include <stdexcept>

// jump control: coyote time, a jump buffer, a short hop on early release
// and optional air jumps for the hero's jump. each feature is a parameter
// from 0 to 15 and 0 turns it off; with all four at 0 nothing is installed.
//
// three vanilla instructions in the bank 15 vertical handler are retargeted
// to stubs placed at the general hack cursor: the free fall entry at $e3cd
// (lda $a5 / bpl to the next instruction, a vanilla no op), the jump init
// entry at $e444 and the arc entry at $e463. the stubs run in the main loop,
// so the nmi budget is untouched.
//
// state is one ram byte, STATE below: the low nibble is the jump buffer
// countdown and the high nibble the air jumps left. two vanilla cells are
// used with their vanilla meaning: $b1, the fall grace counter the game only
// touches in this handler, counts frames in free fall; bit 1 of $a4, set at
// jump start and cleared only on a grounded frame without a press, refuses
// coyote time after an arc that ended in the air. the reset routine clears
// $0200 to $07ff, so STATE starts at zero.
//
// the short hop is exact because the 32 byte arc table is a mirror: after p
// ascent phases the descent from index 32 - p adds back exactly the rise.
// every site, exit, probe and the table are byte identical in the us, us
// rev a, eu and jp roms; the installer verifies them and refuses otherwise.
//
// switchable=1 makes the hack a script controlled client of the
// AtlasDevFrameScheduler, which must then be installed earlier: kind $86 is
// claimed in the next free boot slot (or none with armed=0), every stub
// runs only while some slot holds that kind, and AtlasDevArmRole $86, state
// switches it. switched off, the stubs do exactly what the replaced
// instructions did and clear the state byte. the kind is gate only, so the
// tick never calls the slot; the price is the slot gate in each stub.

namespace {
	constexpr word STATE{ 0x04df };

	constexpr byte ZP_A4{ 0xa4 }, ZP_A5{ 0xa5 }, ZP_A6{ 0xa6 }, ZP_B1{ 0xb1 };
	constexpr byte JOY{ 0x16 }, EDGE{ 0x19 };

	constexpr word FALL_HOOK{ 0xe3cd }, FALL_CONT{ 0xe3d1 }, JUMP_PATH{ 0xe43a };
	constexpr word INIT_HOOK{ 0xe444 }, ACCEPT{ 0xe44d }, NOJUMP{ 0xe433 };
	constexpr word ARC_HOOK{ 0xe463 }, LADDER{ 0xecf6 }, PROBE{ 0xe6c8 };
	constexpr word ARC_TABLE_ADDR{ 0xe4d6 };

	constexpr byte FALL_ORIG[4]{ 0xa5, 0xa5, 0x10, 0x00 };
	constexpr byte INIT_ORIG[5]{ 0xa5, 0xa4, 0x4a, 0xb0, 0x1a };
	constexpr byte ARC_ORIG[4]{ 0xa6, 0xa6, 0xe0, 0x10 };
	constexpr word INIT_VANILLA{ 0xe449 };
	constexpr byte KIND_JUMP{ 0x86 };            // gate only: the tick never calls it
	// extended flags: flag n lives at $0101 + (n >> 3), mask 1 << (n & 7)
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr byte MAX_FLAG{ 247 };
	constexpr byte NO_FLAG{ 0xff };

	struct Settings {
		byte coyote, buffer, shorthop, airjumps, switchable, flag;
		bool gated(void) const { return switchable || flag != NO_FLAG; }
		bool want_fall(void) const { return coyote || buffer || airjumps; }
		bool want_init(void) const { return buffer || airjumps; }
		bool want_hop(void) const { return shorthop != 0; }
		bool any(void) const { return want_fall() || want_init() || want_hop(); }
	};

	// fall through when the hack is off, else branch: off is no scheduler
	// slot holding our kind (switchable) or the extended flag clear (flag)
	void emit_gate(klib::Asm6502& code, const Settings& s, const std::string& p_on) {
		using namespace fh::afs;
		if (s.switchable) {
			for (word slot : { RAM_SLOT0, RAM_SLOT1, RAM_SLOT2 }) {
				code.lda_abs(slot);
				code.cmp_imm(KIND_JUMP);
				code.beq(p_on);
			}
		}
		else {
			code.lda_abs(static_cast<word>(FLAG_BASE + (s.flag >> 3)));
			code.and_imm(static_cast<byte>(1 << (s.flag & 7)));
			code.bne(p_on);
		}
	}

	// arm the buffer nibble on an a edge, else run it down; carry = pressed
	void emit_buf(klib::Asm6502& code, const Settings& s) {
		code.label("@buf");
		code.lda_zp(EDGE);
		code.bpl("@b_rd");
		code.lda_abs(STATE);
		code.and_imm(0xf0);
		code.ora_imm(s.buffer);
		code.sta_abs(STATE);
		code.sec();
		code.rts();
		code.label("@b_rd");
		code.lda_abs(STATE);
		code.and_imm(0x0f);
		code.beq("@b_done");
		code.dec_abs(STATE);
		code.label("@b_done");
		code.clc();
		code.rts();
	}

	// every free fall frame; exits to $e3d1 (keep falling) or $e43a (jump)
	void emit_fall(klib::Asm6502& code, const Settings& s) {
		code.label("@fall");
		if (s.gated()) {
			emit_gate(code, s, "@f_on");
			code.jmp("@f_fall");
			code.label("@f_on");
		}
		if (s.coyote) {
			code.inc_zp(ZP_B1);
			code.bne("@f_cnt");
			code.dec_zp(ZP_B1);
			code.label("@f_cnt");
		}
		const bool trying{ s.coyote || s.airjumps };
		if (s.buffer) {
			code.jsr("@buf");
			if (trying)
				code.bcc("@f_fall");
		}
		else {
			code.lda_zp(EDGE);
			code.bpl("@f_fall");
		}
		if (trying) {
			code.jsr(LADDER);
			code.bcs("@f_fall");
			code.ldx_imm(0x02);
			code.jsr(PROBE);
			code.bne("@f_fall");
		}
		if (s.coyote) {
			code.lda_zp(ZP_A4);
			code.and_imm(0x02);
			if (s.airjumps) {
				code.bne("@f_air");
				code.lda_zp(ZP_B1);
				code.cmp_imm(static_cast<byte>(s.coyote + 1));
				code.bcc("@f_start");
			}
			else {
				code.bne("@f_fall");
				code.lda_zp(ZP_B1);
				code.cmp_imm(static_cast<byte>(s.coyote + 1));
				code.bcs("@f_fall");
			}
		}
		if (s.airjumps) {
			code.label("@f_air");
			code.lda_zp(ZP_A5);
			code.bmi("@f_fall");
			code.lda_abs(STATE);
			code.and_imm(0xf0);
			code.beq("@f_fall");
			code.sec();
			code.sbc_imm(0x10);
			code.sta_abs(STATE);
		}
		if (trying) {
			code.label("@f_start");
			code.lda_zp(ZP_A4);
			code.ora_imm(0x03);
			code.sta_zp(ZP_A4);
			code.lda_imm(0x00);
			code.sta_zp(ZP_A6);
			if (s.buffer) {
				code.lda_abs(STATE);
				code.and_imm(0xf0);
				code.sta_abs(STATE);
			}
			code.jmp(JUMP_PATH);
		}
		code.label("@f_fall");
		code.jmp(FALL_CONT);
	}

	// every other frame; exits to $e463 (arc), $e44d (accept) or $e433
	void emit_init(klib::Asm6502& code, const Settings& s) {
		code.label("@init");
		if (s.gated()) {
			emit_gate(code, s, "@i_on");
			// switched off: forget any buffered press, then do what the
			// replaced instruction did and hand vanilla its own press check
			code.lda_imm(0x00);
			code.sta_abs(STATE);
			code.lda_zp(ZP_A4);
			code.lsr_a();
			code.bcs("@i_arc");
			code.jmp(INIT_VANILLA);
			code.label("@i_on");
		}
		code.lda_zp(ZP_A4);
		code.lsr_a();
		code.bcc("@i_gnd");
		code.lda_zp(ZP_A6);
		code.beq("@i_arc");
		if (s.buffer) {
			code.jsr("@buf");
			if (s.airjumps)
				code.bcc("@i_arc");
		}
		else {
			code.lda_zp(EDGE);
			code.bpl("@i_arc");
		}
		if (s.airjumps) {
			code.lda_zp(ZP_A5);
			code.bmi("@i_arc");
			code.lda_abs(STATE);
			code.and_imm(0xf0);
			code.beq("@i_arc");
			code.ldx_imm(0x02);
			code.jsr(PROBE);
			code.bne("@i_arc");
			code.lda_abs(STATE);
			code.and_imm(0xf0);
			code.sec();
			code.sbc_imm(0x10);
			code.sta_abs(STATE);
			code.lda_imm(0x00);
			code.sta_zp(ZP_A6);
		}
		code.label("@i_arc");
		code.jmp(ARC_HOOK);
		code.label("@i_gnd");
		if (s.airjumps) {
			code.lda_abs(STATE);
			code.and_imm(0x0f);
			code.ora_imm(static_cast<byte>(s.airjumps << 4));
			code.sta_abs(STATE);
		}
		code.ldx_zp(EDGE);
		code.bmi("@i_acc");
		if (s.buffer) {
			code.lda_abs(STATE);
			code.and_imm(0x0f);
			code.beq("@i_no");
			code.label("@i_acc");
			code.lda_abs(STATE);
			code.and_imm(0xf0);
			code.sta_abs(STATE);
		}
		else {
			code.jmp("@i_no");
			code.label("@i_acc");
		}
		code.jmp(ACCEPT);
		code.label("@i_no");
		code.jmp(NOJUMP);
	}

	// every arc frame; ends cpx #$10 / rts so the vanilla branches still work
	void emit_hop(klib::Asm6502& code, const Settings& s) {
		code.label("@hop");
		code.ldx_zp(ZP_A6);
		if (s.gated()) {
			emit_gate(code, s, "@h_on");
			code.jmp("@h_done");
			code.label("@h_on");
		}
		code.lda_zp(JOY);
		code.bmi("@h_done");
		code.cpx_imm(s.shorthop);
		code.bcc("@h_done");
		code.cpx_imm(0x10);
		code.bcs("@h_done");
		code.lda_imm(0x20);
		code.sec();
		code.sbc_zp(ZP_A6);
		code.sta_zp(ZP_A6);
		code.tax();
		code.label("@h_done");
		code.cpx_imm(0x10);
		code.rts();
	}

	// profile=name sets coyote, buffer, shorthop and airjumps at once, in the
	// spirit of the game named; an explicit knob overrides it. no profile
	// keeps the defaults below.
	struct JumpProfile { const char* name; byte coyote, buffer, shorthop, airjumps; };
	constexpr std::array<JumpProfile, 10> JUMP_PROFILES{ {
		{ "vanilla",        0, 0, 0, 0 },
		{ "zelda2",         3, 3, 1, 0 },
		{ "metroid",        5, 5, 3, 0 },
		{ "megaman",        0, 2, 1, 0 },
		{ "castlevania",    0, 0, 0, 0 },
		{ "ninjagaiden",    2, 3, 0, 0 },
		{ "ghostsngoblins", 0, 0, 0, 0 },
		{ "kidicarus",      4, 4, 2, 0 },
		{ "contra",         2, 3, 0, 0 },
		{ "arcade",         5, 5, 3, 1 },
	} };

	const JumpProfile& jump_profile(const fh::GeneralHack& p_hack) {
		static constexpr JumpProfile DEFAULT{ "", 5, 5, 3, 0 };
		if (!p_hack.has_param("profile")) return DEFAULT;
		const std::string name{ p_hack.get_string("profile") };
		for (const JumpProfile& p : JUMP_PROFILES) if (name == p.name) return p;
		throw std::runtime_error(std::format("AtlasDevJumpControl: unknown profile '{}'", name));
	}

	byte setting(const fh::GeneralHack& p_hack, const std::string& p_id, byte p_default) {
		const byte value{ p_hack.byte_or(p_id, p_default) };
		if (value > 15)
			throw std::runtime_error(std::format(
				"AtlasDevJumpControl: {} must be 0 to 15", p_id));
		return value;
	}

	void require_site(const std::vector<byte>& p_rom, word p_addr, const byte* p_orig, std::size_t p_size) {
		const auto off{ klib::Asm6502::get_file_offset(15, p_addr) };
		for (std::size_t i{ 0 }; i < p_size; ++i)
			if (p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format(
					"AtlasDevJumpControl: the vanilla site at ${:04x} is not intact", p_addr));
	}
}

word fh::HackManager::install_AtlasDevJumpControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const byte switchable{ p_hack.byte_or("switchable", 0) };
	if (switchable > 1)
		throw std::runtime_error("AtlasDevJumpControl: switchable must be 0 or 1");
	const byte armed{ p_hack.byte_or("armed", 1) };
	if (armed > 1)
		throw std::runtime_error("AtlasDevJumpControl: armed must be 0 or 1");
	const JumpProfile& base{ jump_profile(p_hack) };
	const byte flag{ p_hack.has_param("flag") ? p_hack.byte_or("flag", 0) : NO_FLAG };
	if (flag != NO_FLAG && flag > MAX_FLAG)
		throw std::runtime_error(std::format("AtlasDevJumpControl: flag must be 0 to {}", MAX_FLAG));
	if (flag != NO_FLAG && switchable)
		throw std::runtime_error("AtlasDevJumpControl: flag and switchable are two runtime switches; use one");
	if (flag != NO_FLAG && p_hack.has_param("armed"))
		throw std::runtime_error("AtlasDevJumpControl: armed belongs to switchable; a flag install is armed by the flag");
	const Settings s{ setting(p_hack, "coyote", base.coyote), setting(p_hack, "buffer", base.buffer),
		setting(p_hack, "shorthop", base.shorthop), setting(p_hack, "airjumps", base.airjumps), switchable, flag };
	if (!s.any())
		return cpu_addr;

	// a switchable install is a scheduler client: the scheduler must be
	// there, and a boot slot is free only while its arm byte is zero and
	// its PRE vector still points to the scheduler's own stub
	using namespace fh::afs;
	std::size_t scheduler{ 0 };
	std::size_t arm_slot{ 3 };
	if (s.switchable) {
		const word base{ find_base(p_rom) };
		if (base == 0)
			throw std::runtime_error("AtlasDevJumpControl: switchable=1 requires the AtlasDevFrameScheduler hack installed first");
		scheduler = klib::Asm6502::get_file_offset(15, base);
		if (armed) {
			const std::size_t pre[3]{ OFF_PRE0, OFF_PRE1, OFF_PRE2 };
			for (std::size_t i{ 0 }; i < 3 && arm_slot == 3; ++i) {
				const word vec{ static_cast<word>(p_rom[scheduler + pre[i]] | (p_rom[scheduler + pre[i] + 1] << 8)) };
				if (p_rom[scheduler + OFF_ARM0 + i] == 0x00 && vec == static_cast<word>(base + OFF_STUB))
					arm_slot = i;
			}
			if (arm_slot == 3)
				throw std::runtime_error("AtlasDevJumpControl: no free scheduler slot");
		}
	}

	klib::Asm6502 code;
	if (s.want_fall())
		emit_fall(code, s);
	if (s.want_init())
		emit_init(code, s);
	if (s.want_hop())
		emit_hop(code, s);
	if (s.buffer)
		emit_buf(code, s);
	const auto size{ code.size() };

	// every ownership and capacity check is complete before mutation
	if (s.want_fall())
		require_site(p_rom, FALL_HOOK, FALL_ORIG, sizeof(FALL_ORIG));
	if (s.want_init())
		require_site(p_rom, INIT_HOOK, INIT_ORIG, sizeof(INIT_ORIG));
	if (s.want_hop()) {
		require_site(p_rom, ARC_HOOK, ARC_ORIG, sizeof(ARC_ORIG));
		const auto table{ klib::Asm6502::get_file_offset(15, ARC_TABLE_ADDR) };
		for (std::size_t k{ 0 }; k < 16; ++k)
			if (p_rom[table + k] != p_rom[table + 31 - k])
				throw std::runtime_error("AtlasDevJumpControl: the jump arc table is not a mirror");
	}
	const auto off{ klib::Asm6502::get_file_offset(15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format(
				"AtlasDevJumpControl: bank 15 space at ${:04x} is not free", cpu_addr + i));

	const word fall{ static_cast<word>(cpu_addr + (s.want_fall() ? code.label_position("@fall") : 0)) };
	const word init{ static_cast<word>(cpu_addr + (s.want_init() ? code.label_position("@init") : 0)) };
	const word hop{ static_cast<word>(cpu_addr + (s.want_hop() ? code.label_position("@hop") : 0)) };
	code.apply_hack_and_clear(p_rom, 15, cpu_addr);
	if (s.want_fall()) {
		code.jmp(fall);
		code.nop(1);
		code.apply_hack_and_clear(p_rom, 15, FALL_HOOK);
	}
	if (s.want_init()) {
		code.jmp(init);
		code.nop(2);
		code.apply_hack_and_clear(p_rom, 15, INIT_HOOK);
	}
	if (s.want_hop()) {
		code.jsr(hop);
		code.nop(1);
		code.apply_hack_and_clear(p_rom, 15, ARC_HOOK);
	}
	if (s.switchable && armed)
		p_rom[scheduler + OFF_ARM0 + arm_slot] = KIND_JUMP;
	return static_cast<word>(cpu_addr + size);
}
