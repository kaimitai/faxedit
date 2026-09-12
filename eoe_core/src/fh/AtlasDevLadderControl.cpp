#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <array>
#include <format>
#include <stdexcept>

// ladder control: how fast the hero climbs, and whether he may attack while
// on a ladder. the climb speeds are constants the vanilla rom already holds,
// so with default parameters this hack writes nothing at all and the patched
// rom is byte identical to the source.
//
// climb speed is four independent constants, one pair per direction and a
// second pair for wing boots flight; stock climbs up at $00a0 and down at
// $00c0 subpixels per frame. seven operand bytes carry them, the immediates
// of the add and subtract pairs at $e31e and $e325 for up, $e36c and $e373
// for down, $e314 for wing boots up and $e35c and $e363 for wing boots down.
// each operand is pinned below, and so is the instruction around it. wing
// boots ascent is whole pixels rather than subpixels because vanilla
// subtracts from the whole pixel byte only, and widening it needs 14 bytes
// where 7 are available.
//
// attacking while climbing is refused by one branch, the bcs at $e10a, whose
// target clears the attack request; attack=1 turns it into a two byte no op.
// the refusal outlasts the input, because $e2e5 is the only place that sets
// the climbing bit and it has no clear on release.
//
// two frame selectors choose the hero's appearance and must move together:
// $b927 in bank 14 for the weapon and shield overlays, $ecac in bank 15 for
// the body. each asks whether he is climbing before it asks whether he is
// attacking, so reordering only one draws him with three arms, the overlays
// swinging while the body still holds the rungs. attackpose=1 reorders both
// to test the attack bit first, in place, padding the spare bytes so every
// following address stays put.
//
// attackflag=n gates the attack on extended flag n at runtime. only the two
// operand bytes of the call at $e107 are retargeted, so the other callers of
// the climbing predicate keep vanilla behavior: jumping off a ladder,
// casting magic and the body's own climb pose are left alone. a clear flag
// is indistinguishable from stock and the flag page is cleared at reset.
//
// every site is verified against its exact vanilla bytes before anything is
// written, and all of them are byte identical in the us, us rev a, eu and jp
// roms.

namespace {
	// extended flags: flag n lives at $0101 + (n >> 3), mask 1 << (n & 7)
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr byte MAX_FLAG{ 247 };
	constexpr byte NO_FLAG{ 0xff };

	constexpr word LADDER{ 0xecf6 };        // carry set while actively climbing
	constexpr word ATTACK_CALL{ 0xe107 };   // jsr $ecf6 inside the attack handler
	constexpr byte ATTACK_CALL_ORIG[3]{ 0x20, 0xf6, 0xec };

	constexpr word ATTACK_BRANCH{ 0xe10b }; // the bcs operand that refuses it
	constexpr byte ATTACK_BRANCH_ORIG{ 0x0d };

	// a climb step must stay inside one eight pixel block per frame. the
	// upward screen crossing is found by the borrow out of the subtract and
	// the downward one by a compare, and both survive an overshoot, but a
	// step that clears a whole block in one frame can pass the end of a
	// ladder without the handler seeing it.
	constexpr word MAX_SUBPIXEL{ 0x0800 };
	constexpr byte MAX_WING_UP{ 8 };

	constexpr word VANILLA_UP{ 0x00a0 }, VANILLA_DOWN{ 0x00c0 };
	constexpr byte VANILLA_WING_UP{ 1 };
	constexpr word VANILLA_WING_DOWN{ 0x0180 };

	struct Site {
		word addr;
		byte orig;
	};

	// the operand bytes, with the value each must hold in a stock rom
	constexpr Site UP_LO{ 0xe322, 0xa0 }, UP_HI{ 0xe328, 0x00 };
	constexpr Site DOWN_LO{ 0xe370, 0xc0 }, DOWN_HI{ 0xe376, 0x00 };
	constexpr Site WING_UP{ 0xe318, 0x01 };
	constexpr Site WING_DOWN_LO{ 0xe360, 0x80 }, WING_DOWN_HI{ 0xe366, 0x01 };

