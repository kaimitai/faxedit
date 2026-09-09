#include "HackManager.h"
#include "AtlasDevFrameScheduler.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

// ladder crown: stop an earned ascent with the hero's feet on the top rung,
// or at the adjacent floor. ordinary falls still pass through ladder shafts.
// optional down hold and alignment apply only while an earned crown is valid.
// fixed-bank code uses the existing general-hack cursor. state is separate
// from the native scheduler, jump buffer and palette/clock roles.
namespace {
	class Code : public klib::Asm6502 {
		bool m_jump_buffer;
	public:
		explicit Code(bool jump_buffer = false) : m_jump_buffer(jump_buffer) {}
		void climb_tick() { if (m_jump_buffer) jsr("jump_buffer_tick"); }
	};
	constexpr word RUNG{ 0x04e0 }, HOLD{ 0x04e1 };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	struct Settings {
		std::string mode, policy;
		byte hold, align;
		std::vector<std::pair<byte, byte>> rooms;
		bool plain() const { return hold == 0 && align == 0 && policy == "all"; }
	};

	[[noreturn]] void fail(const std::string& message) {
		throw std::runtime_error("AtlasDevLadderCrown: " + message);
	}

	byte numeric_byte(std::string_view token) {
		int base{ 10 };
		if (token.starts_with("0x") || token.starts_with("0X")) {
			base = 16; token.remove_prefix(2);
		}
		else if (token.starts_with("0b") || token.starts_with("0B")) {
			base = 2; token.remove_prefix(2);
		}
		else if (token.starts_with('$')) { base = 16; token.remove_prefix(1); }
		else if (token.starts_with('%')) { base = 2; token.remove_prefix(1); }
		if (token.empty() || token.front() == '-' || token.front() == '+')
			fail("expected an unsigned byte");
		unsigned value{};
		const auto result{ std::from_chars(token.data(), token.data() + token.size(), value, base) };
		if (result.ec != std::errc{} || result.ptr != token.data() + token.size() || value > 255)
			fail("expected an unsigned byte");
		return static_cast<byte>(value);
	}

	Settings settings(const fh::GeneralHack& hack) {
		Settings s{ hack.string_or("mode", "crown"), hack.string_or("roompolicy", "all"),
			numeric_byte(hack.string_or("downhold", "0")), numeric_byte(hack.string_or("align", "0")), {} };
		if (s.mode != "crown" && s.mode != "floor" && s.mode != "vanilla")
			fail("mode must be crown, floor or vanilla");
		if (s.policy != "all" && s.policy != "allow" && s.policy != "deny")
			fail("roompolicy must be all, allow or deny");
		if (s.hold > 8 || s.align > 2)
			fail("downhold must be 0 to 8 and align must be 0 to 2");
		if (hack.has_param("rooms")) {
			std::string_view remaining{ hack.get_string("rooms") };
			for (;;) {
				const auto plus{ remaining.find('+') };
				const auto pair{ remaining.substr(0, plus) };
				const auto colon{ pair.find(':') };
				if (colon == std::string_view::npos || pair.find(':', colon + 1) != std::string_view::npos)
					fail("rooms must contain area:screen pairs separated by +");
				s.rooms.emplace_back(numeric_byte(pair.substr(0, colon)), numeric_byte(pair.substr(colon + 1)));
				if (s.rooms.size() > 8) fail("rooms accepts at most eight pairs");
				if (plus == std::string_view::npos) break;
				remaining.remove_prefix(plus + 1);
			}
		}
		std::sort(s.rooms.begin(), s.rooms.end());
		if (std::adjacent_find(s.rooms.begin(), s.rooms.end()) != s.rooms.end())
			fail("rooms must not contain duplicate pairs");
		if ((s.policy == "all") != s.rooms.empty())
			fail("all takes no rooms; allow and deny require rooms");
		if (s.mode != "crown" && (s.hold || s.align))
			fail("downhold and align require crown mode");
		if (s.mode == "vanilla" && !s.plain())
			fail("vanilla mode cannot use crown settings");
		return s;
	}

	void save(Code& c) {
		c.db(0x08); c.pha(); c.txa(); c.pha(); c.tya(); c.pha();
		c.lda_zp(0x00); c.pha(); c.lda_zp(0xb6); c.pha();
	}

