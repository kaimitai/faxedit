#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// sir gawaine and wolfman control: the two sword dwarves step back before
// they strike, then lunge with the sword out. only the thrust
// hurts, and only as far as the sword reaches. vanilla gives their thrust
// the box record at $8a71 (x+48, y+8, width $f0, height 0), which the
// unsigned touch test passes almost anywhere on the dwarf's row, so a thrust
// hits a hero nowhere near the sword; sword=0 keeps that box.
//
// two bank 14 sites are retargeted: the distance call at $94b1 in the
// behaviour both dwarves share, and the first bytes of the touch handler at
// $89ae. the body, the gate and one 14 byte row per monster go to free
// space. the code is the same for every configuration, so whichever name runs
// first installs everything with the other monster's row set to vanilla's
// timing and reach, and the other name then rewrites only its own row. the
// runtime flag is read from the row, so any mix of names and flags works in
// either order. a clear flag, or mode=vanilla, is the vanilla routine: the
// patched sites fall back to vanilla's own code at $94b4 and $89b3, which
// stay untouched, and every site is verified against its vanilla bytes first.
//
// no ram is claimed: $02ec,x is a per sprite behaviour byte the dwarves'
// behaviour never uses; it becomes their attack timer.
namespace {
	constexpr byte BANK{ 14 };
	constexpr byte GAWAINE{ 0x1f }, WOLFMAN{ 0x21 };
	constexpr word ENT{ 0x02cc }, PHASE{ 0x02e4 }, TIMER{ 0x02ec }, HITCNT{ 0x034c }, FACING{ 0x02dc };
	constexpr word DX_FULL{ 0x0375 }, DX_FRAC{ 0x0374 }, COUNTER{ 0x0383 }, FLAG_BASE{ 0x0101 };
	constexpr byte ZP_A4{ 0xa4 }, ZP_AD{ 0xad };
	constexpr word H_DIST{ 0x82f8 }, H_FACE{ 0x867b }, H_MOVE{ 0x8494 };
	constexpr word SITE_FORK{ 0x94b1 }, SITE_GATE{ 0x89ae }, V_AFTER_FORK{ 0x94b4 }, V_CONT{ 0x89b3 }, V_RUSH{ 0x94f3 };
	constexpr int MAX_FLAG{ 247 };
	constexpr byte ROW{ 14 };
	enum : byte { R_T0, R_T1, R_T2, R_T3, R_CE, R_BODY, R_REACH, R_APP, R_CHASE, R_BACK, R_LUNGE, R_SWORD, R_FIDX, R_MASK };

	constexpr std::array<byte, 3> FORK_ORIG{ 0x20, 0xf8, 0x82 };
	constexpr std::array<byte, 5> GATE_ORIG{ 0xa5, 0xad, 0xf0, 0x01, 0x60 };
	constexpr std::array<byte, 5> CONT_ORIG{ 0xad, 0x27, 0x04, 0x10, 0xf5 };
	// the reach compare, far path and in range branch a clear flag falls back to
	constexpr std::array<byte, 63> DEAD_ORIG{
		0xc9, 0x18, 0xf0, 0x1c, 0x90, 0x1b, 0x20, 0x7b, 0x86, 0xa9, 0x01, 0x8d, 0x75, 0x03, 0xa9, 0x00,
		0x8d, 0x74, 0x03, 0x20, 0x94, 0x84, 0xad, 0x83, 0x03, 0x29, 0x08, 0xf0, 0x03, 0xfe, 0xe4, 0x02,
		0x60, 0xa9, 0x01, 0x9d, 0xe4, 0x02, 0xa5, 0xa4, 0x29, 0x01, 0xd0, 0x13, 0x20, 0x7b, 0x86, 0xa0,
		0x00, 0xad, 0x83, 0x03, 0x29, 0x10, 0xf0, 0x02, 0xa0, 0x02, 0x98, 0x9d, 0xe4, 0x02, 0x60 };
	constexpr std::array<byte, 32> RUSH_ORIG{
		0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x20, 0x7b, 0x86, 0xa9, 0x02, 0x8d, 0x75, 0x03, 0xa9, 0x00, 0x8d,
		0x74, 0x03, 0x20, 0x94, 0x84, 0xad, 0x83, 0x03, 0x29, 0x04, 0xd0, 0x03, 0xfe, 0xe4, 0x02, 0x60 };