	// the whole instruction around each speed operand, so a rom whose
	// surrounding code differs is refused rather than quietly altered
	constexpr word SPEED_SITE[7]{ 0xe31e, 0xe325, 0xe36c, 0xe373, 0xe314, 0xe35c, 0xe363 };
	constexpr byte SPEED_ORIG[7][7]{
		{ 0xa5, 0xa0, 0x38, 0xe9, 0xa0, 0x85, 0xa0 },
		{ 0xa5, 0xa1, 0xe9, 0x00, 0x85, 0xa1, 0x00 },
		{ 0xa5, 0xa0, 0x18, 0x69, 0xc0, 0x85, 0xa0 },
		{ 0xa5, 0xa1, 0x69, 0x00, 0x85, 0xa1, 0x00 },
		{ 0xa5, 0xa1, 0x38, 0xe9, 0x01, 0x85, 0xa1 },
		{ 0xa5, 0xa0, 0x18, 0x69, 0x80, 0x85, 0xa0 },
		{ 0xa5, 0xa1, 0x69, 0x01, 0x85, 0xa1, 0x00 },
	};
	constexpr std::size_t SPEED_LEN[7]{ 7, 6, 7, 6, 7, 7, 6 };

	// the two frame selectors, near clones differing only in a branch target
	struct Pose {
		byte bank;
		word org;
		std::size_t room;
		word attack_frames;
		word climb_path;
		byte orig[12];
	};
	constexpr Pose POSE[2]{
		{ 14, 0xb927, 12, 0xb958, 0xb933,
			{ 0xa5, 0xa4, 0x4a, 0x90, 0x07, 0xa5, 0xa4, 0x30, 0x28, 0xa9, 0x03, 0x60 } },
		{ 15, 0xecac, 12, 0xeccc, 0xecb8,
			{ 0xa5, 0xa4, 0x4a, 0x90, 0x07, 0xa5, 0xa4, 0x30, 0x17, 0xa9, 0x03, 0x60 } },
	};

	// profile=name sets up, down, attack and attackpose at once, in the spirit
	// of the game named; wing boots are left as given. metroid and contra
	// have no ladders and are refused rather than invented. an explicit knob
	// overrides the profile.
	struct LadderProfile { const char* name; word up, down; byte attack, attack_pose; };
	constexpr std::array<LadderProfile, 8> LADDER_PROFILES{ {
		{ "vanilla",        0x00a0, 0x00c0, 0, 0 },
		{ "zelda2",         224, 224, 1, 1 },
		{ "megaman",        255, 255, 0, 0 },
		{ "castlevania",    160, 160, 0, 0 },
		{ "ninjagaiden",    255, 255, 1, 1 },
		{ "ghostsngoblins", 160, 192, 0, 0 },
		{ "kidicarus",      224, 224, 1, 1 },
		{ "arcade",         255, 255, 1, 1 },
	} };

	const LadderProfile& ladder_profile(const fh::GeneralHack& p_hack) {
		const std::string name{ p_hack.string_or("profile", "vanilla") };
		for (const LadderProfile& p : LADDER_PROFILES) if (name == p.name) return p;
		if (name == "metroid" || name == "contra")
			throw std::runtime_error(std::format(
				"AtlasDevLadderControl: {} has no ladders, so there is no {} profile here; leave the hack out or set the knobs", name, name));
		throw std::runtime_error(std::format("AtlasDevLadderControl: unknown profile '{}'", name));
	}

	struct Settings {
		word up, down, wing_down;
		byte wing_up, attack, attack_pose, attack_flag;
		bool want_runtime(void) const { return attack_flag != NO_FLAG; }
	};