	void restore(Code& c) {
		c.pla(); c.sta_zp(0xb6); c.pla(); c.sta_zp(0x00);
		c.pla(); c.tay(); c.pla(); c.tax(); c.pla(); c.db(0x28);
	}

	void far(Code& c, byte branch, const std::string& label) {
		c.db(static_cast<byte>(branch ^ 0x20)); c.db(3); c.jmp(label);
	}

	void inline_guards(Code& c, const std::string& fallback) {
		c.lda_zp(0x54); c.cmp_imm(0xff); far(c, 0xd0, fallback);
		c.lda_zp(0xa5); c.and_imm(0x82); far(c, 0xd0, fallback);
		c.lda_zp(0x9e); c.cmp_imm(0xf1); far(c, 0xb0, fallback);
	}

	void emit_gate(Code& c, const Settings& s) {
		c.label("gate");
		c.lda_zp(0x54); c.cmp_imm(0xff); c.bne("gate_no");
		c.lda_zp(0xa5); c.and_imm(0x82); c.bne("gate_no");
		c.lda_zp(0x9e); c.cmp_imm(0xf1); c.bcs("gate_no");
		if (s.policy != "all") {
			c.ldx_imm(static_cast<byte>(s.rooms.size() - 1));
			c.label("room_loop");
			c.lda_abs_x("room_areas"); c.cmp_zp(0x24); c.bne("room_next");
			c.lda_abs_x("room_screens"); c.cmp_zp(0x63); c.beq("room_match");
			c.label("room_next"); c.dex(); c.bpl("room_loop");
			c.jmp(s.policy == "allow" ? "gate_no" : "gate_yes");
			c.label("room_match"); c.jmp(s.policy == "allow" ? "gate_yes" : "gate_no");
		}
		c.label("gate_yes"); c.sec(); c.rts();
		c.label("gate_no"); c.clc(); c.rts();
		if (s.policy != "all") {
			c.label("room_areas"); for (const auto& room : s.rooms) c.db(room.first);
			c.label("room_screens"); for (const auto& room : s.rooms) c.db(room.second);
		}
	}

	void up_aligned(Code& c, const std::string& fallback) {
		c.lda_zp(0x16); c.and_imm(0x0c); c.cmp_imm(0x08); far(c, 0xd0, fallback);
		c.lda_zp(0x9e); c.and_imm(0x0f); far(c, 0xd0, fallback);
	}

	void remaining(Code& c, const std::string& handle, const std::string& fallback) {
		// compare the unsigned 8.8 remainder with the live ascent operands.
		c.lda_zp(0xa1); c.and_imm(0x0f); c.cmp_abs(0xe328);
		far(c, 0x90, handle); far(c, 0xd0, fallback);
		c.lda_zp(0xa0); c.cmp_abs(0xe322);
		far(c, 0x90, handle); far(c, 0xf0, handle); c.jmp(fallback);
	}

