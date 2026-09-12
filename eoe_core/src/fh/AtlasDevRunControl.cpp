#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <algorithm>
#include <cctype>
#include <format>
#include <stdexcept>

// run control: double tap a direction to run. the second tap inside the
// window raises the ramp's cap from $0180 to speed*32 and adds accel each
// frame; releasing, turning on the ground, a hit or a ladder drop back to
// walking. one ram byte STATE holds the running bit (7) and the window
// countdown (bits 0..5); the reset routine clears $0200 to $07ff so it
// starts at zero.
//
// four vanilla instructions in the bank 15 movement handler are retargeted
// to stubs at the general hack cursor: the two speed reset calls at $e1d5
// (right) and $e226 (left), the idle tail at $e1be and the cap compare at
// $e2a3, which the stub answers with the same carry the vanilla compare
// produced; while not running that path is the vanilla compare itself, so
// the hack is invisible until the second tap. the stubs run in the main
// loop, so the nmi budget is untouched.
//
// kind=N makes the hack a client of the AtlasDevFrameScheduler, which must
// be installed earlier: every stub runs the vanilla bytes unless a slot
// holds N, so AtlasDevArmRole N, 1 and AtlasDevArmRole N, 0 switch it.
namespace {
	constexpr word STATE{ 0x04f6 };
	constexpr byte ZP_JOY{ 0x16 }, ZP_A3{ 0xa3 }, ZP_A4{ 0xa4 }, ZP_A9{ 0xa9 }, ZP_AA{ 0xaa };
	constexpr word SITE_START_R{ 0xe1d5 }, SITE_START_L{ 0xe226 }, SITE_IDLE{ 0xe1be }, SITE_CAP{ 0xe2a3 };
	constexpr word V_SPEED_RESET{ 0xe27f };
	constexpr byte START_ORIG[3]{ 0x20, 0x7f, 0xe2 };
	constexpr byte IDLE_ORIG[7]{ 0xa5, 0xa4, 0x29, 0xdf, 0x85, 0xa4, 0x60 };
	constexpr byte CAP_ORIG[8]{ 0xa5, 0xa9, 0xc9, 0x80, 0xa5, 0xaa, 0xe9, 0x01 };
	constexpr byte RESET_ORIG[9]{ 0xa9, 0xc0, 0x85, 0xa9, 0xa9, 0x00, 0x85, 0xaa, 0x60 };

	struct Settings {
		byte speed, accel, window, walk_cycle, kind;
		bool instant, boot;
		word cap(void) const { return static_cast<word>(speed * 32); }
		byte cap_lo(void) const { return static_cast<byte>(cap() & 0xff); }
		byte cap_hi(void) const { return static_cast<byte>(cap() >> 8); }
		bool gated(void) const { return kind != 0; }
	};

	void require_site(const std::vector<byte>& p_rom, word p_addr, const byte* p_orig, std::size_t p_size) {
		const auto off{ klib::Asm6502::get_file_offset(15, p_addr) };
		for (std::size_t i{ 0 }; i < p_size; ++i)
			if (p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format(
					"AtlasDevRunControl: the vanilla site at ${:04x} is not intact", p_addr));
	}

