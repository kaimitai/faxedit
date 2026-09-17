#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// screen transition: how a screen change looks, per direction. vanilla slides
// the next screen in when the player walks off the left or right side, 64
// frames with everything frozen, and blanks the screen and redraws it in one
// burst going up or down. this hack sets the choice for each direction, so a
// change can slide one way and blank another; a blank takes about 16 frames
// where a slide takes about 72.
//
// the main loop picks between the two at $db86: an area whose smooth flag
// ($042f) is clear always blanks, and otherwise the lda/cmp/bcc at $db8b
// decides by direction. this hack replaces the lda and cmp with a call to a
// subroutine in bank 15 that reads a four byte table with the direction in x
// and leaves carry set to blank, clear to slide. the bcc after it is untouched
// and still lands on the slide.
//
// the smooth flag is read first and nothing here changes it, so the table
// applies only in areas that allow smooth scrolling.
//
// up=scroll and down=scroll need AtlasDevVerticalScroll, which supplies the
// vertical slide; the vanilla slide assumes a horizontal move. list that hack
// first and this one calls its gate subroutine for those directions, so it
// needs no change. its state byte is read before the table, so a vertical
// change already under way keeps its slide whatever the table or the flag say.
//
// with a flag the table applies only while the flag is set; clear, the vanilla
// choice runs. the gate, the blank path and the redraw loop are checked
// against their vanilla bytes first, and they are identical in the us, us rev
// a, eu and jp roms. no ram is claimed.
namespace {
	constexpr byte BANK15{ 15 };
	constexpr word GATE{ 0xdb86 }, CALL{ 0xdb8b }, BRANCH{ 0xdb8f }, BLANK_PATH{ 0xdb91 },
		SLIDE{ 0xdbaf }, REDRAW{ 0xd2ce };
	constexpr byte DIRECTION{ 0x54 };
	constexpr word VS_STATE{ 0x04e2 };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };
	constexpr byte SCROLL{ 0 }, BLINK{ 1 };

	// lda $042f / beq $db91 / lda $54 / cmp #$02 / bcc $dbaf
	constexpr std::array<byte, 11> GATE_ORIG{ 0xad, 0x2f, 0x04, 0xf0, 0x06, 0xa5, 0x54, 0xc9, 0x02, 0x90, 0x1e };
	// the six bytes this hack owns, as the game ships them
	constexpr std::array<byte, 6> CALL_ORIG{ 0xa5, 0x54, 0xc9, 0x02, 0x90, 0x1e };
	// the blank path: the sprites, their graphics, the queue flush, the redraw, then back to the main loop
	constexpr std::array<byte, 30> PATH_ORIG{ 0x20, 0x47, 0xcb, 0x20, 0x25, 0xca, 0x20, 0x30, 0xc1, 0x20, 0xb4, 0xc1,
		0x20, 0x25, 0xca, 0x20, 0x8d, 0xc2, 0x20, 0xf7, 0xca, 0x20, 0x0f, 0xdd, 0x20, 0x17, 0xcb, 0x4c, 0x45, 0xdb };
	// the redraw loop: one step and one queue write at a time until the scroll wraps
	constexpr std::array<byte, 25> REDRAW_ORIG{ 0x20, 0x48, 0xe0, 0x20, 0xe7, 0xd2, 0x20, 0x1d, 0xd6, 0xa5, 0x54, 0xc9,
		0x02, 0xb0, 0x05, 0xa5, 0x0c, 0xd0, 0xed, 0x60, 0xa5, 0x57, 0xd0, 0xe8, 0x60 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK15, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	byte style_of(const std::string& p_name, const std::string& p_value) {
		std::string v{ p_value };
		std::transform(v.begin(), v.end(), v.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (v == "scroll") return SCROLL;
		if (v == "blink") return BLINK;
		throw std::runtime_error(std::format(
			"AtlasDevScreenTransition: {} must be scroll or blink", p_name));
	}

	// vanilla: left and right slide, up and down blank and redraw. a pair
	// shorthand sets both of its directions and an explicit direction wins.
	std::array<byte, 4> styles(const fh::GeneralHack& p_hack) {
		std::array<byte, 4> table{ SCROLL, SCROLL, BLINK, BLINK };
		const std::array<std::pair<const char*, std::array<std::size_t, 2>>, 2> pairs{ {
			{ "h", { 0, 1 } }, { "v", { 2, 3 } } } };
		for (const auto& [name, idx] : pairs)
			if (p_hack.has_param(name)) {
				const byte s{ style_of(name, p_hack.string_or(name, "")) };
				for (std::size_t i : idx) table[i] = s;
			}
		const std::array<const char*, 4> each{ "left", "right", "up", "down" };
		for (std::size_t i{ 0 }; i < each.size(); ++i)
			if (p_hack.has_param(each[i]))
				table[i] = style_of(each[i], p_hack.string_or(each[i], ""));
		return table;
	}

	// the six bytes at $db8b. stock means nothing else owns the gate. the
	// jsr/bcc/nop shape means a transition hack is already there and the
	// address it calls is what this one chains to. anything else is refused:
	// some other hack owns the site, and guessing at it would be worse than
	// stopping.
	std::optional<word> captured_gate(const std::vector<byte>& p_rom, const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK15, CALL) };
		std::array<byte, 6> six{};
		for (std::size_t i{ 0 }; i < six.size(); ++i) six[i] = p_rom[off + i];
		if (six == CALL_ORIG) return std::nullopt;
		if (six[0] == 0x20 && six[3] == 0x90
			&& six[4] == static_cast<byte>(SLIDE - (CALL + 5)) && six[5] == 0xea)
			return static_cast<word>(six[1] | (six[2] << 8));
		throw std::runtime_error(std::format(
			"{}: the gate at ${:04x} is neither stock nor a known transition hack: "
			"{:02x} {:02x} {:02x} {:02x} {:02x} {:02x}",
			p_name, CALL, six[0], six[1], six[2], six[3], six[4], six[5]));
	}
}