	void emit_crown(Code& c, const Settings& s) {
		const bool plain{ s.plain() };
		const auto clear_hold{ [&] {
			if (s.hold) { c.lda_imm(0); c.sta_abs(HOLD); }
		} };
		const auto guards{ [&](const std::string& fallback) {
			if (plain) inline_guards(c, fallback);
			else { c.jsr("gate"); far(c, 0x90, fallback); }
		} };
		c.label("ascent_entry"); save(c);
		guards("ascent_clear"); up_aligned(c, "ascent_clear");
		c.lda_zp(0xa4); c.and_imm(0x18); c.cmp_imm(0x18); far(c, 0xd0, "ascent_clear");
		c.lda_zp(0xa1); c.and_imm(0xf0); c.sta_zp(0xb6);
		c.jsr("crown_shape"); far(c, 0x90, "ascent_clear");
		// only an actual aligned ascent earns the saved top-rung index.
		c.lda_zp(0); c.sec(); c.sbc_imm(0x10); c.sta_abs(RUNG); clear_hold();
		remaining(c, "support", "ascent_fallback");
		c.label("ascent_clear"); c.lda_imm(0); c.sta_abs(RUNG); clear_hold();
		c.label("ascent_fallback"); c.climb_tick(); restore(c); c.ldx_imm(2); c.jsr(0xe6c8); c.jmp(0xe30c);

		c.label("entry_guard"); save(c);
		c.lda_abs(RUNG); far(c, 0xf0, plain ? "entry_fallback" : "entry_clear");
		guards("entry_clear");
		c.lda_zp(0xa4); c.and_imm(1); far(c, 0xd0, "entry_clear");
		c.lda_abs(RUNG); c.cmp_imm(0x20); far(c, 0x90, "entry_clear");
		c.cmp_imm(0xc0); far(c, 0xb0, "entry_clear");
		c.and_imm(0xf0); c.sec(); c.sbc_imm(0x20); c.sta_zp(0xb6);
		c.jsr("crown_shape"); far(c, 0x90, "entry_clear");
		c.lda_zp(0); c.sec(); c.sbc_imm(0x10); c.cmp_abs(RUNG); far(c, 0xd0, "entry_clear");
		c.lda_zp(0xa1); c.cmp_zp(0xb6); far(c, 0x90, "entry_clear");
		far(c, 0xf0, "at_crown");
		c.and_imm(0xf0); c.cmp_zp(0xb6); far(c, 0xd0, "entry_clear");
		up_aligned(c, "entry_clear"); clear_hold(); remaining(c, "support", "entry_fallback");

		c.label("at_crown"); c.lda_zp(0x16); c.and_imm(4); c.beq("crown_not_down");
		c.lda_zp(0x9e); c.and_imm(0x0f);
		if (plain) c.bne("support");
		else {
			c.beq("down_aligned");
			if (s.align) {
				// snap the integer coordinate only, toward the owned rung.
				c.lda_abs(RUNG); c.and_imm(0x0f);
				for (int i{}; i < 4; ++i) c.asl_a();
				c.sta_zp(0); c.lda_zp(0x9e); c.sec(); c.sbc_zp(0);
				c.cmp_imm(static_cast<byte>(s.align + 1)); c.bcc("align_apply");
				c.cmp_imm(static_cast<byte>(256 - s.align)); c.bcc("down_unaligned");
				c.label("align_apply"); c.lda_zp(0); c.sta_zp(0x9e); c.jmp("down_aligned");
			}
			c.label("down_unaligned"); clear_hold(); c.jmp("support");
			c.label("down_aligned");
			if (s.hold) {
				// n completed movement calls wait; n+1 starts down. readiness
				// persists through fractional steps, until release or departure.
				c.lda_abs(HOLD); c.cmp_imm(s.hold); c.bcs("down_ready");
				c.inc_abs(HOLD); c.jmp("support");
			}
			c.label("down_ready");
		}
		c.lda_zp(0xa4); c.ora_imm(0x18); c.sta_zp(0xa4); restore(c); c.jmp(0xe349);
		c.label("crown_not_down"); clear_hold(); c.lda_zp(0x16); c.and_imm(8);
		if (plain) c.beq("support");
		else far(c, 0xf0, "support");
		remaining(c, "support", "fraction_ascent");
		c.label("fraction_ascent"); c.climb_tick(); restore(c); c.jmp(0xe30e);

		c.label("support"); c.lda_zp(0xb6); c.sta_zp(0xa1); c.lda_imm(0); c.sta_zp(0xa0);
		c.lda_zp(0xa4); c.and_imm(0xe7); c.sta_zp(0xa4); restore(c); c.jmp(0xe43a);
		c.label("entry_clear"); c.lda_imm(0); c.sta_abs(RUNG); clear_hold();
		c.label("entry_fallback"); restore(c); c.lda_zp(0xa5); c.bmi("entry_wings"); c.jmp(0xe2d6);
		c.label("entry_wings"); c.jmp(0xe2cc);

		// two air rows, an ordinary rung, and an immediately adjacent floor.
		c.label("crown_shape"); c.lda_zp(0xb6); c.cmp_imm(0x91); far(c, 0xb0, "crown_no");
		c.lda_zp(0x9e); c.clc(); c.adc_imm(7); c.lsr_a(4); c.sta_zp(0);
		c.lda_zp(0xb6); c.ora_zp(0); c.tax();
		for (int row{}; row < 2; ++row) {
			c.ldy_abs_x(0x0600); c.jsr(0xe8c6);
			c.cmp_imm(2); c.beq("crown_no"); c.cmp_imm(10); c.beq("crown_no");
			c.tay(); c.lda_abs_y(0xe8d9); c.bne("crown_no");
			c.txa(); c.clc(); c.adc_imm(0x10); c.tax();
		}
		c.ldy_abs_x(0x0600); c.jsr(0xe8c6); c.cmp_imm(2); c.bne("crown_no");
		c.txa(); c.clc(); c.adc_imm(0x10); c.tax(); c.sta_zp(0); c.and_imm(0x0f);
		c.beq("crown_right"); c.dex(); c.jsr(0xe87c); c.bne("crown_yes");
		c.label("crown_right"); c.ldx_zp(0); c.txa(); c.and_imm(0x0f); c.cmp_imm(0x0f);
		c.beq("crown_no"); c.inx(); c.jsr(0xe87c); c.bne("crown_yes");
		c.label("crown_no"); c.clc(); c.rts();
		c.label("crown_yes"); c.sec(); c.rts();

		// room setup clears ownership after its internal backward loop.
		c.label("reset_wrapper"); c.lda_imm(0); c.sta_abs(RUNG);
		if (s.hold) c.sta_abs(HOLD);
		c.jmp(0xcd6f);
		if (!plain) emit_gate(c, s);
	}