	// the same layout and bytes as tools/build_run_control.py body()
	void emit_body(klib::Asm6502& code, const Settings& s) {
		using namespace fh::afs;
		if (s.gated()) {
			code.label("@armed");
			const word slots[3]{ RAM_SLOT0, RAM_SLOT1, RAM_SLOT2 };
			for (std::size_t i{ 0 }; i < 3; ++i) {
				code.lda_abs(slots[i]);
				code.cmp_imm(s.kind);
				if (i < 2) code.beq("@armed_rts");
			}
			code.label("@armed_rts");
			code.rts();
		}
		code.label("@start_right");
		code.lda_zp(ZP_A4); code.and_imm(0x40);
		code.bne("@start_same");
		code.beq("@start_no");
		code.label("@start_left");
		code.lda_zp(ZP_A4); code.and_imm(0x40);
		code.bne("@start_no");
		code.label("@start_same");
		if (s.gated()) { code.jsr("@armed"); code.bne("@start_off"); }
		code.lda_zp(ZP_JOY); code.and_imm(0x03);
		code.beq("@start_no");
		code.lda_abs(STATE); code.and_imm(0x3f);
		code.beq("@start_no");
		code.lda_imm(0x80); code.sta_abs(STATE);
		if (s.instant) {
			code.lda_imm(s.cap_lo()); code.sta_zp(ZP_A9);
			code.lda_imm(s.cap_hi()); code.sta_zp(ZP_AA);
			code.rts();
		}
		else
			code.jmp(V_SPEED_RESET);
		code.label("@start_no");
		code.lda_imm(0x00); code.sta_abs(STATE);
		if (s.gated()) code.label("@start_off");
		code.jmp(V_SPEED_RESET);

		code.label("@idle");
		if (s.gated()) { code.jsr("@armed"); code.bne("@idle_off"); }
		code.lda_zp(ZP_A4); code.and_imm(0x20);
		code.beq("@idle_tick");
		code.lda_imm(s.window); code.sta_abs(STATE);
		code.bne("@idle_v");
		code.label("@idle_tick");
		code.lda_abs(STATE); code.and_imm(0x3f);
		code.beq("@idle_v");
		code.dec_abs(STATE);
		if (s.gated()) {
			code.jmp("@idle_v");
			code.label("@idle_off");
			code.lda_imm(0x00); code.sta_abs(STATE);
		}
		code.label("@idle_v");
		code.lda_zp(ZP_A4); code.and_imm(0xdf); code.sta_zp(ZP_A4);
		code.rts();

		code.label("@cap");
		if (s.gated()) { code.jsr("@armed"); code.bne("@cap_walk"); }
		code.lda_abs(STATE);
		code.bpl("@cap_walk");
		code.lda_zp(ZP_A4); code.and_imm(0x05);
		code.bne("@cap_run");
		code.lda_zp(ZP_JOY); code.and_imm(0x01);
		code.beq("@cap_lheld");
		code.lda_zp(ZP_A4); code.and_imm(0x40);
		code.bne("@cap_run");
		code.beq("@cap_stop");
		code.label("@cap_lheld");
		code.lda_zp(ZP_A4); code.and_imm(0x40);
		code.bne("@cap_stop");
		code.label("@cap_run");
		if (s.accel) {
			code.lda_zp(ZP_A9); code.clc(); code.adc_imm(s.accel); code.sta_zp(ZP_A9);
			code.lda_zp(ZP_AA); code.adc_imm(0x00); code.sta_zp(ZP_AA);
		}
		code.lda_zp(ZP_A9); code.cmp_imm(s.cap_lo());
		code.lda_zp(ZP_AA); code.sbc_imm(s.cap_hi());
		code.bcc("@cap_anim");
		code.lda_imm(s.cap_lo()); code.sta_zp(ZP_A9);
		code.lda_imm(s.cap_hi()); code.sta_zp(ZP_AA);
		code.sec();
		code.label("@cap_anim");
		for (byte k{ 1 }; k < s.walk_cycle; ++k) code.inc_zp(ZP_A3);
		code.rts();
		code.label("@cap_stop");
		code.lda_abs(STATE); code.and_imm(0x7f); code.sta_abs(STATE);
		code.lda_imm(0x80); code.sta_zp(ZP_A9);
		code.lda_imm(0x01); code.sta_zp(ZP_AA);
		code.sec();
		code.rts();
		code.label("@cap_walk");
		code.lda_zp(ZP_A9); code.cmp_imm(0x80);
		code.lda_zp(ZP_AA); code.sbc_imm(0x01);
		code.rts();
	}

	byte ranged(const fh::GeneralHack& p_hack, const std::string& p_id, byte p_default, int p_lo, int p_hi) {
		const int value{ p_hack.byte_or(p_id, p_default) };
		if (value < p_lo || value > p_hi)
			throw std::runtime_error(std::format("AtlasDevRunControl: {} must be {} to {}", p_id, p_lo, p_hi));
		return static_cast<byte>(value);
	}
}