	struct Row {
		int windup{ 24 }, swing{ 12 }, recover{ 24 }, reach{ 24 }, approach{ 1 }, chase{ 2 }, bodyhurt{ 0 };
		int tell{ 12 }, back{ 1 }, lunge{ 2 }, sword{ 16 }, flag{ -1 };

		std::array<byte, ROW> bytes(void) const {
			const int t0{ windup - tell }, t1{ windup }, t2{ t1 + swing }, t3{ t2 + recover };
			const int ce{ (back != 0 || lunge != 0) ? t2 : t0 };   // a dwarf that never moves decides every frame
			auto b = [](int v) { return static_cast<byte>(v); };
			return { b(t0), b(t1), b(t2), b(t3), b(ce), b(bodyhurt), b(reach), b(approach), b(chase),
				b(back), b(lunge), b(sword), b(flag < 0 ? 0 : flag >> 3), b(flag < 0 ? 0 : 1 << (flag & 7)) };
		}
	};

	// the row of a monster no name lists: vanilla's timing and reach on the
	// per sprite clock, contact in every pose, vanilla's thrust box
	Row vanilla_row(void) {
		Row r;
		r.windup = 16; r.swing = 16; r.recover = 0; r.bodyhurt = 1;
		r.tell = 0; r.back = 0; r.lunge = 0; r.sword = 0;
		return r;
	}

	Row parse_row(const fh::GeneralHack& p_hack, const std::string& p_name) {
		auto get = [&](const char* p_id, int p_default, int p_min, int p_max) {
			if (!p_hack.has_param(p_id))
				return p_default;
			const int v{ static_cast<int>(p_hack.word_or(p_id, 0)) };
			if (v < p_min || v > p_max)
				throw std::runtime_error(std::format("{}: {} must be {} to {}", p_name, p_id, p_min, p_max));
			return v;
		};
		Row r;
		r.windup = get("windup", 24, 1, 255);
		r.swing = get("swing", 12, 1, 255);
		r.recover = get("recover", 24, 0, 255);
		r.reach = get("reach", 24, 1, 255);
		r.approach = get("approach", 1, 0, 3);
		r.chase = get("chase", 2, 0, 3);
		r.bodyhurt = get("bodyhurt", 0, 0, 1);
		r.tell = get("tell", std::min(12, r.windup), 0, 255);
		r.back = get("back", 1, 0, 3);
		r.lunge = get("lunge", 2, 0, 3);
		r.sword = get("sword", 16, 0, 255);
		r.flag = get("flag", -1, 0, MAX_FLAG);
		if (r.windup + r.swing + r.recover > 255)
			throw std::runtime_error(std::format("{}: windup + swing + recover must be 255 or less", p_name));
		if (r.tell > r.windup)
			throw std::runtime_error(std::format("{}: tell must not exceed windup", p_name));
		return r;
	}

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	// klib::Asm6502 has no inc_abs_x
	void inc_abs_x(klib::Asm6502& c, word p_addr) {   // INC $addr,X
		c.db(0xfe); c.db(static_cast<byte>(p_addr & 0xff)); c.db(static_cast<byte>(p_addr >> 8));
	}

	word at(word p_rows, byte p_field) { return static_cast<word>(p_rows + p_field); }

	// the runtime flag, read from the row: mask 0 is always on, else the byte
	// at $0101 + index is tested against the mask. branches to p_on when on and
	// falls through when the flag is clear; y is the row on entry and on exit
	void flag_test(klib::Asm6502& c, word p_rows, const std::string& p_on) {
		c.lda_abs_y(at(p_rows, R_MASK)); c.beq(p_on);
		c.lda_abs_y(at(p_rows, R_FIDX)); c.tay();
		c.lda_abs_y(FLAG_BASE); c.pha(); c.jsr("@sel"); c.pla();
		c.and_abs_y(at(p_rows, R_MASK)); c.bne(p_on);
	}