	void emit_floor(Code& c, const std::string& prefix) {
		const auto name{ [&](const std::string& label) { return prefix + label; } };
		const auto guard{ [&](const std::string& fallback) {
			c.lda_zp(0x54); c.cmp_imm(0xff); c.bne(name(fallback));
			c.lda_zp(0xa5); c.and_imm(0x82); c.bne(name(fallback));
			c.lda_zp(0x9e); c.cmp_imm(0xf1); c.bcs(name(fallback));
			c.lda_zp(0xa1); c.and_imm(0xf0); c.sta_zp(0xb6);
		} };
		c.label(name("ascent_entry")); save(c); guard("ascent_fallback");
		c.jsr(name("cap_shape")); c.bcc(name("ascent_fallback"));
		c.lda_zp(0xa1); c.and_imm(0x0f); c.cmp_abs(0xe328);
		c.bcc(name("ascent_handle")); c.bne(name("ascent_fallback"));
		c.lda_zp(0xa0); c.cmp_abs(0xe322); c.bcc(name("ascent_handle")); c.bne(name("ascent_fallback"));
		c.label(name("ascent_handle")); c.lda_zp(0xb6); c.sta_zp(0xa1);
		c.lda_imm(0); c.sta_zp(0xa0); c.lda_zp(0xa4); c.and_imm(0xef); c.sta_zp(0xa4);
		restore(c); c.jmp(0xe43a);
		c.label(name("ascent_fallback")); c.climb_tick(); restore(c); c.ldx_imm(2); c.jsr(0xe6c8); c.jmp(0xe30c);

		c.label(name("entry_guard")); save(c); guard("entry_fallback");
		c.lda_zp(0xa4); c.and_imm(1); c.beq(name("entry_fallback"));
		c.lda_zp(0x16); c.and_imm(0x0c); c.cmp_imm(8); c.bne(name("entry_fallback"));
		c.jsr(name("cap_shape")); c.bcs(name("entry_handle"));
		c.lda_zp(0xb6); c.clc(); c.adc_imm(0x10); c.bcs(name("entry_fallback"));
		c.sta_zp(0xb6); c.jsr(name("cap_shape")); c.bcc(name("entry_fallback"));
		c.label(name("entry_handle")); c.lda_zp(0xa4); c.and_imm(0xef); c.sta_zp(0xa4);
		restore(c); c.jmp(0xe399);
		c.label(name("entry_fallback")); restore(c); c.lda_zp(0xa5); c.bmi(name("entry_wings")); c.jmp(0xe2d6);
		c.label(name("entry_wings")); c.jmp(0xe2cc);

		c.label(name("cap_shape")); c.lda_zp(0xb6); c.cmp_imm(0xa1); c.bcs(name("cap_no"));
		c.lda_zp(0x9e); c.clc(); c.adc_imm(7); c.lsr_a(4); c.sta_zp(0);
		c.lda_zp(0xb6); c.ora_zp(0); c.tax(); c.ldy_abs_x(0x0600); c.jsr(0xe8c6);
		c.cmp_imm(2); c.beq(name("cap_no")); c.cmp_imm(10); c.beq(name("cap_no"));
		c.tay(); c.lda_abs_y(0xe8d9); c.bne(name("cap_no"));
		c.txa(); c.clc(); c.adc_imm(0x10); c.tax(); c.ldy_abs_x(0x0600); c.jsr(0xe8c6);
		c.cmp_imm(2); c.bne(name("cap_no"));
		c.txa(); c.clc(); c.adc_imm(0x10); c.tax(); c.sta_zp(0); c.and_imm(0x0f);
		c.beq(name("cap_right")); c.dex(); c.jsr(0xe87c); c.bne(name("cap_yes"));
		c.label(name("cap_right")); c.ldx_zp(0); c.txa(); c.and_imm(0x0f); c.cmp_imm(0x0f);
		c.beq(name("cap_no")); c.inx(); c.jsr(0xe87c); c.bne(name("cap_yes"));
		c.label(name("cap_no")); c.clc(); c.rts();
		c.label(name("cap_yes")); c.sec(); c.rts();
	}

