#include "AtlasDevFaxOptions.h"
#include "HackManager.h"
#include "fh_constants.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// the fax options menu: a window over the pause screen with one on/off row
// per listed hack, then DEFAULTS. up and down move the cursor, with key
// repeat; left, right or a flip the row through the resident toggle, so a
// change applies at once. a on DEFAULTS turns every row back on. select, b
// or start close the window back to the pause screen.
//
// it runs main-line from the pause hook, in its own bank or in sram. the
// window is drawn and cleared by the game's routines in bank 12, reached
// through the vanilla far call.
//
// on close it gives back what it borrowed: the window geometry at
// $0208-$020c, the y of sprites 1 to 63 (sprite 0 is the status bar split),
// and this zero page:
//   $de-$e0 $e2-$e4  the far call keeps a, x and y in one of these, $de-$e0
//                    on the US ROM and $e2-$e4 on US rev A and EU; the menu's
//                    cursor, row and top are in the other
//   $e6 $e7          the menu's key repeat, and the visible rows left to draw
//   $e8-$eb          a line's ppu address and text position
//   $ec-$ef          the text pointer, and the far call's return and target
// the trampoline into a switchable bank saves and restores $0100 itself and
// uses $e5 as scratch. nothing else the menu calls, nor the nmi, uses $de-$e7.

namespace {
	constexpr byte WINDOW_BANK{ 12 };
	constexpr std::size_t WIDTH{ 26 }, LABEL_WIDTH{ 19 }, ACTION_WIDTH{ 22 };
	constexpr byte VISIBLE{ 10 };
	constexpr byte TEXT_X{ 3 }, TITLE_Y{ 8 }, ROW_Y{ 10 }, HELP_Y{ 21 }, CONTROLS_Y{ 23 };
	constexpr byte REPEAT_DELAY{ 24 }, REPEAT_INTERVAL{ 6 };

	// x, y, width and height of the window, then the closed flag
	constexpr std::array<word, 5> GEOMETRY{ fh::RAM::TextBox_X, fh::RAM::TextBox_Y, fh::RAM::TextBox_Width,
		fh::RAM::TextBox_Height, fh::RAM::TextBox_Closed };
	constexpr std::array<byte, 4> WINDOW{ 2, 6, 28, 22 };

	constexpr byte BUTTON_A{ 0x80 }, BUTTON_B{ 0x40 }, BUTTON_SELECT{ 0x20 }, BUTTON_START{ 0x10 };
	constexpr byte BUTTON_UP{ 0x08 }, BUTTON_DOWN{ 0x04 }, BUTTON_LEFT{ 0x02 }, BUTTON_RIGHT{ 0x01 };
	constexpr byte DIRECTIONS{ 0x0f };

	constexpr byte SPRITE_SIZE{ 4 }, HIDDEN_Y{ 0xf0 };

	// the font tiles
	constexpr byte SPACE{ 0x00 }, OPEN{ 0x0e }, CLOSE{ 0x0f }, COLON{ 0x3a }, LETTER_A{ 0x10 }, DIGIT_0{ 0x30 };
	// offsets of "OFF" and "ON " in the value texts
	constexpr byte VALUE_OFF{ 0 }, VALUE_ON{ 3 };

	// the menu's key repeat, and the visible rows left to draw; its cursor, row and
	// top are passed in, clear of the far call's
	constexpr byte REPEAT{ fh::RAM::ZP_e6 }, SLOT{ fh::RAM::ZP_e7 };
	// a line's text position and text pointer
	constexpr byte POS_X{ fh::RAM::ZP_ea }, POS_Y{ fh::RAM::ZP_eb };
	constexpr byte TEXT_LO{ fh::RAM::ZP_Temp_Int24_L }, TEXT_HI{ fh::RAM::ZP_Temp_Int24_M };

	constexpr std::array<byte, 16> SAVED_ZP{
		fh::RAM::ZP_BankedCallSavedA, fh::RAM::ZP_BankedCallSavedX, fh::RAM::ZP_BankedCallSavedY,
		fh::RAM::ZP_e2, fh::RAM::ZP_e3, fh::RAM::ZP_e4, REPEAT, SLOT,
		fh::RAM::ZP_e8, fh::RAM::ZP_e9, POS_X, POS_Y,
		TEXT_LO, TEXT_HI, fh::RAM::ZP_Temp_Int24_U, fh::RAM::ZP_ef };