word fh::HackManager::install_AtlasDevScreenTransition(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevScreenTransition" };
	// every parameter is judged in every mode, vanilla included, so a typo is
	// caught even when the hack installs nothing
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	const std::array<byte, 4> table{ styles(p_hack) };
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = static_cast<int>(p_hack.word_or("flag", 0));
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	const std::optional<word> vgate{ captured_gate(p_rom, name) };
	if ((table[2] == SCROLL || table[3] == SCROLL) && !vgate.has_value())
		throw std::runtime_error(name + ": up=scroll and down=scroll need AtlasDevVerticalScroll "
			"listed before this hack");
	if (table[0] == SCROLL && table[1] == SCROLL && table[2] == BLINK && table[3] == BLINK)
		return cpu_addr;                       // the vanilla choice: nothing to install
	require_site(p_rom, BLANK_PATH, PATH_ORIG, name);
	require_site(p_rom, REDRAW, REDRAW_ORIG, name);
	if (!vgate.has_value())
		require_site(p_rom, GATE, GATE_ORIG, name);

	klib::Asm6502 code;
	if (vgate.has_value()) {
		// a vertical change already under way owns the choice, whatever the
		// table or the flag say: clearing the flag part way through one must
		// not strand it
		code.lda_abs(VS_STATE); code.bne("@to_vs");
	}
	if (flag >= 0) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3)));
		code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.beq("@vanilla");
	}
	code.ldx_zp(DIRECTION);
	code.lda_abs_x("@table");
	code.beq("@want_scroll");
	code.sec(); code.rts();
	code.label("@want_scroll");
	code.cpx_imm(0x02);
	code.bcc("@out_clc");
	code.label("@to_vs");
	if (vgate.has_value()) code.jmp(vgate.value());
	else { code.sec(); code.rts(); }
	code.label("@out_clc");
	code.clc(); code.rts();
	code.label("@vanilla");
	code.lda_zp(DIRECTION); code.cmp_imm(0x02); code.rts();
	code.label("@table");
	for (byte s : table) code.db(s);

	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	// the call site keeps the shape the gate already had: the bcc sits two
	// bytes up and still lands on the slide
	code.jsr(cpu_addr); code.bcc(static_cast<byte>(SLIDE - (CALL + 5))); code.nop();
	code.apply_hack_and_clear(p_rom, BANK15, CALL);
	return static_cast<word>(cpu_addr + size);
}