	void emit_floor_shims(Code& c, const Settings& s) {
		emit_floor(c, "legacy_");
		for (const auto& entry : { std::string{ "ascent_entry" }, std::string{ "entry_guard" } }) {
			c.label(entry); save(c); c.jsr("gate"); c.bcc(entry + "_denied");
			restore(c); c.jmp("legacy_" + entry);
			c.label(entry + "_denied");
			if (entry == "ascent_entry") c.climb_tick();
			restore(c);
			if (entry == "ascent_entry") { c.ldx_imm(2); c.jsr(0xe6c8); c.jmp(0xe30c); }
			else {
				c.lda_zp(0xa5); c.bmi("floor_wings"); c.jmp(0xe2d6);
				c.label("floor_wings"); c.jmp(0xe2cc);
			}
		}
		emit_gate(c, s);
	}

	std::size_t offset(word cpu) { return 0x30010 + static_cast<std::size_t>(cpu); }

	void require_span(const std::vector<byte>& rom, word cpu, std::size_t length) {
		const auto start{ offset(cpu) };
		if (cpu < 0xc000 || length > 0x10000U - cpu
			|| start > rom.size() || length > rom.size() - start)
			fail(std::format("incomplete fixed-bank span at ${:04x}", cpu));
	}

	void require_site(const std::vector<byte>& rom, word cpu, std::initializer_list<byte> expected) {
		require_span(rom, cpu, expected.size());
		if (!std::equal(expected.begin(), expected.end(), rom.begin() + offset(cpu)))
			fail(std::format("the required instructions at ${:04x} are not intact", cpu));
	}

	void require_speed(const std::vector<byte>& rom, word low, word high, byte arithmetic) {
		// the immediate values remain configurable, but their instruction
		// envelopes and bounded 8.8 meaning cannot change.
		require_span(rom, low, 7); require_span(rom, high, 6);
		require_site(rom, low, { 0xa5, 0xa0, static_cast<byte>(arithmetic == 0xe9 ? 0x38 : 0x18), arithmetic });
		require_site(rom, static_cast<word>(low + 5), { 0x85, 0xa0 });
		require_site(rom, high, { 0xa5, 0xa1, arithmetic });
		require_site(rom, static_cast<word>(high + 4), { 0x85, 0xa1 });
		const unsigned step{ rom[offset(low) + 4] | (static_cast<unsigned>(rom[offset(high) + 3]) << 8) };
		if (step < 1 || step > 0x0800) fail("ladder speed is outside 1 to 2048 subpixels per movement call");
	}