	void require_site(const std::vector<byte>& p_rom, byte p_bank, word p_addr,
		const byte* p_orig, std::size_t p_size) {
		const auto off{ klib::Asm6502::get_file_offset(p_bank, p_addr) };
		for (std::size_t i{ 0 }; i < p_size; ++i)
			if (p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format(
					"AtlasDevLadderControl: the vanilla site at ${:04x} is not intact", p_addr));
	}

	void require_operand(const std::vector<byte>& p_rom, const Site& p_site) {
		const auto off{ klib::Asm6502::get_file_offset(15, p_site.addr) };
		if (p_rom[off] != p_site.orig)
			throw std::runtime_error(std::format(
				"AtlasDevLadderControl: the operand at ${:04x} is not vanilla", p_site.addr));
	}

	void put(std::vector<byte>& p_rom, word p_addr, byte p_value) {
		p_rom[klib::Asm6502::get_file_offset(15, p_addr)] = p_value;
	}

	// both branch targets are outside the block, so the distances are
	// measured from the emitted layout rather than written down: the bmi ends
	// four bytes into the block and the bcc seven
	sbyte branch_to(word p_from_next, word p_target, const char* p_what) {
		const int delta{ static_cast<int>(p_target) - static_cast<int>(p_from_next) };
		if (delta < -128 || delta > 127)
			throw std::runtime_error(std::format(
				"AtlasDevLadderControl: the {} branch to ${:04x} is out of range", p_what, p_target));
		return static_cast<sbyte>(delta);
	}

	// test the attack bit once, before the jump and climb paths split, so an
	// attack always picks the attack frames
	void emit_pose(klib::Asm6502& p_code, const Pose& p_pose) {
		p_code.lda_zp(0xa4);
		p_code.bmi(branch_to(static_cast<word>(p_pose.org + 4), p_pose.attack_frames, "attack"));
		p_code.lsr_a();
		p_code.bcc(branch_to(static_cast<word>(p_pose.org + 7), p_pose.climb_path, "climb"));
		p_code.lda_imm(0x03);
		p_code.rts();
	}

	// carry in from the vanilla predicate, carry out to the vanilla branch:
	// not climbing leaves it clear, a clear flag leaves it set and the attack
	// is refused exactly as stock refuses it, a set flag clears it
	void emit_runtime(klib::Asm6502& p_code, byte p_flag) {
		p_code.jsr(LADDER);
		p_code.bcc("@out");
		p_code.lda_abs(static_cast<word>(FLAG_BASE + (p_flag >> 3)));
		p_code.and_imm(static_cast<byte>(1 << (p_flag & 7)));
		p_code.beq("@out");
		p_code.clc();
		p_code.label("@out");
		p_code.rts();
	}
}

