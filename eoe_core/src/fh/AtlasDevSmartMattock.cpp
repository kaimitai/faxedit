#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// smart mattock: a mattock the hero carries digs a rock without being
// selected first. press mode digs on down and b, push mode digs after the
// hero walks into the rock for a while, both does either. the dig itself is
// the vanilla routine, so the message, sound and crumble are unchanged. when
// the mattock is the selected item it is spent as vanilla spends it; when it
// is only carried one is taken from the item list and the selected item is
// left alone. a rock is the area's rock id from the table the dig already
// reads, and a zero id is refused, so the empty tile dig outside trunk does
// not happen while the hack is on.
//
// three vanilla instructions in bank 15 are retargeted: the selected item
// load at $c48b in the down and b check, the counter reset at $e9b9 that
// every non fountain frame of the push check reaches, and the spend call at
// $c64c inside the dig. no ram is claimed: $d7 is the push counter the
// mascon fountain uses, and its branch never reaches the reset.
//
// flag=n gates the hack on extended flag n at run time; a clear flag is
// vanilla at every entry.
//
// every site is verified against its exact vanilla bytes before anything is
// written, and all of them are byte identical in the us, us rev a, eu and jp
// roms. the jp rom differs only inside the vanilla dig routine's own far
// call, which the hack does not touch.
namespace {
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr byte MAX_FLAG{ 247 };
	constexpr byte MATTOCK{ 0x09 };
	constexpr word ITEMS{ 0x03ad }, COUNT{ 0x03c6 }, SELECTED{ 0x03c1 };
	constexpr byte ZP_JOY{ 0x16 }, ZP_A4{ 0xa4 }, ZP_X{ 0x9e }, ZP_Y{ 0xa1 }, ZP_AREA{ 0x24 }, ZP_PUSH{ 0xd7 };
	constexpr byte ARG_X{ 0xb5 }, ARG_Y{ 0xb6 };
	constexpr word SCREEN{ 0x0600 }, DIST_TABLE{ 0xc68d }, ROCK_TABLE{ 0xc68f };
	constexpr word V_DIG{ 0xc616 }, V_AFTER_LDA{ 0xc48e }, V_CLEAR{ 0xc4bf }, V_TO_BLOCK{ 0xe86c };
	constexpr word SITE_PRESS{ 0xc48b }, SITE_PUSH{ 0xe9b9 }, SITE_SPEND{ 0xc64c };
	constexpr byte PRESS_ORIG[3]{ 0xad, 0xc1, 0x03 };
	constexpr byte PUSH_ORIG[4]{ 0xa9, 0x00, 0x85, 0xd7 };
	constexpr byte SPEND_ORIG[3]{ 0x20, 0xbf, 0xc4 };
	constexpr byte TO_BLOCK_ORIG[16]{ 0xa5, 0xb6, 0x29, 0xf0, 0x85, 0x00, 0xa5, 0xb5, 0x4a, 0x4a, 0x4a, 0x4a, 0x05, 0x00, 0xaa, 0x60 };

	struct Settings {
		bool presses, pushes;
		byte push;
		int flag;    // -1 means always on
		bool flagged(void) const { return flag >= 0; }
		word flag_byte(void) const { return static_cast<word>(FLAG_BASE + (flag >> 3)); }
		byte flag_mask(void) const { return static_cast<byte>(1 << (flag & 7)); }
	};

	void require_site(const std::vector<byte>& p_rom, word p_addr, const byte* p_orig, std::size_t p_size) {
		const auto off{ klib::Asm6502::get_file_offset(15, p_addr) };
		for (std::size_t i{ 0 }; i < p_size; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format(
					"AtlasDevSmartMattock: the vanilla site at ${:04x} is not intact", p_addr));
	}

	// klib::Asm6502 has no adc_abs_y or cpx_abs, so those two are raw bytes
	void adc_abs_y(klib::Asm6502& code, word p_addr) {   // ADC $addr,Y
		code.db(0x79); code.db(static_cast<byte>(p_addr & 0xff)); code.db(static_cast<byte>(p_addr >> 8));
	}

	void cpx_abs(klib::Asm6502& code, word p_addr) {     // CPX $addr
		code.db(0xec); code.db(static_cast<byte>(p_addr & 0xff)); code.db(static_cast<byte>(p_addr >> 8));
	}

	void flag_test(klib::Asm6502& code, const Settings& s, const std::string& fail) {
		if (!s.flagged()) return;
		code.lda_abs(s.flag_byte()); code.and_imm(s.flag_mask()); code.beq(fail);
	}