	void require_source(const std::vector<byte>& rom, const Settings& s) {
		constexpr std::array<byte, 4> magic{ 0x4e, 0x45, 0x53, 0x1a };
		if (rom.size() != ROM_SIZE || !std::equal(magic.begin(), magic.end(), rom.begin())
			|| rom[4] != 0x10 || rom[5] != 0 || (rom[6] & 0xfc) != 0x10
			|| (rom[7] != 0 && rom[7] != 8)
			|| (rom[7] == 8 && ((rom[8] & 0x0f) != 0 || rom[9] != 0)))
			fail("requires an unexpanded MMC1 iNES or NES2 ROM without trainer or CHR ROM");
		// mirroring, battery flags and unused padding do not affect bank layout.
		// check compatibility at the movement code below.
		if (s.mode == "crown") {
			// This installer does not interpret arbitrary preinstalled handlers.
			// Configured companions are installed after Crown from clean inputs.
			using namespace fh::afs;
			if (const word base{ find_base(rom) }; base != 0) {
				require_span(rom, base, CORE_SIZE);
				const word stub{ static_cast<word>(base + OFF_STUB) };
				for (const auto position : { OFF_PRE0, OFF_PRE1, OFF_PRE2, OFF_POST })
					if (Code::read_word(rom, offset(base) + position) != stub)
						fail("preinstalled scheduler handlers are unsupported; rebuild from a clean ROM");
				if (rom[offset(base) + OFF_POSTARMED] != 0)
					fail("preinstalled scheduler handlers are unsupported; rebuild from a clean ROM");
			}
			else {
				require_site(rom, 0xc9af, { 0xa9, 7, 0x8d, 0x14, 0x40 });
				require_site(rom, 0xc9de, { 0x8d, 1, 0x20, 0xa5, 0x5a });
			}
		}
		require_site(rom, 0xe307, { 0xa2, 2, 0x20, 0xc8, 0xe6 });
		require_site(rom, 0xe2c8, { 0xa5, 0xa5, 0x10, 0x0a });
		// tile property decoding, solid lookup, and the collision dispatch.
		require_site(rom, 0xe8c6, { 0x98, 0x4a, 0xa8, 0x90, 8, 0xb9, 0x3c, 4, 0x4a, 0x4a, 0x4a, 0x4a, 0x60,
			0xb9, 0x3c, 4, 0x29, 0x0f, 0x60 });
		require_site(rom, 0xe87c, { 0xbc, 0, 6, 0x20, 0xc6, 0xe8, 0xa8, 0xb9, 0xd9, 0xe8, 0x60 });
		require_site(rom, 0xe86c, { 0xa5, 0xb6, 0x29, 0xf0, 0x85, 0, 0xa5, 0xb5, 0x4a, 0x4a, 0x4a, 0x4a, 5, 0, 0xaa, 0x60 });
		require_site(rom, 0xe6c8, { 0x8a, 0xf0, 0xc6, 0xca, 0xf0, 0x8d, 0xca, 0xf0, 0x53 });
		require_site(rom, 0xe30c, { 0xd0, 0x35, 0xe6, 0xa3, 0xa5, 0xa5, 0x10, 0x0a });
		require_site(rom, 0xe43a, { 0xa5, 0xa4, 0x29, 0xfb, 0x85, 0xa4, 0xa9, 0, 0x85, 0xb1 });
		// these paths remain reachable after leaving a perch. separately
		// installed jump or fall replacements cannot silently take ownership.
		require_site(rom, 0xe3cd, { 0xa5, 0xa5, 0x10, 0 });
		require_site(rom, 0xe444, { 0xa5, 0xa4, 0x4a, 0xb0, 0x1a });
		require_site(rom, 0xe463, { 0xa6, 0xa6, 0xe0, 0x10 });
		require_site(rom, 0xe3d1, { 0xa5, 0xa4, 9, 4, 0x85, 0xa4 });
		require_site(rom, 0xe3f5, { 0xa5, 0xa1, 0x18, 0x69, 8, 0x85, 0xa1 });
		require_site(rom, 0xe182, { 0xa5, 0xa4, 0x29, 5, 0xf0, 0x0f });
		require_speed(rom, 0xe31e, 0xe325, 0xe9);
		require_speed(rom, 0xe36c, 0xe373, 0x69);
		if (s.mode == "crown") {
			require_site(rom, 0xc145, { 0x20, 0x6f, 0xcd });
			require_site(rom, 0xcd6f, { 0xa9, 9, 0x85, 0x99, 0xa9, 0, 0x85, 0x98, 0x60 });
			require_site(rom, 0xe349, { 0xa5, 0xa4, 0x29, 0xda, 0x85, 0xa4, 0xa2, 3, 0x20, 0xc8, 0xe6 });
		}
		else require_site(rom, 0xe399, { 0xa5, 0xa6, 0xc9, 0x20, 0x90, 6 });
	}