	// the body, then the gate, as one block of code
	void emit_code(klib::Asm6502& c, word p_rows) {
		// the body, entered in place of the distance call with x = the sprite
		c.jsr("@sel");
		flag_test(c, p_rows, "@on");
		c.jsr(H_DIST); c.jmp(V_AFTER_FORK);                       // flag clear: vanilla
		c.label("@on");
		// the distance call runs ldy $0304,x when the dwarf is left of the
		// hero, so the row is selected again after every call
		c.jsr(H_DIST); c.pha(); c.jsr("@sel");
		c.lda_abs_x(TIMER); c.beq("@free");                        // once the step back starts, the attack
		c.cmp_abs_y(at(p_rows, R_T0)); c.bcc("@free");             // plays out whatever the hero does
		c.cmp_abs_y(at(p_rows, R_CE)); c.bcs("@free");
		c.pla(); c.jmp("@hold");
		c.label("@free"); c.pla();
		c.cmp_abs_y(at(p_rows, R_REACH)); c.beq("@ret0"); c.bcc("@inrange");
		c.lda_imm(0); c.sta_abs_x(TIMER); c.jsr(H_FACE);          // far: reset, face, approach
		c.lda_abs_y(at(p_rows, R_APP)); c.jsr("@move");
		c.lda_abs(COUNTER); c.and_imm(0x08); c.beq("@ret0"); inc_abs_x(c, PHASE);
		c.label("@ret0"); c.rts();
		c.label("@inrange");
		c.lda_zp(ZP_A4); c.and_imm(0x01); c.beq("@hold");
		c.lda_imm(0); c.sta_abs_x(PHASE); c.jsr(H_FACE);          // the hero swings: rush
		c.lda_abs_y(at(p_rows, R_CHASE)); c.jsr("@move");
		c.lda_abs(COUNTER); c.and_imm(0x04); c.bne("@ret0"); inc_abs_x(c, PHASE); c.rts();
		c.label("@hold");
		c.jsr(H_FACE);
		c.lda_abs_x(HITCNT); c.beq("@tick"); c.lda_imm(0); c.sta_abs_x(TIMER);   // hit: restart
		c.label("@tick");
		inc_abs_x(c, TIMER); c.lda_abs_x(TIMER); c.cmp_abs_y(at(p_rows, R_T3)); c.bcc("@pose");
		c.lda_imm(0); c.sta_abs_x(TIMER);
		c.label("@pose");
		c.lda_abs_x(TIMER);
		c.cmp_abs_y(at(p_rows, R_T0)); c.bcc("@guard");
		c.cmp_abs_y(at(p_rows, R_T1)); c.bcc("@tell");
		c.cmp_abs_y(at(p_rows, R_T2)); c.bcs("@guard");
		c.lda_imm(2); c.sta_abs_x(PHASE);                          // the lunge
		c.lda_abs_y(at(p_rows, R_LUNGE)); c.beq("@ret");
		c.jsr(H_DIST); c.beq("@ret"); c.jsr("@sel");               // touching: stay
		c.lda_abs_y(at(p_rows, R_LUNGE)); c.jmp("@move");
		c.label("@tell");                                           // guard, step back
		c.lda_imm(0); c.sta_abs_x(PHASE);
		c.lda_abs_y(at(p_rows, R_BACK)); c.beq("@ret");
		c.jsr("@flip"); c.lda_abs_y(at(p_rows, R_BACK)); c.jsr("@move"); c.jmp("@flip");
		c.label("@guard"); c.lda_imm(0); c.sta_abs_x(PHASE);
		c.label("@ret"); c.rts();
		c.label("@sel");                                            // y = 0 sir gawaine, 14 wolfman
		c.ldy_imm(0); c.lda_abs_x(ENT); c.cmp_imm(WOLFMAN); c.bne("@sel1"); c.ldy_imm(ROW);
		c.label("@sel1"); c.rts();
		c.label("@move");                                           // vanilla's mover: facing, walls, edges
		c.sta_abs(DX_FULL); c.lda_imm(0); c.sta_abs(DX_FRAC); c.jsr(H_MOVE); c.rts();
		c.label("@flip");
		c.lda_abs_x(FACING); c.eor_imm(0x01); c.sta_abs_x(FACING); c.rts();

		// the gate, entered in place of the touch handler's first five bytes
		c.label("@gate");
		c.lda_zp(ZP_AD); c.bne("@gret");                           // iframes: vanilla's refusal
		c.lda_abs_x(ENT); c.cmp_imm(GAWAINE); c.beq("@gd"); c.cmp_imm(WOLFMAN); c.bne("@cont");
		c.label("@gd"); c.jsr("@sel");
		flag_test(c, p_rows, "@gon"); c.jmp(V_CONT);               // flag clear: vanilla
		c.label("@gon");
		c.lda_abs_y(at(p_rows, R_BODY)); c.bne("@cont");           // bodyhurt: vanilla
		c.lda_abs_x(PHASE); c.cmp_imm(2); c.bne("@gret");          // sword down: no damage
		c.lda_abs_y(at(p_rows, R_SWORD)); c.beq("@cont");          // sword 0: vanilla's box
		c.jsr(H_DIST); c.pha(); c.jsr("@sel"); c.pla();
		c.cmp_abs_y(at(p_rows, R_SWORD)); c.bcc("@cont"); c.beq("@cont");   // within the sword
		c.label("@gret"); c.rts();
		c.label("@cont"); c.jmp(V_CONT);
	}

