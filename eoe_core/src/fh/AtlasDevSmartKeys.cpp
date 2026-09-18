#include "HackManager.h"
#include "GeneralHack.h"
#include "fe/Config.h"
#include "fe/fe_constants.h"
#include "common/klib/Asm6502.h"

#include <array>
#include <cstddef>
#include <format>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

// smart keys: open a locked door with a matching key the hero is carrying,
// without selecting it first and without touching the selected item.
//
// the door gate is an rts trick jump table at $eb3f, indexed by the door's
// requirement byte $042b. entries 1 to 5 are the five keys, each a sixteen
// byte handler that compares the selected item $03c1 with its key and shows
// a refusal message when they differ. entries 6 to 8 are the rings, which
// test ownership in $042c instead.
//
// when the selected item is the right key, the door opens exactly as in
// vanilla and that key is spent. otherwise one matching key is taken from the
// item list at $03ad, the list closes up around the gap, the count at $03c6
// drops by one, and the selected item is left alone. a door the hero has no
// key for refuses with its usual message.
//
// the five key handlers occupy the eighty bytes from $eb51 to $eba0, and the
// body replaces them in place, so no general hack space is used. the
// dispatcher leaves twice the requirement in y and never changes it, so one
// shared handler serves all five entries and derives the key from y. entry 0
// and the three ring handlers are not touched.
//
// every site is verified against its exact vanilla bytes before anything is
// written, and a rom that differs is refused.
//
// permadoors, listed before this hack, replaces the requirement load at $eb2f
// with a call to its check routine (an opened door reads as no requirement) and
// the deselect at $ebd9 with a call to its set-flag routine. the shared handler
// never passes $ebd9 on the carried path, so when those two calls are found
// the carried path goes through a small stub in general hack space that calls
// the set-flag routine with the selected item saved around it, since that
// routine deselects the key it assumes was used. the selected-key path is
// vanilla and permadoors' own hooks see it. listed the other way round,
// permadoors would install after this hack and carried keys would open doors
// it never remembers, so that order is refused.
namespace {
	constexpr word ORG{ 0xeb51 }, END{ 0xeba1 };
	constexpr word TABLE{ 0xeb3f };
	constexpr word SELECTED{ 0x03c1 }, ITEM_COUNT{ 0x03c6 }, ITEM_ARRAY{ 0x03ad };
	constexpr word VANILLA_TAIL{ 0xebd1 };   // message, destroy $03c1, clear gate
	constexpr word UNLOCK_TAIL{ 0xebe1 };    // clear gate and sound only
	constexpr word DISPATCH_LDA{ 0xeb2f };   // lda $042b, or permadoors' jsr
	constexpr word DESELECT{ 0xebd9 };       // lda #$ff / sta $03c1, or permadoors' jsr + 2 nops
	constexpr word FAR_CALL{ 0xf859 };
	constexpr byte MESSAGE_BANK{ 0x0c };
	constexpr word MESSAGE_TARGET{ 0x8241 };
	constexpr byte USED_KEY{ 0x84 };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	constexpr std::array<word, 5> HANDLERS{ 0xeb51, 0xeb61, 0xeb71, 0xeb81, 0xeb91 };
	constexpr std::array<byte, 5> REFUSALS{ 0x7d, 0x7c, 0x7b, 0x02, 0x7e };

	[[noreturn]] void fail(const std::string& message) {
		throw std::runtime_error("AtlasDevSmartKeys: " + message);
	}

	void require_site(const std::vector<byte>& rom, word cpu,
		std::initializer_list<byte> expected) {
		const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
		std::size_t i{ 0 };
		for (byte b : expected)
			if (off + i >= rom.size() || rom[off + i++] != b)
				fail(std::format("the door key gate at ${:04x} is not vanilla", cpu));
	}

	struct PermaDoors { bool present{ false }; word check{ 0 }; word set_flag{ 0 }; };
	// the requirement load is vanilla, or permadoors' two calls in the shape its
	// installer writes them; anything else at $eb2f is refused by name
	PermaDoors detect_permadoors(const std::vector<byte>& rom) {
		const auto at{ [&](word cpu) { return rom[klib::Asm6502::get_file_offset(15, cpu)]; } };
		const auto word_at{ [&](word cpu) { return static_cast<word>(at(cpu) | (at(cpu + 1) << 8)); } };
		if (at(DISPATCH_LDA) == 0xad) {
			require_site(rom, DISPATCH_LDA, { 0xad, 0x2b, 0x04 });
			return {};
		}
		if (at(DISPATCH_LDA) != 0x20 || at(DESELECT) != 0x20
			|| at(DESELECT + 3) != 0xea || at(DESELECT + 4) != 0xea)
			fail("the door key gate at $eb2f is hooked by something other than PermaDoors");
		return { true, word_at(DISPATCH_LDA + 1), word_at(DESELECT + 1) };
	}
	// the five key handlers, and the table entries that reach them
	void require_gate(const std::vector<byte>& rom) {
		if (rom.size() != ROM_SIZE)
			fail("expected a 256 KiB rom with a sixteen byte header");
		require_site(rom, DISPATCH_LDA + 3, { 0xf0, 0x0a, 0x0a, 0xa8 });
		for (std::size_t i{ 0 }; i < HANDLERS.size(); ++i) {
			require_site(rom, HANDLERS[i],
				{ 0xad, 0xc1, 0x03, 0xc9, static_cast<byte>(0x04 + i) });
			const word stored{ static_cast<word>(HANDLERS[i] - 1) };
			require_site(rom, static_cast<word>(TABLE + 2 * (i + 1)),
				{ static_cast<byte>(stored & 0xff), static_cast<byte>(stored >> 8) });
		}
		// the success tails this hack jumps into
		require_site(rom, VANILLA_TAIL, { 0xa9, 0x84, 0x20, 0x59, 0xf8 });
		require_site(rom, UNLOCK_TAIL, { 0xa9, 0x00, 0x8d, 0x2b, 0x04 });
	}

