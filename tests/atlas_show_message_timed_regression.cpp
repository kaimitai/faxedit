#include "common/klib/Asm6502.h"
#include "common/klib/Kstring.h"
#include "fe/Config.h"
#include "fh/HackManager.h"
#include "fi/Opcode.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<byte>;
constexpr word ORIGIN{ 0xad00 };
constexpr std::size_t HANDLER_SIZE{ 84 };
constexpr std::size_t TABLE_SIZE{ 50 }; // 25 opcodes, two bytes each

void require(bool value, const std::string& message) {
	if (!value) throw std::runtime_error(message);
}

std::size_t offset(word cpu) {
	return klib::Asm6502::get_file_offset(12, cpu);
}

void put_word(Bytes& rom, word cpu, word value) {
	const auto at{ offset(cpu) };
	rom[at] = static_cast<byte>(value);
	rom[at + 1] = static_cast<byte>(value >> 8);
}

byte lo(word value) { return static_cast<byte>(value); }
byte hi(word value) { return static_cast<byte>(value >> 8); }

void test_region(const std::string& region, word load, word pump, word release,
	word messages_load, word show_next) {
	Bytes rom(0x40010, 0xff);
	put_word(rom, 0x8277, 0x9100);
	put_word(rom, 0x8273, 0x9120);

	const fe::Config config(EOE_TEST_CONFIG_PATH, "", rom, region);
	require(config.constant("rom_iscripts_loadbyte") == load, region + ": operand loader");
	require(config.constant("rom_iscripts_updateportraitanimation") == pump, region + ": portrait tick");
	require(config.constant("rom_text_releasepause") == release, region + ": pause release");
	require(config.constant("rom_messages_load") == messages_load, region + ": message loader");
	require(config.constant("rom_textbox_shownextchar") == show_next, region + ": glyph tick");

	const auto end{ fh::HackManager{}.apply_script_library(config, rom,
		offset(ORIGIN), { fh::HackLib::AtlasDevShowMessageTimed }, 24) };
	require(end == offset(ORIGIN) + HANDLER_SIZE + TABLE_SIZE, region + ": handler size");

	const word next{ static_cast<word>(config.constant("rom_iscripts_invokenextaction")) };
	const word reveal{ ORIGIN + 21 };
	const word hold{ ORIGIN + 57 };
	const Bytes handler{
		0x20, lo(load), hi(load), 0x48, // message id
		0x20, lo(load), hi(load), 0x48, // frame count
		0x20, lo(load), hi(load), 0x48, // skip mask
		0xba, 0xbd, 0x03, 0x01,          // message id
		0xf0, 0x1e,
		0x20, lo(messages_load), hi(messages_load),
		0x20, lo(pump), hi(pump),        // @reveal
		0x20, lo(show_next), hi(show_next),
		0xad, 0x15, 0x02,
		0xd0, 0x07,
		0xad, 0x13, 0x02,
		0xd0, 0xf0,
		0xf0, 0x09,
		0x20, lo(hold), hi(hold),        // @page
		0x20, lo(release), hi(release),
		0x4c, lo(reveal), hi(reveal),
		0x20, lo(hold), hi(hold),        // @finish
		0x68, 0x68, 0x68,
		0x4c, lo(next), hi(next),
		0xba, 0xbd, 0x04, 0x01,          // @hold: frames
		0xf0, 0x14,
		0x48,
		0x20, lo(pump), hi(pump),
		0xba, 0xbd, 0x04, 0x01,          // skip mask
		0x25, 0x19,
		0xd0, 0x07,
		0x68, 0x38, 0xe9, 0x01,
		0xd0, 0xee,
		0x60,
		0x68,
		0x60
	};
	require(handler.size() == HANDLER_SIZE, region + ": expected image size");
	require(std::equal(handler.begin(), handler.end(), rom.begin() + offset(ORIGIN)),
		region + ": handler");
}

void test_opcode() {
	Bytes rom(0x40010, 0xff);
	const fe::Config config(EOE_TEST_CONFIG_PATH, "", rom, "us");
	auto opcodes{ config.bmap("iscript_opcodes") };
	opcodes[0x18] = "Mnemonic=AtlasDevShowMessageTimed,Impl=AtlasDevShowMessageTimed";
	const auto info{ fi::load_iscript_opcodes_from_config(
		opcodes, config.str_map("iscript_opcode_impls")) };
	const auto& opcode{ info.opcodes.at(0x18) };
	require(info.required_impls == std::vector<std::string>{ "AtlasDevShowMessageTimed" },
		"opcode registration");
	require(opcode.args.size() == 3 && opcode.size() == 4 &&
		opcode.flow == fi::Flow::Continue && !opcode.ends_stream, "opcode shape");
	require(klib::str::parse_enum_ci<fh::HackLib>("AtlasDevShowMessageTimed") ==
		fh::HackLib::AtlasDevShowMessageTimed, "implementation name");
}
}

int main() {
	try {
		test_opcode();
		test_region("us", 0x87a4, 0x87b0, 0x9949, 0xf3f5, 0xf466);
		test_region("us-rev-a", 0x87a4, 0x87b0, 0x998d, 0xf3f5, 0xf466);
		test_region("eu", 0x87a4, 0x87b0, 0x998d, 0xf3f5, 0xf466);
		test_region("jp", 0x877c, 0x8788, 0x9d6b, 0xf505, 0xf55a);
		std::cout << "show message timed regression passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