	std::size_t code_size(void) {
		klib::Asm6502 probe;
		emit_code(probe, 0);
		return probe.size();
	}

	void write_row(std::vector<byte>& p_rom, word p_addr, const Row& p_row) {
		const auto row{ p_row.bytes() };
		for (std::size_t i{ 0 }; i < ROW; ++i)
			p_rom.at(klib::Asm6502::get_file_offset(BANK, static_cast<word>(p_addr + i))) = row[i];
	}
}

word fh::HackManager::install_AtlasDevDwarfControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack, bool p_wolfman) const {
	const std::string name{ p_wolfman ? "AtlasDevWolfmanControl" : "AtlasDevSirGawaineControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	const Row row{ parse_row(p_hack, name) };
	if (mode == "vanilla")
		return cpu_addr;

	const std::size_t size{ code_size() };
	const word own_row{ static_cast<word>(p_wolfman ? ROW : 0) };
	const auto fork_off{ klib::Asm6502::get_file_offset(BANK, SITE_FORK) };
	if (fork_off + 2 < p_rom.size() && p_rom[fork_off] == 0x4c) {
		// the other name already installed the code: verify it and write this row
		const word org{ static_cast<word>(p_rom[fork_off + 1] | (p_rom[fork_off + 2] << 8)) };
		const word rows{ static_cast<word>(org + size) };
		klib::Asm6502 expect;
		emit_code(expect, rows);
		const word gate{ static_cast<word>(org + expect.label_position("@gate")) };
		std::vector<byte> scratch(p_rom);
		expect.apply_hack_and_clear(scratch, BANK, org);
		const auto off{ klib::Asm6502::get_file_offset(BANK, org) };
		const auto gate_off{ klib::Asm6502::get_file_offset(BANK, SITE_GATE) };
		const bool ours{ off + size <= p_rom.size()
			&& std::equal(scratch.begin() + off, scratch.begin() + off + size, p_rom.begin() + off)
			&& p_rom[gate_off] == 0x4c && p_rom[gate_off + 1] == (gate & 0xff) && p_rom[gate_off + 2] == (gate >> 8) };
		if (!ours)
			throw std::runtime_error(std::format("{}: the site at ${:04x} is patched by something else", name, SITE_FORK));
		write_row(p_rom, static_cast<word>(rows + own_row), row);
		return cpu_addr;
	}

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_FORK, FORK_ORIG, name);
	require_site(p_rom, SITE_GATE, GATE_ORIG, name);
	require_site(p_rom, V_AFTER_FORK, DEAD_ORIG, name);
	require_site(p_rom, V_RUSH, RUSH_ORIG, name);
	require_site(p_rom, V_CONT, CONT_ORIG, name);
	const std::size_t total{ size + 2 * ROW };
	const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
	for (std::size_t i{ 0 }; i < total; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));

	const word rows{ static_cast<word>(cpu_addr + size) };
	klib::Asm6502 code;
	emit_code(code, rows);
	const word gate{ static_cast<word>(cpu_addr + code.label_position("@gate")) };
	code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
	write_row(p_rom, rows, p_wolfman ? vanilla_row() : row);
	write_row(p_rom, static_cast<word>(rows + ROW), p_wolfman ? row : vanilla_row());
	code.jmp(cpu_addr); code.apply_hack_and_clear(p_rom, BANK, SITE_FORK);
	code.jmp(gate); code.nop(2); code.apply_hack_and_clear(p_rom, BANK, SITE_GATE);
	return static_cast<word>(cpu_addr + total);
}