	std::string pad(const std::string& p_text, std::size_t p_width) {
		if (p_text.size() > p_width)
			throw std::runtime_error(std::format("AtlasDevFaxOptions: {} does not fit {} characters", p_text, p_width));
		return p_text + std::string(p_width - p_text.size(), ' ');
	}

	std::string center(const std::string& p_text) {
		const std::size_t left{ (WIDTH - std::min(p_text.size(), WIDTH)) / 2 };
		return pad(std::string(left, ' ') + p_text, WIDTH);
	}

	void text(klib::Asm6502& p_code, const std::string& p_text) {
		for (const char c : p_text) {
			if (c >= 'A' && c <= 'Z')
				p_code.db(static_cast<byte>(LETTER_A + (c - 'A')));
			else if (c >= '0' && c <= '9')
				p_code.db(static_cast<byte>(DIGIT_0 + (c - '0')));
			else if (c == ' ')
				p_code.db(SPACE);
			else if (c == ':')
				p_code.db(COLON);
			else if (c == '[')
				p_code.db(OPEN);
			else if (c == ']')
				p_code.db(CLOSE);
			else
				throw std::runtime_error(std::format("AtlasDevFaxOptions: the menu font has no '{}'", c));
		}
	}
}

void fh::emit_faxoptions_menu(klib::Asm6502& p_code, const std::vector<FaxOptionsRow>& p_rows,
	const FaxOptionsResident& p_resident, byte p_state_zp, const fe::Config& p_config) {
	auto& code{ p_code };
	const byte cursor{ p_state_zp }, row{ static_cast<byte>(p_state_zp + 1) }, top{ static_cast<byte>(p_state_zp + 2) };
	const byte count{ static_cast<byte>(p_rows.size()) };
	// the hack rows, then DEFAULTS
	const byte row_total{ static_cast<byte>(count + 1) };
	const word far_call{ HackManager::cfg_word(p_config, c::ID_ROM_VANILLA_FAR_CALL) };
	const word bit_offset{ static_cast<word>(p_resident.rows_table + p_resident.bit_column) };
	const word bit_mask{ static_cast<word>(bit_offset + 1) };

	// the far call reads the bank and the target less one after its jsr
	const auto window_call = [&](word p_target) {
		code.jsr(far_call);
		code.db(WINDOW_BANK);
		code.dw(static_cast<word>(p_target - 1));
	};
	const auto draw = [&](const std::string& p_label, byte p_y) {
		code.lda_imm_lo(p_label);
		code.sta_zp(TEXT_LO);
		code.lda_imm_hi(p_label);
		code.sta_zp(TEXT_HI);
		code.lda_imm(TEXT_X);
		code.sta_zp(POS_X);
		code.lda_imm(p_y);
		code.sta_zp(POS_Y);
		code.jsr("@draw_line");
	};

	// open: save what the menu borrows, hide the sprites, draw the window
	code.label("@entry");
	for (const byte zp : SAVED_ZP) {
		code.lda_zp(zp);
		code.pha();
	}
	for (const word geometry : GEOMETRY) {
		code.lda_abs(geometry);
		code.pha();
	}
	code.ldx_imm(SPRITE_SIZE);
	code.label("@hide_sprites");
	code.lda_abs_x(RAM::SPRITE0_Y);
	code.pha();
	code.lda_imm(HIDDEN_Y);
	code.sta_abs_x(RAM::SPRITE0_Y);
	code.txa();
	code.clc();
	code.adc_imm(SPRITE_SIZE);
	code.tax();
	code.bne("@hide_sprites");
	code.jsr(ROM::WaitForNextFrame);
	code.lda_imm(0);
	code.sta_zp(cursor);
	code.sta_zp(top);
	code.lda_imm(REPEAT_DELAY);
	code.sta_zp(REPEAT);
	for (std::size_t i{ 0 }; i < WINDOW.size(); ++i) {
		code.lda_imm(WINDOW[i]);
		code.sta_abs(GEOMETRY[i]);
	}
	window_call(ROM::OpenWindowDraw);
	draw("@title", TITLE_Y);
	draw("@controls", CONTROLS_Y);
	code.jsr("@render_all");
	code.jsr("@render_help");
	code.jsr(ROM::PPUBuffer_WaitEmpty);

	// one frame per pass
	code.label("@poll");
	code.jsr(ROM::WaitForNextFrame);
	// every change is already applied, so these only close
	code.lda_zp(RAM::ZP_Joy1_ChangedButtonMask);
	code.and_imm(BUTTON_SELECT | BUTTON_B | BUTTON_START);
	code.beq("@poll_a");
	code.jmp("@close");
	code.label("@poll_a");
	code.lda_zp(RAM::ZP_Joy1_ChangedButtonMask);
	code.and_imm(BUTTON_A);
	code.bne("@activate");
	// exactly one direction held, or the repeat starts over
	code.lda_zp(RAM::ZP_Joy1_ButtonMask);
	code.and_imm(DIRECTIONS);
	for (const byte direction : { BUTTON_RIGHT, BUTTON_LEFT, BUTTON_DOWN, BUTTON_UP }) {
		code.cmp_imm(direction);
		code.beq("@direction_valid");
	}
	code.lda_imm(REPEAT_DELAY);
	code.sta_zp(REPEAT);
	code.jmp("@poll");
	// a fresh press acts now, a held one after the delay and then every interval
	code.label("@direction_valid");
	code.lda_zp(RAM::ZP_Joy1_ChangedButtonMask);
	code.and_imm(DIRECTIONS);
	code.bne("@direction_fresh");
	code.dec_zp(REPEAT);
	code.bne("@poll");
	code.lda_imm(REPEAT_INTERVAL);
	code.sta_zp(REPEAT);
	// only up and down repeat; a held left or right flips the row once
	code.lda_zp(RAM::ZP_Joy1_ButtonMask);
	code.and_imm(BUTTON_LEFT | BUTTON_RIGHT);
	code.bne("@poll");
	code.jmp("@direction_event");
	code.label("@direction_fresh");
	code.lda_imm(REPEAT_DELAY);
	code.sta_zp(REPEAT);
	code.label("@direction_event");
	code.lda_zp(RAM::ZP_Joy1_ButtonMask);
	code.and_imm(DIRECTIONS);
	code.cmp_imm(BUTTON_UP);
	code.beq("@previous");
	code.cmp_imm(BUTTON_DOWN);
	code.beq("@next");
	// left or right
	code.jmp("@switch");

	code.label("@activate");
	code.lda_imm(REPEAT_DELAY);
	code.sta_zp(REPEAT);
	code.lda_zp(cursor);
	code.cmp_imm(count);
	code.beq("@defaults");
	// a on a hack row flips it, as left and right do
	code.label("@switch");
	code.ldx_zp(cursor);
	code.cpx_imm(count);
	code.bcs("@poll");
	code.jsr(p_resident.toggle);
	code.jmp("@render_current");

	// up and down wrap round the list
	code.label("@previous");
	code.lda_zp(cursor);
	code.sta_zp(row);
	code.sec();
	code.sbc_imm(1);
	code.bpl("@previous_store");
	code.lda_imm(static_cast<byte>(row_total - 1));
	code.label("@previous_store");
	code.sta_zp(cursor);
	code.jsr("@scroll");
	code.bcs("@render_current_help");
	// the row that lost the cursor
	code.jsr("@render_row");
	code.jmp("@render_current");
	code.label("@next");
	code.lda_zp(cursor);
	code.sta_zp(row);
	code.clc();
	code.adc_imm(1);
	code.cmp_imm(row_total);
	code.bcc("@next_store");
	code.lda_imm(0);
	code.label("@next_store");
	code.sta_zp(cursor);
	code.jsr("@scroll");
	code.bcs("@render_current_help");
	code.jsr("@render_row");
	code.jmp("@render_current");

	code.label("@render_current");
	code.lda_zp(cursor);
	code.sta_zp(row);
	code.jsr("@render_row");
	code.label("@render_current_help");
	code.jsr("@render_help");
	code.jmp("@poll");

	// keep the cursor in the visible rows; c set: the list moved and was redrawn
	code.label("@scroll");
	code.lda_zp(cursor);
	code.cmp_zp(top);
	code.bcc("@scroll_up");
	code.lda_zp(top);
	code.clc();
	code.adc_imm(VISIBLE);
	code.cmp_zp(cursor);
	code.beq("@scroll_down");
	code.bcc("@scroll_down");
	code.clc();
	code.rts();
	code.label("@scroll_up");
	code.lda_zp(cursor);
	code.sta_zp(top);
	code.jmp("@scroll_redraw");
	code.label("@scroll_down");
	code.lda_zp(cursor);
	code.sec();
	code.sbc_imm(VISIBLE - 1);
	code.sta_zp(top);
	code.label("@scroll_redraw");
	code.jsr("@render_all");
	code.sec();
	code.rts();

	code.label("@defaults");
	code.jsr(p_resident.defaults);
	code.jsr("@render_all");
	code.jsr("@render_help");
	code.jmp("@poll");

	// close: clear the window and give back what was borrowed
	code.label("@close");
	window_call(ROM::WindowClose);
	code.jsr(ROM::PPUBuffer_WaitEmpty);
	code.ldx_imm(static_cast<byte>(0x100 - SPRITE_SIZE));
	code.label("@restore_sprites");
	code.pla();
	code.sta_abs_x(RAM::SPRITE0_Y);
	code.txa();
	code.sec();
	code.sbc_imm(SPRITE_SIZE);
	code.tax();
	code.bne("@restore_sprites");
	for (auto geometry{ GEOMETRY.rbegin() }; geometry != GEOMETRY.rend(); ++geometry) {
		code.pla();
		code.sta_abs(*geometry);
	}
	code.jsr(ROM::WaitForNextFrame);
	// the press that closed the menu does not reach the pause screen
	code.lda_imm(0);
	code.sta_zp(RAM::ZP_Joy1_ChangedButtonMask);
	for (auto zp{ SAVED_ZP.rbegin() }; zp != SAVED_ZP.rend(); ++zp) {
		code.pla();
		code.sta_zp(*zp);
	}
	code.rts();

	// the visible rows from the top one
	code.label("@render_all");
	code.lda_zp(top);
	code.sta_zp(row);
	code.lda_imm(VISIBLE);
	code.sta_zp(SLOT);
	code.label("@render_loop");
	code.jsr("@render_row");
	code.inc_zp(row);
	code.lda_zp(row);
	code.cmp_imm(row_total);
	code.bcs("@render_done");
	code.dec_zp(SLOT);
	code.bne("@render_loop");
	code.label("@render_done");
	code.rts();

	// one row: " label  value " or " [DEFAULTS] ", brackets on the cursor's
	code.label("@render_row");
	code.lda_zp(row);
	code.asl_a();
	code.tay();
	code.lda_abs_y("@labels");
	code.sta_zp(TEXT_LO);
	code.lda_abs_y("@labels", 1);
	code.sta_zp(TEXT_HI);
	// the row's bit picks its value text; kept on the stack while x is the queue cursor
	code.ldy_imm(VALUE_OFF);
	code.lda_zp(row);
	code.cmp_imm(count);
	code.bcs("@row_value");
	code.asl_a();
	code.asl_a();
	code.tax();
	code.lda_abs_x(bit_offset);
	code.tay();
	code.lda_abs_y(p_resident.state);
	code.ldy_imm(VALUE_OFF);
	code.and_abs_x(bit_mask);
	code.beq("@row_value");
	code.ldy_imm(VALUE_ON);
	code.label("@row_value");
	code.tya();
	code.pha();
	code.lda_zp(row);
	code.sec();
	code.sbc_zp(top);
	code.clc();
	code.adc_imm(ROW_Y);
	code.sta_zp(POS_Y);
	code.lda_imm(TEXT_X);
	code.sta_zp(POS_X);
	code.jsr("@line_header");
	code.lda_imm(SPACE);
	code.jsr(ROM::PPUQueuePayload);
	code.lda_zp(row);
	code.cmp_imm(count);
	code.bcs("@row_action");
	code.ldy_imm(0);
	code.label("@row_label");
	code.lda_ind_y(TEXT_LO);
	code.jsr(ROM::PPUQueuePayload);
	code.iny();
	code.cpy_imm(LABEL_WIDTH);
	code.bne("@row_label");
	code.lda_imm(SPACE);
	code.jsr(ROM::PPUQueuePayload);
	code.jsr("@row_open");
	code.pla();
	code.tay();
	for (int i{ 0 }; i < 3; ++i) {
		code.lda_abs_y("@values", i);
		code.jsr(ROM::PPUQueuePayload);
	}
	code.jsr("@row_close");
	code.stx_zp(RAM::ZP_PPUBufferWriteCursor);
	code.rts();
	code.label("@row_action");
	code.pla();
	code.jsr("@row_open");
	code.ldy_imm(0);
	code.label("@row_text");
	code.lda_ind_y(TEXT_LO);
	code.jsr(ROM::PPUQueuePayload);
	code.iny();
	code.cpy_imm(ACTION_WIDTH);
	code.bne("@row_text");
	code.jsr("@row_close");
	code.lda_imm(SPACE);
	code.jsr(ROM::PPUQueuePayload);
	code.stx_zp(RAM::ZP_PPUBufferWriteCursor);
	code.rts();
	// a bracket on the cursor's row, a space on the others
	code.label("@row_open");
	code.ldy_imm(OPEN);
	code.jmp("@row_mark");
	code.label("@row_close");
	code.ldy_imm(CLOSE);
	code.label("@row_mark");
	code.lda_zp(row);
	code.cmp_zp(cursor);
	code.beq("@row_mark_put");
	code.ldy_imm(SPACE);
	code.label("@row_mark_put");
	code.tya();
	code.jmp(ROM::PPUQueuePayload);

	// the help line, one text per row kind
	code.label("@render_help");
	code.lda_zp(cursor);
	code.cmp_imm(count);
	code.bcs("@help_action");
	code.lda_imm_lo("@help_switch");
	code.sta_zp(TEXT_LO);
	code.lda_imm_hi("@help_switch");
	code.sta_zp(TEXT_HI);
	code.jmp("@help_draw");
	code.label("@help_action");
	code.lda_imm_lo("@help_defaults");
	code.sta_zp(TEXT_LO);
	code.lda_imm_hi("@help_defaults");
	code.sta_zp(TEXT_HI);
	code.label("@help_draw");
	code.lda_imm(TEXT_X);
	code.sta_zp(POS_X);
	code.lda_imm(HELP_Y);
	code.sta_zp(POS_Y);

	// a whole line from the text pointer, at the text position
	code.label("@draw_line");
	code.jsr("@line_header");
	code.ldy_imm(0);
	code.label("@line_loop");
	code.lda_ind_y(TEXT_LO);
	code.jsr(ROM::PPUQueuePayload);
	code.iny();
	code.cpy_imm(WIDTH);
	code.bne("@line_loop");
	code.stx_zp(RAM::ZP_PPUBufferWriteCursor);
	code.rts();
	// x: the queue cursor, after the header of a line
	code.label("@line_header");
	code.jsr(ROM::PPUAddressFromPos);
	code.lda_imm(WIDTH);
	code.jmp(ROM::PPUQueueAppendHeader);

	code.label("@title");
	text(code, center("OPTIONS"));
	code.label("@controls");
	text(code, center("LR SET START OR B BACK"));
	code.label("@help_switch");
	text(code, center("A OR LEFT RIGHT TO SWITCH"));
	code.label("@help_defaults");
	text(code, center("TURN EVERY ROW BACK ON"));
	code.label("@values");
	text(code, "OFF");
	text(code, "ON ");
	code.label("@labels");
	for (std::size_t i{ 0 }; i < p_rows.size(); ++i)
		code.dw(std::format("@label_{}", i));
	code.dw("@label_defaults");
	for (std::size_t i{ 0 }; i < p_rows.size(); ++i) {
		code.label(std::format("@label_{}", i));
		text(code, pad(p_rows[i].label, LABEL_WIDTH));
	}
	code.label("@label_defaults");
	text(code, pad("DEFAULTS", ACTION_WIDTH));
}
