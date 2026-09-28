#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <cmath>
#include <optional>
#include <stdexcept>

// status ward: while a timed status effect is running, monsters near
// the player are pushed away, so the effect projects a small forcefield
// on top of what it normally does. the game keeps its four timed
// effects in one counter block with one convention (negative inactive,
// positive active, one tick per 64 frames), so trigger= picks which of
// them carries the field: ointment (the classic invincibility ward,
// the default), glove (a battle aura), wingboots (a slipstream while
// flying) or hourglass (sweep the frozen enemies away). trigger=any
// raises the field for whichever of the four is running, and
// trigger=always keeps it up permanently with no item at all. runs as
// a POST role on the AtlasDevFrameScheduler and uses no ram of its
// own; the trigger is the effect's own counter, so anything that
// grants the item grants the ward. pull=true inverts the field into a
// magnet that drags enemies toward the player (an 8 pixel deadzone
// stops the jitter at the center), and arc=front or arc=back guards
// only one side, read from the facing bit every frame, so turning
// around exposes you. the orb graphic takes fieldspeed=<0 to 5, 0 is
// static and 5 is frantic>, fieldcount=<1, 2 or 4> and
// fielddir=<cw or ccw> on top of fieldtile and fieldattr.
// takes radius=<pixels, default 40>, push=<pixels per frame, 1 to 4,
// default 2> and exempt=<one entity id that stands firm, default $33>.
// empty slots and entities with zero hit points are never pushed, and
// the push clamps near the screen edges so a monster can be pinned
// against the wall but never shoved out of the playfield.
// AtlasDevArmRole 5, state switches the ward from scripts; switching
// off simply stops the pushes, nothing needs restoring. two extras:
// fieldpulse=<mask> lets the field rest on frames where the scheduler
// counter masked is zero, so enemies get pushed back in visible waves
// ($18 rests 8 of every 32 frames; 0 = steady). aura=1 holds the tint
// kind in slot 2 while the timer runs, so an installed
// AtlasDevInfectedTint glows the hero exactly while the field is up.
// fieldfx=true draws the field itself: four ring sprites orbiting the
// hero at the ward radius, riding the scheduler's PRE lane into OAM
// entries 60 to 63 right before the displaced OAM DMA. they orbit once
// every 64 frames, blink out with the fieldpulse rest window, and the
// role body parks them the moment the timer lapses or the role is
// disarmed, so nothing goes stale on screen. fieldtile=<tile, default
// $40> and fieldattr=<attribute, default 0: sprite palette 0, which
// also makes the orbs follow the aura tint> pick the look. near the
// horizontal screen edges an orb wraps to the far side for a moment;
// edgepark=true parks orbs outside the screen on either axis instead.

namespace {
	constexpr word OINTMENT{ 0x0427 };
	constexpr word GLOVE{ 0x0428 };
	constexpr word WINGBOOTS{ 0x0429 };
	constexpr word HOURGLASS{ 0x042a };
	constexpr word ENTITY_ID{ 0x02cc }, ENTITY_HP{ 0x0344 };
	// true per slot entity X lives in zero page at $ba plus the slot; the
	// on screen value is derived from it every frame, so pushing the
	// derived copy would do nothing
	constexpr byte ZP_ENTITY_X{ 0xba };
	constexpr byte ZP_PLAYER_X{ 0x9e };
	// the prior-art PlayerFlags byte: bit 6 set = facing right
	constexpr byte ZP_PLAYER_FLAGS{ 0xa4 };
	// hero screen Y; the drawn box top is this + 32 (the HUD band)
	constexpr byte ZP_PLAYER_Y{ 0xa1 };
	constexpr word OAM_FX{ 0x07f0 };      // OAM shadow entries 60 to 63
	constexpr byte KIND_WARD{ 0x05 };
	constexpr byte KIND_TINT{ 0x03 };