	// the same layout and bytes as tools/build_smart_mattock.py body()
	void emit_body(klib::Asm6502& code, const Settings& s) {
		code.label("@can_dig");
		code.lda_abs(SELECTED); code.cmp_imm(MATTOCK); code.beq("@rock");
		if (s.presses) {
			code.ldx_abs(COUNT); code.cpx_imm(9); code.bcs("@no");
			code.label("@scan");
			code.dex(); code.bmi("@no"); code.lda_abs_x(ITEMS); code.cmp_imm(MATTOCK); code.bne("@scan");
		}
		else {
			code.clc(); code.bcc("@no");
		}
		code.label("@rock");
		code.ldy_imm(0); code.lda_zp(ZP_A4); code.and_imm(0x40); code.beq("@dist"); code.iny();
		code.label("@dist");
		code.lda_zp(ZP_X); code.clc(); adc_abs_y(code, DIST_TABLE); code.cmp_imm(0xf0); code.bcs("@no");
		code.sta_zp(ARG_X); code.lda_zp(ZP_Y); code.sta_zp(ARG_Y); code.jsr(V_TO_BLOCK);
		code.lda_zp(ZP_AREA); code.asl_a(); code.asl_a(); code.tay();
		code.lda_abs_y(ROCK_TABLE); code.beq("@no");
		code.cmp_abs_x(SCREEN); code.bne("@no");
		code.sec(); code.rts();
		code.label("@no"); code.clc(); code.rts();

		code.label("@press");
		flag_test(code, s, "@press_vanilla");
		code.jsr("@can_dig"); code.bcs("@press_dig");
		code.lda_abs(SELECTED); code.cmp_imm(MATTOCK); code.beq("@press_ret");
		code.label("@press_vanilla"); code.lda_abs(SELECTED); code.jmp(V_AFTER_LDA);
		code.label("@press_dig"); code.jmp(V_DIG);
		code.label("@press_ret"); code.rts();

		if (s.pushes) {
			code.label("@push");
			flag_test(code, s, "@push_vanilla");
			code.lda_zp(ZP_JOY); code.and_imm(0x03); code.beq("@push_vanilla");
			code.lda_zp(ZP_A4); code.and_imm(0x05); code.bne("@push_vanilla");
			code.jsr("@can_dig"); code.bcc("@push_vanilla");
			code.inc_zp(ZP_PUSH); code.lda_zp(ZP_PUSH); code.cmp_imm(s.push); code.bcc("@push_ret");
			code.lda_imm(0); code.sta_zp(ZP_PUSH); code.jmp(V_DIG);
			code.label("@push_vanilla"); code.lda_imm(0); code.sta_zp(ZP_PUSH);
			code.label("@push_ret"); code.rts();
		}

		code.label("@spend");
		code.lda_abs(SELECTED); code.cmp_imm(MATTOCK); code.bne("@carried"); code.jmp(V_CLEAR);
		code.label("@carried");
		code.ldx_abs(COUNT); code.cpx_imm(9); code.bcs("@spend_ret");
		code.label("@spend_scan");
		code.dex(); code.bmi("@spend_ret"); code.lda_abs_x(ITEMS); code.cmp_imm(MATTOCK); code.bne("@spend_scan");
		code.inx();
		code.label("@shift");
		cpx_abs(code, COUNT); code.bcs("@done");
		code.lda_abs_x(ITEMS); code.dex(); code.sta_abs_x(ITEMS); code.inx(); code.inx(); code.bne("@shift");
		code.label("@done"); code.dec_abs(COUNT);
		code.label("@spend_ret"); code.rts();
	}
}

word fh::HackManager::install_AtlasDevSmartMattock(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	std::string mode{ p_hack.string_or("mode", "both") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (mode != "vanilla" && mode != "press" && mode != "push" && mode != "both")
		throw std::runtime_error("AtlasDevSmartMattock: mode must be vanilla, press, push or both");
	const bool presses{ mode == "press" || mode == "both" };
	const bool pushes{ mode == "push" || mode == "both" };
	// every parameter is judged in every mode, vanilla included, so a spec the
	// builder would refuse is refused here too
	if (p_hack.has_param("push") && !pushes)
		throw std::runtime_error("AtlasDevSmartMattock: push is only used with mode=push or mode=both");
	const word push_w{ p_hack.word_or("push", 48) };
	if (push_w < 1 || push_w > 255)
		throw std::runtime_error("AtlasDevSmartMattock: push must be 1 to 255 frames");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		const word f{ p_hack.word_or("flag", 0) };
		if (f > MAX_FLAG)
			throw std::runtime_error(std::format("AtlasDevSmartMattock: flag must be 0 to {}", MAX_FLAG));
		flag = static_cast<int>(f);
	}
	if (mode == "vanilla") return cpu_addr;
	const Settings s{ presses, pushes, static_cast<byte>(push_w), flag };

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_PRESS, PRESS_ORIG, sizeof(PRESS_ORIG));
	if (pushes) require_site(p_rom, SITE_PUSH, PUSH_ORIG, sizeof(PUSH_ORIG));
	require_site(p_rom, SITE_SPEND, SPEND_ORIG, sizeof(SPEND_ORIG));
	require_site(p_rom, V_TO_BLOCK, TO_BLOCK_ORIG, sizeof(TO_BLOCK_ORIG));

	klib::Asm6502 code;
	emit_body(code, s);
	const auto size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("AtlasDevSmartMattock: bank 15 space at ${:04x} is not free", cpu_addr + i));

	const word press{ static_cast<word>(cpu_addr + code.label_position("@press")) };
	const word spend{ static_cast<word>(cpu_addr + code.label_position("@spend")) };
	const word push{ pushes ? static_cast<word>(cpu_addr + code.label_position("@push")) : word{ 0 } };
	code.apply_hack_and_clear(p_rom, 15, cpu_addr);
	code.jmp(press); code.apply_hack_and_clear(p_rom, 15, SITE_PRESS);
	code.jsr(spend); code.apply_hack_and_clear(p_rom, 15, SITE_SPEND);
	if (pushes) { code.jmp(push); code.nop(1); code.apply_hack_and_clear(p_rom, 15, SITE_PUSH); }
	return static_cast<word>(cpu_addr + size);
}