	void far_call(klib::Asm6502& code) {
		code.jsr(FAR_CALL);
		code.db(MESSAGE_BANK);
		code.dw(MESSAGE_TARGET);
	}

	// cpx abs has no wrapper in the assembler; emit it directly
	void cpx_abs(klib::Asm6502& code, word address) {
		code.db(0xec);
		code.dw(address);
	}

	void emit_body(klib::Asm6502& code, word unlock) {
		// y holds twice the requirement. lsr leaves the requirement in both a
		// and y, and the key id is the requirement plus 3, $04 to $08.
		code.tya(); code.lsr_a(); code.tay(); code.clc(); code.adc_imm(3);
		code.cmp_abs(SELECTED);
		code.bne("carried");
		code.jmp(VANILLA_TAIL);          // already selected: exactly vanilla

		// refuse a count above eight rather than read past the list
		code.label("carried");
		code.ldx_abs(ITEM_COUNT); code.cpx_imm(9);
		code.bcs("absent");

		// scan down from the count; for identical items the last match is as
		// good as the first
		code.label("search");
		code.dex();
		code.bmi("absent");
		code.cmp_abs_x(ITEM_ARRAY);
		code.bne("search");

		// close the gap, the way the engine's own item removal does
		code.inx();
		code.label("shift");
		cpx_abs(code, ITEM_COUNT);
		code.bcs("removed");
		code.lda_abs_x(ITEM_ARRAY); code.dex(); code.sta_abs_x(ITEM_ARRAY);
		code.inx(); code.inx();
		code.bne("shift");

		code.label("removed");
		code.dec_abs(ITEM_COUNT);
		code.lda_imm(USED_KEY);
		far_call(code);
		// the unlock tail, not the vanilla one: $ebd1 destroys $03c1, and the
		// key did not come from there. with permadoors this is its remember stub.
		code.jmp(unlock);

		code.label("absent");
		// y is 1 to 5, so the table is indexed from the byte before it, which
		// is this rts. y is never zero, so the rts itself is never read.
		code.lda_abs_y("messages_base");
		far_call(code);
		code.label("messages_base");
		code.rts();
		code.label("messages");
		for (byte m : REFUSALS) code.db(m);
	}
}

word fh::HackManager::install_AtlasDevSmartKeys(const fe::Config& config,
	std::vector<byte>& rom, word cpu_addr, const fh::GeneralHack& hack) const {
	const std::string mode{ hack.string_or("mode", "carried") };
	if (mode != "carried" && mode != "vanilla")
		fail("mode must be carried or vanilla");
	if (mode == "vanilla") return cpu_addr;
	// permadoors after this hack would install over a gate that no longer
	// passes its second hook on the carried path
	bool seen_self{ false };
	for (const auto& listed : fh::parse_general_hacks(config.string_or_empty(fe::c::ID_GENERAL_HACKS))) {
		if (listed.get_type() == fh::GeneralHackLib::AtlasDevSmartKeys) seen_self = true;
		else if (seen_self && listed.get_type() == fh::GeneralHackLib::PermaDoors)
			fail("list PermaDoors before AtlasDevSmartKeys, so doors opened with a carried key are remembered");
	}
	require_gate(rom);
	const PermaDoors permadoors{ detect_permadoors(rom) };
	// keep every write private until the whole install succeeds
	std::vector<byte> patched{ rom };
	word unlock{ UNLOCK_TAIL };
	word next{ cpu_addr };
	if (permadoors.present) {
		// remember the door the way the selected-key path does, without losing
		// the selected item: permadoors' set-flag routine deselects on return
		klib::Asm6502 stub;
		stub.lda_abs(SELECTED); stub.pha();
		stub.jsr(permadoors.set_flag);
		stub.pla(); stub.sta_abs(SELECTED);
		stub.jmp(UNLOCK_TAIL);
		unlock = cpu_addr;
		next = stub.apply_hack_and_clear_get_next_cpu_addr(patched, 15, cpu_addr);
	}

	klib::Asm6502 code;
	emit_body(code, unlock);
	if (code.size() > END - ORG)
		fail(std::format("body is {} bytes and only {} fit in place of the key handlers",
			code.size(), END - ORG));

	code.apply_hack_and_clear(patched, 15, ORG);
	// point entries 1 to 5 at the shared handler. the rts trick stores the
	// target address minus one; entries 0 and 6 to 8 are left alone.
	const word stored{ static_cast<word>(ORG - 1) };
	for (std::size_t i{ 1 }; i <= 5; ++i) {
		const auto off{ klib::Asm6502::get_file_offset(15,
			static_cast<word>(TABLE + 2 * i)) };
		patched[off] = static_cast<byte>(stored & 0xff);
		patched[off + 1] = static_cast<byte>(stored >> 8);
	}
	rom = patched;
	// without permadoors no general hack space is used: the body lives where
	// the five handlers it replaces were. with it, the stub is the only cost.
	return next;
}