	// the scheduler abi once exposed these three helpers for role
	// installers; current upstream keeps the abi slim and every role does
	// the same checks locally, so this port carries its own copies
	struct PostChain {
		bool chained;
		word target;
	};

	PostChain validate_post_chain(const std::vector<byte>& p_rom,
		std::size_t p_scheduler, word p_base) {
		using namespace fh::afs;
		const word target{ static_cast<word>(p_rom[p_scheduler + OFF_POST]
			| (p_rom[p_scheduler + OFF_POST + 1] << 8)) };
		const byte marker{ p_rom[p_scheduler + OFF_POSTARMED] };
		const word stub{ static_cast<word>(p_base + OFF_STUB) };
		if (marker == 0) {
			if (target != stub)
				throw std::runtime_error(
					"AtlasDevStatusWard: scheduler POST lane has inconsistent unclaimed state");
			return { false, stub };
		}
		if (marker != 1)
			throw std::runtime_error(
				"AtlasDevStatusWard: scheduler POST marker must be 0 or 1");
		if (target < 0x8000 || target >= 0xc000)
			throw std::runtime_error(
				"AtlasDevStatusWard: scheduler POST chain target is outside bank 9");
		const auto target_off{ klib::Asm6502::get_file_offset(9, target) };
		if (target_off >= p_rom.size() || p_rom[target_off] == 0xff)
			throw std::runtime_error(
				"AtlasDevStatusWard: scheduler POST chain target is pristine or unavailable");
		return { true, target };
	}

	std::optional<std::size_t> select_post_role_arm_site(
		const std::vector<byte>& p_rom, std::size_t p_scheduler, word p_base,
		byte p_kind) {
		using namespace fh::afs;
		constexpr std::size_t pre_sites[3]{ OFF_PRE0, OFF_PRE1, OFF_PRE2 };
		const word stub{ static_cast<word>(p_base + OFF_STUB) };
		const auto read_operand{ [&p_rom, p_scheduler](std::size_t site) {
			return static_cast<word>(p_rom[p_scheduler + site]
				| (p_rom[p_scheduler + site + 1] << 8));
		} };
		for (std::size_t i{ 0 }; i < 3; ++i) {
			if (p_rom[p_scheduler + OFF_ARM0 + i] != p_kind)
				continue;
			if (read_operand(pre_sites[i]) != stub)
				throw std::runtime_error(
					"AtlasDevStatusWard: scheduler kind 5 slot has a PRE claimant");
			return OFF_ARM0 + i;
		}
		for (std::size_t i{ 0 }; i < 3; ++i)
			if (p_rom[p_scheduler + OFF_ARM0 + i] == 0
				&& read_operand(pre_sites[i]) == stub)
				return OFF_ARM0 + i;
		throw std::runtime_error(
			"AtlasDevStatusWard: scheduler arm table has no unclaimed slot");
	}

	word find_pristine_bank9_org(const std::vector<byte>& p_rom,
		std::size_t p_size) {
		constexpr std::size_t bank_size{ 0x4000 };
		if (p_size == 0 || p_size > bank_size)
			return 0;
		const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
		if (p_rom.size() < bank9 + bank_size)
			return 0;
		for (std::size_t off{ 0 }; off <= bank_size - p_size; ++off) {
			bool pristine{ true };
			for (std::size_t i{ 0 }; i < p_size; ++i)
				if (p_rom[bank9 + off + i] != 0xff) {
					pristine = false;
					break;
				}
			if (pristine)
				return static_cast<word>(0x8000 + off);
		}
		return 0;
	}
}