word fh::HackManager::install_AtlasDevLadderControl(const fe::Config&, std::vector<byte>& p_rom,
	word cpu_addr, const fh::GeneralHack& p_hack) const {
	const LadderProfile& base{ ladder_profile(p_hack) };
	const Settings s{
		p_hack.word_or("up", base.up),
		p_hack.word_or("down", base.down),
		p_hack.word_or("wingdown", VANILLA_WING_DOWN),
		p_hack.byte_or("wingup", VANILLA_WING_UP),
		p_hack.byte_or("attack", base.attack),
		p_hack.byte_or("attackpose", base.attack_pose),
		p_hack.has_param("attackflag") ? p_hack.byte_or("attackflag", 0) : NO_FLAG,
	};

	if (s.up < 1 || s.up > MAX_SUBPIXEL)
		throw std::runtime_error(std::format(
			"AtlasDevLadderControl: up must be 1 to {} subpixels per frame", MAX_SUBPIXEL));
	if (s.down < 1 || s.down > MAX_SUBPIXEL)
		throw std::runtime_error(std::format(
			"AtlasDevLadderControl: down must be 1 to {} subpixels per frame", MAX_SUBPIXEL));
	if (s.wing_down < 1 || s.wing_down > MAX_SUBPIXEL)
		throw std::runtime_error(std::format(
			"AtlasDevLadderControl: wingdown must be 1 to {} subpixels per frame", MAX_SUBPIXEL));
	if (s.wing_up < 1 || s.wing_up > MAX_WING_UP)
		throw std::runtime_error(std::format(
			"AtlasDevLadderControl: wingup is whole pixels per frame, 1 to {}", MAX_WING_UP));
	if (s.attack > 1)
		throw std::runtime_error("AtlasDevLadderControl: attack must be 0 or 1");
	if (s.attack_pose > 1)
		throw std::runtime_error("AtlasDevLadderControl: attackpose must be 0 or 1");
	if (s.want_runtime() && s.attack_flag > MAX_FLAG)
		throw std::runtime_error(std::format(
			"AtlasDevLadderControl: attackflag must be 0 to {}", MAX_FLAG));
	if (s.want_runtime() && s.attack)
		throw std::runtime_error(
			"AtlasDevLadderControl: attack=1 is the always on form; use attackflag alone "
			"for the runtime gate");

	klib::Asm6502 runtime;
	if (s.want_runtime())
		emit_runtime(runtime, s.attack_flag);
	const auto size{ s.want_runtime() ? runtime.size() : 0 };

	// every ownership and capacity check is complete before any mutation
	for (std::size_t i{ 0 }; i < 7; ++i)
		require_site(p_rom, 15, SPEED_SITE[i], SPEED_ORIG[i], SPEED_LEN[i]);
	for (const Site& site : { UP_LO, UP_HI, DOWN_LO, DOWN_HI, WING_UP, WING_DOWN_LO, WING_DOWN_HI })
		require_operand(p_rom, site);
	if (s.attack || s.want_runtime())
		require_site(p_rom, 15, ATTACK_CALL, ATTACK_CALL_ORIG, sizeof(ATTACK_CALL_ORIG));
	if (s.attack_pose)
		for (const Pose& pose : POSE)
			require_site(p_rom, pose.bank, pose.org, pose.orig, sizeof(pose.orig));
	if (s.want_runtime()) {
		const auto off{ klib::Asm6502::get_file_offset(15, cpu_addr) };
		for (std::size_t i{ 0 }; i < size; ++i)
			if (p_rom[off + i] != 0xff)
				throw std::runtime_error(std::format(
					"AtlasDevLadderControl: no free space for {} bytes at ${:04x}", size, cpu_addr));
	}

	put(p_rom, UP_LO.addr, static_cast<byte>(s.up & 0xff));
	put(p_rom, UP_HI.addr, static_cast<byte>(s.up >> 8));
	put(p_rom, DOWN_LO.addr, static_cast<byte>(s.down & 0xff));
	put(p_rom, DOWN_HI.addr, static_cast<byte>(s.down >> 8));
	put(p_rom, WING_UP.addr, s.wing_up);
	put(p_rom, WING_DOWN_LO.addr, static_cast<byte>(s.wing_down & 0xff));
	put(p_rom, WING_DOWN_HI.addr, static_cast<byte>(s.wing_down >> 8));
	if (s.attack)
		put(p_rom, ATTACK_BRANCH, 0x00);

	if (s.attack_pose)
		for (const Pose& pose : POSE) {
			klib::Asm6502 code;
			emit_pose(code, pose);
			// pad rather than shorten, so every following address stays put
			code.nop(pose.room - code.size());
			code.apply_hack_and_clear(p_rom, pose.bank, pose.org);
		}

	if (s.want_runtime()) {
		runtime.apply_hack_and_clear(p_rom, 15, cpu_addr);
		// retarget only the operand of the existing call
		put(p_rom, static_cast<word>(ATTACK_CALL + 1), static_cast<byte>(cpu_addr & 0xff));
		put(p_rom, static_cast<word>(ATTACK_CALL + 2), static_cast<byte>(cpu_addr >> 8));
	}

	return static_cast<word>(cpu_addr + size);
}