word fh::HackManager::install_AtlasDevRunControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	using namespace fh::afs;
	std::string mode{ p_hack.string_or("mode", "ramp") };
	std::transform(mode.begin(), mode.end(), mode.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (mode != "ramp" && mode != "instant")
		throw std::runtime_error("AtlasDevRunControl: mode must be ramp or instant");
	const Settings s{ ranged(p_hack, "speed", 20, 13, 64), ranged(p_hack, "accel", 16, 0, 255),
		ranged(p_hack, "window", 12, 1, 63), ranged(p_hack, "walk_cycle", 1, 1, 4),
		p_hack.byte_or("kind", 0), mode == "instant", p_hack.bool_or("boot", true) };

	// ownership checks first, so a refused install leaves the ROM byte identical
	std::size_t scheduler{ 0 };
	std::size_t arm_site{ OFF_ARM0 + 3 };
	if (s.gated()) {
		const word base{ find_base(p_rom) };
		if (base == 0)
			throw std::runtime_error("AtlasDevRunControl: kind needs the AtlasDevFrameScheduler hack installed first");
		scheduler = klib::Asm6502::get_file_offset(15, base);
		if (s.boot) {
			const auto read_operand{ [&p_rom, scheduler](std::size_t site) {
				return static_cast<word>(p_rom[scheduler + site] | (p_rom[scheduler + site + 1] << 8));
			} };
			const word stub_target{ static_cast<word>(base + OFF_STUB) };
			constexpr std::size_t pre_sites[3]{ OFF_PRE0, OFF_PRE1, OFF_PRE2 };
			for (std::size_t i{ 0 }; i < 3; ++i)
				if (p_rom[scheduler + OFF_ARM0 + i] == s.kind) {
					if (read_operand(pre_sites[i]) != stub_target)
						throw std::runtime_error(std::format("AtlasDevRunControl: scheduler kind {} slot has a PRE claimant", s.kind));
					arm_site = OFF_ARM0 + i;
					break;
				}
			if (arm_site == OFF_ARM0 + 3)
				for (std::size_t i{ 0 }; i < 3; ++i)
					if (p_rom[scheduler + OFF_ARM0 + i] == 0x00 && read_operand(pre_sites[i]) == stub_target) {
						arm_site = OFF_ARM0 + i;
						break;
					}
			if (arm_site == OFF_ARM0 + 3)
				throw std::runtime_error("AtlasDevRunControl: scheduler arm table has no unclaimed slot");
		}
	}
	require_site(p_rom, SITE_START_R, START_ORIG, sizeof(START_ORIG));
	require_site(p_rom, SITE_START_L, START_ORIG, sizeof(START_ORIG));
	require_site(p_rom, SITE_IDLE, IDLE_ORIG, sizeof(IDLE_ORIG));
	require_site(p_rom, SITE_CAP, CAP_ORIG, sizeof(CAP_ORIG));
	require_site(p_rom, V_SPEED_RESET, RESET_ORIG, sizeof(RESET_ORIG));

	klib::Asm6502 code;
	emit_body(code, s);
	const auto size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("AtlasDevRunControl: bank 15 space at ${:04x} overflows the ROM image or is not free", cpu_addr + i));

	const word start_r{ static_cast<word>(cpu_addr + code.label_position("@start_right")) };
	const word start_l{ static_cast<word>(cpu_addr + code.label_position("@start_left")) };
	const word idle{ static_cast<word>(cpu_addr + code.label_position("@idle")) };
	const word cap{ static_cast<word>(cpu_addr + code.label_position("@cap")) };
	code.apply_hack_and_clear(p_rom, 15, cpu_addr);
	code.jsr(start_r); code.apply_hack_and_clear(p_rom, 15, SITE_START_R);
	code.jsr(start_l); code.apply_hack_and_clear(p_rom, 15, SITE_START_L);
	code.jmp(idle); code.nop(4); code.apply_hack_and_clear(p_rom, 15, SITE_IDLE);
	code.jsr(cap); code.nop(5); code.apply_hack_and_clear(p_rom, 15, SITE_CAP);
	if (s.gated() && s.boot)
		p_rom[scheduler + arm_site] = s.kind;
	return static_cast<word>(cpu_addr + size);
}