	void require_ram(const fe::Config& config) {
		// Check Crown's two cells against declared script storage, including
		// physical CPU RAM mirrors. No other feature's allocation is changed.
		const auto check{[&](const char* key, std::size_t fallback, std::size_t size = 1) {
			const auto start{config.constant_or(key, fallback)};
			if (!size || size > 0x800 || start >= 0x2000 || size > 0x2000 - start)
				fail(std::string(key) + " is outside internal CPU RAM");
			for (std::size_t i{}; i < size; ++i) {
				const auto cell{(start + i) & 0x7ff};
				if (cell == RUNG || cell == HOLD)
					fail(std::format("{} overlaps Crown RAM at ${:04x}", key, cell));
			}
		}};
		check("hack_script_jsr_ram_addr_lo", 0x0182);
		check("hack_script_jsr_ram_addr_hi", 0x0183);
		check("hack_script_selected_flag_ram_addr", 0x0184);
		const auto count{config.constant_or("hack_script_var_count", 8)};
		if (count < 1 || count > 128) fail("hack_script_var_count must be 1 to 128");
		check("hack_script_var_ram_addr", 0x03b5, count);
	}
}

word fh::HackManager::install_AtlasDevLadderCrown(const fe::Config& config, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack, bool jump_buffer) const {
	const Settings s{ settings(hack) };
	if (s.mode == "vanilla") return cpu_addr;
	require_source(rom, s);
	if (s.mode == "crown") require_ram(config);
	if (jump_buffer) require_site(rom, 0xe34f, { 0xa2, 3, 0x20, 0xc8, 0xe6 });

	Code code(jump_buffer);
	if (s.mode == "crown") emit_crown(code, s);
	else if (s.policy == "all") emit_floor(code, "");
	else emit_floor_shims(code, s);
	if (jump_buffer) {
		// E6C8 starts with TXA: incoming A/N/Z are dead, while C/V, X/Y
		// survive this countdown. Only the buffer nibble ages while climbing.
		code.label("descent_entry"); code.jsr("jump_buffer_tick");
		code.ldx_imm(3); code.jsr(0xe6c8); code.jmp(0xe354);
		code.label("jump_buffer_tick"); code.lda_abs(0x04df); code.and_imm(0x0f);
		code.beq("jump_buffer_done"); code.dec_abs(0x04df);
		code.label("jump_buffer_done"); code.rts();
	}
	const std::size_t size{ code.size() };
	if (!jump_buffer && s.plain() && size != (s.mode == "crown" ? 605 : 315))
		fail("unexpected default body size");
	// use the space assigned by faxedit, including reclaimed bytes.
	// check physical bounds here; the caller checks capacity.
	require_span(rom, cpu_addr, size);

	// keep label resolution and every write private until the whole install
	// succeeds. callers never observe a half-written hook or body.
	std::vector<byte> patched{ rom };
	const auto address{ [&](const std::string& name) {
		return static_cast<word>(cpu_addr + code.label_position(name));
	} };
	const word ascent{ address("ascent_entry") }, entry{ address("entry_guard") };
	const word reset{ s.mode == "crown" ? address("reset_wrapper") : word{} };
	const word descent{ jump_buffer ? address("descent_entry") : word{} };
	code.apply_hack_and_clear(patched, 15, cpu_addr);
	code.jmp(ascent); code.nop(2); code.apply_hack_and_clear(patched, 15, 0xe307);
	code.jmp(entry); code.nop(1); code.apply_hack_and_clear(patched, 15, 0xe2c8);
	if (jump_buffer) { code.jmp(descent); code.nop(2); code.apply_hack_and_clear(patched, 15, 0xe34f); }
	if (s.mode == "crown") { code.jsr(reset); code.apply_hack_and_clear(patched, 15, 0xc145); }
	rom = std::move(patched);
	return static_cast<word>(cpu_addr + size);
}