word fh::HackManager::install_AtlasDevStatusWard(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	using namespace fh::afs;
	const word base{ find_base(p_rom) };
	if (base == 0)
		throw std::runtime_error("AtlasDevStatusWard requires the AtlasDevFrameScheduler hack installed first");
	const byte radius{ p_hack.byte_or("radius", 40) };
	if (radius < 4 || radius > 120)
		throw std::runtime_error("AtlasDevStatusWard: radius must be 4 to 120 pixels");
	const byte push{ p_hack.byte_or("push", 2) };
	if (push < 1 || push > 4)
		throw std::runtime_error("AtlasDevStatusWard: push must be 1 to 4 pixels");
	const byte exempt{ p_hack.byte_or("exempt", 0x33) };
	const byte fieldpulse{ p_hack.byte_or("fieldpulse", 0) };
	const bool aura{ p_hack.bool_or("aura", false) };
	const bool fieldfx{ p_hack.bool_or("fieldfx", false) };
	const bool edgepark{ p_hack.bool_or("edgepark", false) };
	const byte fieldtile{ p_hack.byte_or("fieldtile", 0x40) };
	const byte fieldattr{ p_hack.byte_or("fieldattr", 0x00) };
	const std::string trig_name{ p_hack.string_or("trigger", "ointment") };
	word trigger{ 0 };
	enum class TrigMode { plain, any, always };
	TrigMode tmode{ TrigMode::plain };
	if (trig_name == "ointment") trigger = OINTMENT;
	else if (trig_name == "glove") trigger = GLOVE;
	else if (trig_name == "wingboots") trigger = WINGBOOTS;
	else if (trig_name == "hourglass") trigger = HOURGLASS;
	else if (trig_name == "any") tmode = TrigMode::any;
	else if (trig_name == "always") tmode = TrigMode::always;
	else
		throw std::runtime_error("AtlasDevStatusWard: trigger must be ointment, glove, wingboots, hourglass, any or always");
	const bool pull{ p_hack.bool_or("pull", false) };
	const std::string arc{ p_hack.string_or("arc", "both") };
	if (arc != "both" && arc != "front" && arc != "back")
		throw std::runtime_error("AtlasDevStatusWard: arc must be both, front or back");
	if (pull && radius <= 12)
		throw std::runtime_error("AtlasDevStatusWard: pull needs radius above 12, the 8 pixel deadzone plus the push step would leave no band where the drag can ever run");
	if (aura && tmode == TrigMode::always)
		throw std::runtime_error("AtlasDevStatusWard: aura with trigger=always is a permanently armed tint; install the tint armed instead");
	const byte fieldspeed{ p_hack.byte_or("fieldspeed", 3) };
	if (fieldspeed > 5)
		throw std::runtime_error("AtlasDevStatusWard: fieldspeed must be 0 to 5");
	const byte fieldcount{ p_hack.byte_or("fieldcount", 4) };
	if (fieldcount != 1 && fieldcount != 2 && fieldcount != 4)
		throw std::runtime_error("AtlasDevStatusWard: fieldcount must be 1, 2 or 4");
	const std::string fielddir{ p_hack.string_or("fielddir", "cw") };
	if (fielddir != "cw" && fielddir != "ccw")
		throw std::runtime_error("AtlasDevStatusWard: fielddir must be cw or ccw");
	const std::string fieldpattern{ p_hack.string_or("fieldpattern", "orbit") };
	if (fieldpattern != "orbit" && fieldpattern != "expand" && fieldpattern != "contract")
		throw std::runtime_error("AtlasDevStatusWard: fieldpattern must be orbit, expand or contract");
	const bool radial{ fieldpattern != "orbit" };
	const byte fieldblink{ p_hack.byte_or("fieldblink", 0) };
	const byte fieldframes{ p_hack.byte_or("fieldframes", 1) };
	if (fieldframes != 1 && fieldframes != 2 && fieldframes != 4)
		throw std::runtime_error("AtlasDevStatusWard: fieldframes must be 1, 2 or 4");
	if (static_cast<unsigned>(fieldtile) + fieldframes > 256)
		throw std::runtime_error("AtlasDevStatusWard: animated field tiles exceed tile $FF");
	const byte fieldanimspeed{ p_hack.byte_or("fieldanimspeed", 3) };
	if (fieldanimspeed > 5)
		throw std::runtime_error("AtlasDevStatusWard: fieldanimspeed must be 0 to 5");
	// Preserve the legacy single-tile comparison exactly. Animated ownership
	// is the non-wrapping interval [fieldtile, fieldtile + fieldframes).
	const auto emit_tile_guard = [&](klib::Asm6502& c, const std::string& skip) {
		c.cmp_imm(fieldtile);
		if (fieldframes == 1)
			c.bne(skip);
		else {
			c.bcc(skip);
			if (static_cast<unsigned>(fieldtile) + fieldframes < 256) {
				c.cmp_imm(static_cast<byte>(fieldtile + fieldframes));
				c.bcs(skip);
			}
		}
	};
	// the status gate, shared by the role body and the orb graphic: on a
	// live status control falls through; otherwise it branches to p_off.
	// zero counts as inactive (power-on ram clear; the final 64-frame tick)
	const auto emit_gate = [&](klib::Asm6502& c, const std::string& p_active,
		const std::string& p_off) {
		if (tmode == TrigMode::always)
			return;
		if (tmode == TrigMode::any) {
			int n{ 0 };
			for (word ad : { OINTMENT, GLOVE, WINGBOOTS, HOURGLASS }) {
				const std::string skip{ "@gate" + std::to_string(n++) };
				c.lda_abs(ad);
				c.bmi(skip);
				c.bne(p_active);
				c.label(skip);
			}
			c.jmp(p_off);
			c.label(p_active);
			return;
		}
		c.lda_abs(trigger);
		c.bmi(p_off);
		c.beq(p_off);
	};
	const char* off_lbl{ fieldfx ? "@park" : "@tail" };

	const auto scheduler{ klib::Asm6502::get_file_offset(15, base) };
	const auto chain{ validate_post_chain(p_rom, scheduler, base) };

	// StatusWard is useful as the fourth installed role even though only
	// three kinds can be armed at once: when the table is full it installs
	// dormant and AtlasDevArmRole swaps it in later. Reuse the local strict
	// selector whenever kind 5 already exists or a structurally free slot is
	// available; only the genuinely full/reserved case takes the documented
	// dormant fallback.
	constexpr std::size_t pre_sites[3]{ OFF_PRE0, OFF_PRE1, OFF_PRE2 };
	const word stub{ static_cast<word>(base + OFF_STUB) };
	const auto read_pre{ [&p_rom, scheduler](std::size_t site) {
		return static_cast<word>(p_rom[scheduler + site]
			| (p_rom[scheduler + site + 1] << 8));
	} };
	std::optional<std::size_t> existing_kind;
	bool has_unclaimed_slot{ false };
	for (std::size_t i{ 0 }; i < 3; ++i) {
		if (p_rom[scheduler + OFF_ARM0 + i] == KIND_WARD) {
			if (existing_kind.has_value())
				throw std::runtime_error(
					"AtlasDevStatusWard: scheduler kind 5 appears in multiple slots");
			existing_kind = i;
		}
		has_unclaimed_slot = has_unclaimed_slot
			|| (p_rom[scheduler + OFF_ARM0 + i] == 0
				&& read_pre(pre_sites[i]) == stub);
	}
	std::optional<std::size_t> arm_site;
	if (existing_kind.has_value()
		&& read_pre(pre_sites[*existing_kind]) != stub) {
		// Several StatusWard bodies may share kind 5 (one per trigger), but
		// only one may own the visible field. Recognize that existing owner
		// narrowly: every PRE vector must share one fixed-bank entry whose
		// code begins with this role's CMP #5 / BNE gate.
		const word field_entry{ read_pre(pre_sites[*existing_kind]) };
		bool compatible{ field_entry >= 0xc000 };
		for (const auto pre : pre_sites)
			compatible = compatible && read_pre(pre) == field_entry;
		if (compatible) {
			const auto entry_off{ klib::Asm6502::get_file_offset(15, field_entry) };
			compatible = entry_off + 3 <= p_rom.size()
				&& p_rom[entry_off] == 0xc9 && p_rom[entry_off + 1] == KIND_WARD
				&& p_rom[entry_off + 2] == 0xd0;
		}
		if (!compatible)
			throw std::runtime_error(
				"AtlasDevStatusWard: scheduler kind 5 has an incompatible PRE claimant");
		arm_site = OFF_ARM0 + *existing_kind;
	}
	else if (existing_kind.has_value() || has_unclaimed_slot) {
		arm_site = select_post_role_arm_site(p_rom, scheduler, base, KIND_WARD);
	}

	klib::Asm6502 code;
	for (word slot : { RAM_SLOT0, RAM_SLOT1, RAM_SLOT2 }) {
		code.lda_abs(slot);
		code.cmp_imm(KIND_WARD);
		code.beq("@enabled");
	}
	// disarmed: with aura, fall through the cleanup so a disarm mid-timer
	// never strands the tint kind in slot 2. the jmp doubles as a
	// range-safe trampoline for far branches later in the body
	code.label("@viaoff");
	code.jmp(aura ? "@aura_off" : off_lbl);
	code.label("@enabled");
	if (aura) {
		emit_gate(code, "@aon", "@viaoff");
		code.lda_abs(RAM_SLOT2);              // never overwrite a ward armed
		code.cmp_imm(KIND_WARD);              // IN slot 2: the glow yields,
		code.beq("@field");                   // the ward survives
		code.lda_imm(KIND_TINT);
		code.sta_abs(RAM_SLOT2);              // glow while warded
		code.jmp("@field");
		code.label("@aura_off");
		code.lda_abs(RAM_SLOT2);
		code.cmp_imm(KIND_TINT);
		code.bne("@acdone");
		code.lda_imm(0x00);
		code.sta_abs(RAM_SLOT2);              // timer done: aura off
		code.label("@acdone");
		code.jmp(off_lbl);
		code.label("@field");
	}
	else
		emit_gate(code, "@wardon", "@viaoff");
	if (fieldpulse) {
		// the field rests while (counter and mask) is zero: waves
		// (long-form branch, @tail can sit past the 127 byte reach)
		code.lda_abs(RAM_CNT_LO);
		code.and_imm(fieldpulse);
		code.bne("@pcont");
		code.jmp("@tail");
		code.label("@pcont");
	}
	code.ldx_imm(0x07);
	code.label("@loop");
	code.lda_abs_x(ENTITY_ID);
	code.cmp_imm(0xff);
	code.beq("@next");                        // empty slot
	code.cmp_imm(exempt);
	code.beq("@next");                        // the exempt id stands firm
	code.lda_abs_x(ENTITY_HP);
	code.beq("@next");                        // zero hit points
	code.lda_zp_x(ZP_ENTITY_X);
	code.sec();
	code.sbc_zp(ZP_PLAYER_X);
	// classify by the borrow, not the sign bit: N aliases monsters more
	// than 128 pixels away onto the wrong arm and inverts the force
	code.bcc("@left");
	if (arc != "both") {
		// the facing bit picks the guarded side; A is clobbered, so the
		// diff is re-derived after the check
		code.lda_zp(ZP_PLAYER_FLAGS);
		code.and_imm(0x40);
		if (arc == "front") code.beq("@next"); else code.bne("@next");
		code.lda_zp_x(ZP_ENTITY_X);
		code.sec();
		code.sbc_zp(ZP_PLAYER_X);
	}
	code.cmp_imm(radius);
	code.bcs("@next");                        // too far right
	if (pull) {
		code.cmp_imm(0x08);
		code.bcc("@next");                    // deadzone: no jitter
		for (byte i{ 0 }; i < push; ++i)
			code.dec_zp_x(ZP_ENTITY_X);       // drag it in
	}
	else {
		code.lda_zp_x(ZP_ENTITY_X);           // edge clamp: never push past
		code.cmp_imm(static_cast<byte>(0xf0 - push));   // $f0, the byte would wrap
		code.bcs("@next");
		for (byte i{ 0 }; i < push; ++i)
			code.inc_zp_x(ZP_ENTITY_X);
	}
	code.jmp("@next");
	code.label("@left");
	if (arc != "both") {
		code.lda_zp(ZP_PLAYER_FLAGS);
		code.and_imm(0x40);
		if (arc == "front") code.bne("@next"); else code.beq("@next");
		code.lda_zp_x(ZP_ENTITY_X);
		code.sec();
		code.sbc_zp(ZP_PLAYER_X);
	}
	code.eor_imm(0xff);
	code.clc();
	code.adc_imm(0x01);                       // abs(diff)
	code.cmp_imm(radius);
	code.bcs("@next");                        // too far left
	if (pull) {
		code.cmp_imm(0x08);
		code.bcc("@next");                    // deadzone
		for (byte i{ 0 }; i < push; ++i)
			code.inc_zp_x(ZP_ENTITY_X);       // drag it in
	}
	else {
		code.lda_zp_x(ZP_ENTITY_X);           // edge clamp: never push below $10
		code.cmp_imm(static_cast<byte>(0x10 + push));
		code.bcc("@next");
		for (byte i{ 0 }; i < push; ++i)
			code.dec_zp_x(ZP_ENTITY_X);
	}
	code.label("@next");
	code.dex();
	code.bpl("@loop");
	if (fieldfx) {
		// inactive frames park the orb entries (the engine's own $f8),
		// so a disarm or an expired timer never leaves them on screen
		code.jmp("@tail");
		code.label("@park");
		// park-if-mine: only entries carrying the field tile are ours -
		// anything else in the shared OAM tail belongs to someone else
		for (word k{ 0 }; k < 4; ++k) {
			const std::string skip{ "@pk" + std::to_string(k) };
			code.lda_abs(static_cast<word>(OAM_FX + 4 * k + 1));
			emit_tile_guard(code, skip);
			code.lda_imm(0xf8);
			code.sta_abs(static_cast<word>(OAM_FX + 4 * k));
			code.label(skip);
		}
	}
	code.label("@tail");
	if (chain.chained)
		code.jmp(chain.target);
	else
		code.rts();

	const word org{ find_pristine_bank9_org(p_rom, code.size()) };
	if (org == 0)
		throw std::runtime_error("AtlasDevStatusWard: no free bank 9 window");

	code.apply_hack_and_clear(p_rom, 9, org);
	p_rom[scheduler + OFF_POST] = org & 0xff;
	p_rom[scheduler + OFF_POST + 1] = org >> 8;
	p_rom[scheduler + OFF_POSTARMED] = 0x01;
	// With no compatible free slot the optional is empty by design: the
	// body remains installed and scripts can swap kind 5 in later.
	if (arm_site.has_value())
		p_rom[scheduler + *arm_site] = KIND_WARD;
	if (!fieldfx)
		return cpu_addr;

	// Orbit keeps its legacy X8/Y8 tables. Radial patterns use X32/Y32:
	// four stages of eight compass angles, with a rounded radius per stage.
	// installed on all three PRE vectors so it follows the ward through
	// AtlasDevArmRole slot swaps (it acts only on the ward kind byte)
	klib::Asm6502 fx;
	const unsigned stages{ radial ? 4U : 1U };
	const word y_table{ static_cast<word>(cpu_addr + stages * 8) };
	for (const bool y_axis : { false, true })
		for (unsigned stage{ 0 }; stage < stages; ++stage) {
			const int stage_radius{ radial
				? static_cast<int>(std::lround(radius * (stage + 1) / 4.0)) : radius };
			for (int k{ 0 }; k < 8; ++k) {
				const double angle{ k * 3.14159265358979323846 / 4 };
				const auto offset{ std::lround(stage_radius * (y_axis ? std::sin(angle) : std::cos(angle)))
					+ (y_axis ? 43 : 4) };
				fx.db(static_cast<byte>(offset & 0xff));
			}
		}
	fx.cmp_imm(KIND_WARD);
	fx.bne("@out");                           // not this slot's kind
	emit_gate(fx, "@fxon", "@park");
	if (fieldblink) {
		fx.lda_abs(RAM_CNT_LO);
		fx.and_imm(fieldblink);
		fx.beq("@park");                      // visual only: POST force is unchanged
	}
	if (fieldpulse) {
		fx.lda_abs(RAM_CNT_LO);
		fx.and_imm(fieldpulse);
		fx.bne("@draw");                      // blink out on the rest window
	}
	else if (tmode == TrigMode::always && !fieldblink)
		fx.jmp("@draw");                      // no gate: no flags to ride
	else
		fx.bne("@draw");                      // gate left A > 0, Z = 0
	fx.label("@park");
	for (word k{ 0 }; k < 4; ++k) {
		const std::string skip{ "@fpk" + std::to_string(k) };
		fx.lda_abs(static_cast<word>(OAM_FX + 4 * k + 1));
		emit_tile_guard(fx, skip);
		fx.lda_imm(0xf8);
		fx.sta_abs(static_cast<word>(OAM_FX + 4 * k));
		fx.label(skip);
	}
	fx.label("@out");
	fx.rts();
	fx.label("@draw");
	const std::vector<byte> spacing{ fieldcount == 1 ? std::vector<byte>{ 0 }
		: fieldcount == 2 ? std::vector<byte>{ 0, 4 }
		: std::vector<byte>{ 0, 2, 4, 6 } };
	for (word k{ 0 }; k < spacing.size(); ++k) {
		if (radial) {
			if (fieldspeed == 0)
				fx.lda_imm(24);               // static always means the full radius
			else {
				fx.lda_abs(RAM_CNT_LO);
				fx.lsr_a(6 - fieldspeed);
				fx.and_imm(0x03);
				if (fieldpattern == "contract") fx.eor_imm(0x03);
				for (int shift{ 0 }; shift < 3; ++shift) fx.asl_a();
			}
			const byte compass{ static_cast<byte>(fielddir == "ccw" ? spacing[k] ^ 7 : spacing[k]) };
			if (compass) fx.ora_imm(compass);
		}
		else {
			if (fieldspeed == 0)
				fx.lda_imm(0x00);             // static compass points
			else {
				fx.lda_abs(RAM_CNT_LO);
				fx.lsr_a(6 - fieldspeed);     // orbit period 8<<(6-speed)
			}
			if (spacing[k]) {
				fx.clc();
				fx.adc_imm(spacing[k]);       // spread the orbs evenly
			}
			fx.and_imm(0x07);
			if (fielddir == "ccw") fx.eor_imm(0x07);
		}
		fx.tax();
		if (edgepark) {
			// Sign-aware edge park. A negative X offset borrows off the left
			// edge (carry clear); a positive offset overflows off the right
			// edge (carry set). Park only that orb instead of drawing it on
			// the opposite side of the screen.
			const std::string neg{ "@fneg" + std::to_string(k) };
			const std::string put{ "@fput" + std::to_string(k) };
			const std::string prk{ "@fprk" + std::to_string(k) };
			const std::string yneg{ "@fyneg" + std::to_string(k) };
			const std::string ycheck{ "@fycheck" + std::to_string(k) };
			const std::string ysto{ "@fy" + std::to_string(k) };
			fx.lda_abs_x(cpu_addr);              // dxc[angle]
			fx.bmi(neg);
			fx.clc();
			fx.adc_zp(ZP_PLAYER_X);
			fx.bcs(prk);                         // off the right edge
			fx.bcc(put);
			fx.label(neg);
			fx.clc();
			fx.adc_zp(ZP_PLAYER_X);
			fx.bcc(prk);                         // off the left edge
			fx.label(put);
			fx.sta_abs(static_cast<word>(OAM_FX + 4 * k + 3));
			// dy + 43 spans [-77, 163], so its sign cannot be read
			// from bit 7: a valid positive offset can exceed 127.
			// All positive values are <= radius + 43; the wrapped
			// negative values are strictly above that bound.
			fx.lda_abs_x(y_table);
			fx.cmp_imm(static_cast<byte>(radius + 44));
			fx.bcs(yneg);
			fx.clc();
			fx.adc_zp(ZP_PLAYER_Y);
			fx.bcs(prk);                         // below byte range
			fx.bcc(ycheck);
			fx.label(yneg);
			fx.clc();
			fx.adc_zp(ZP_PLAYER_Y);
			fx.bcc(prk);                         // above top edge
			fx.label(ycheck);
			fx.cmp_imm(0xf0);
			fx.bcc(ysto);                        // Y=0 is still on screen
			fx.label(prk);
			fx.lda_imm(0xf8);
			fx.label(ysto);
			fx.sta_abs(static_cast<word>(OAM_FX + 4 * k));
		}
		else {
			fx.lda_zp(ZP_PLAYER_X);
			fx.clc();
			fx.adc_abs_x(cpu_addr);              // + dxc[angle]
			fx.sta_abs(static_cast<word>(OAM_FX + 4 * k + 3));
			fx.lda_zp(ZP_PLAYER_Y);
			fx.clc();
			fx.adc_abs_x(y_table);            // + dyc[angle]
			fx.sta_abs(static_cast<word>(OAM_FX + 4 * k));
		}
		if (fieldframes == 1 || fieldanimspeed == 0)
			fx.lda_imm(fieldtile);
		else {
			fx.lda_abs(RAM_CNT_LO);
			fx.lsr_a(6 - fieldanimspeed);
			fx.and_imm(static_cast<byte>(fieldframes - 1));
			fx.clc();
			fx.adc_imm(fieldtile);
		}
		fx.sta_abs(static_cast<word>(OAM_FX + 4 * k + 1));
		fx.lda_imm(fieldattr);
		fx.sta_abs(static_cast<word>(OAM_FX + 4 * k + 2));
	}
	fx.rts();

	const auto fx_off{ klib::Asm6502::get_file_offset(15, cpu_addr) };
	if (static_cast<std::size_t>(cpu_addr) + fx.size() > 0x10000
		|| fx_off + fx.size() > p_rom.size())
		throw std::runtime_error(
			"AtlasDevStatusWard: field graphic does not fit in bank 15");
	for (std::size_t i{ 0 }; i < fx.size(); ++i)
		if (p_rom[fx_off + i] != 0xff)
			throw std::runtime_error("AtlasDevStatusWard: bank 15 space for the field graphic is not free");
	const word entry{ static_cast<word>(cpu_addr + stages * 16) };
	// PRE-lane discipline: a lane is free only while its operand points at
	// the core's RTS stub. Check every lane before writing any so a refusal
	// cannot leave the scheduler half-rewired.
	for (auto pre : pre_sites) {
		const word claimed{ read_pre(pre) };
		if (claimed != stub)
			throw std::runtime_error("AtlasDevStatusWard: a PRE lane is "
				"already claimed by another role; the field graphic needs all three");
	}
	// apply_hack_and_clear empties the assembler. Preserve the size first or
	// this installer reports zero bytes used and the next bank-15 hack can
	// silently overwrite the field graphic.
	const std::size_t fx_size{ fx.size() };
	fx.apply_hack_and_clear(p_rom, 15, cpu_addr);
	for (auto pre : { OFF_PRE0, OFF_PRE1, OFF_PRE2 }) {
		p_rom[scheduler + pre] = entry & 0xff;
		p_rom[scheduler + pre + 1] = entry >> 8;
	}
	return static_cast<word>(cpu_addr + fx_size);
}
