#include "HackManager.h"
#include "fe/Config.h"
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
namespace {
	constexpr word ORG{ 0xeb51 }, END{ 0xeba1 };
	constexpr word TABLE{ 0xeb3f };
	constexpr word SELECTED{ 0x03c1 }, ITEM_COUNT{ 0x03c6 }, ITEM_ARRAY{ 0x03ad };
	constexpr word VANILLA_TAIL{ 0xebd1 };   // message, destroy $03c1, clear gate
	constexpr word UNLOCK_TAIL{ 0xebe1 };    // clear gate and sound only
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

	// the five key handlers, and the table entries that reach them
	void require_gate(const std::vector<byte>& rom) {
		if (rom.size() != ROM_SIZE)
			fail("expected a 256 KiB rom with a sixteen byte header");
		require_site(rom, 0xeb2f, { 0xad, 0x2b, 0x04, 0xf0, 0x0a, 0x0a, 0xa8 });
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

	void emit_body(klib::Asm6502& code) {
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
		// key did not come from there.
		code.jmp(UNLOCK_TAIL);

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

word fh::HackManager::install_AtlasDevSmartKeys(const fe::Config&,
	std::vector<byte>& rom, word cpu_addr, const fh::GeneralHack& hack) const {
	const std::string mode{ hack.string_or("mode", "carried") };
	if (mode != "carried" && mode != "vanilla")
		fail("mode must be carried or vanilla");
	if (mode == "vanilla") return cpu_addr;
	require_gate(rom);

	klib::Asm6502 code;
	emit_body(code);
	if (code.size() > END - ORG)
		fail(std::format("body is {} bytes and only {} fit in place of the key handlers",
			code.size(), END - ORG));

	// keep every write private until the whole install succeeds
	std::vector<byte> patched{ rom };
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
	// no general hack space is used: the body lives where the five handlers
	// it replaces were.
	return cpu_addr;
}
