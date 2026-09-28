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

void require(bool value, const std::string& message) {
	if (!value) throw std::runtime_error(message);
}

std::size_t offset(word cpu) {
	return klib::Asm6502::get_file_offset(12, cpu);
}

word read_word(const Bytes& rom, word cpu) {
	const auto at{ offset(cpu) };
	return static_cast<word>(rom.at(at) | rom.at(at + 1) << 8);
}

void put_word(Bytes& rom, word cpu, word value) {
	const auto at{ offset(cpu) };
	rom[at] = static_cast<byte>(value);
	rom[at + 1] = static_cast<byte>(value >> 8);
}

void test_region(const std::string& region, word menu) {
	Bytes source(0x40010, 0xff);
	put_word(source, 0x8277, 0x9100);
	put_word(source, 0x8273, 0x9120);
	for (unsigned i{}; i < 24; ++i) {
		source[offset(0x9100) + i] = static_cast<byte>(0x40 + i);
		source[offset(0x9120) + i] = 0x87;
	}

	const fe::Config config(EOE_TEST_CONFIG_PATH, "", source, region);
	require(config.constant("rom_player_menu_show") == menu,
		region + ": menu entry");

	auto rom{ source };
	const auto end{ fh::HackManager{}.apply_script_library(config, rom,
		offset(ORIGIN), { fh::HackLib::AtlasDevShowInventoryMenu }, 24) };
	require(end == offset(ORIGIN) + 83, region + ": library size");

	const word next{
		static_cast<word>(config.constant("rom_iscripts_invokenextaction"))
	};
	const Bytes handler{
		0x20,0xfb,0x81,0xad,1,2,0x48,0xa9,0,0x8d,1,2,
		0x20,0x2b,0x82,0x20,byte(menu),byte(menu >> 8),0x68,0x8d,1,2,
		0x10,3,0x20,0x1f,0x82,0x20,0xe2,0x81,0x4c,byte(next),byte(next >> 8)
	};
	require(std::equal(handler.begin(), handler.end(),
		rom.begin() + offset(ORIGIN)), region + ": handler");

	const word low{ read_word(rom, 0x8277) };
	const word high{ read_word(rom, 0x8273) };
	require(low == ORIGIN + 33 && high == ORIGIN + 58,
		region + ": dispatch tables");
	require(1 + rom[offset(low) + 24] +
		(rom[offset(high) + 24] << 8) == ORIGIN,
		region + ": dispatch entry");
}

void test_opcode() {
	Bytes rom(0x40010, 0xff);
	const fe::Config config(EOE_TEST_CONFIG_PATH, "", rom, "us");
	auto opcodes{ config.bmap("iscript_opcodes") };
	opcodes[0x18] =
		"Mnemonic=AtlasDevShowInventoryMenu,Impl=AtlasDevShowInventoryMenu";
	const auto info{ fi::load_iscript_opcodes_from_config(
		opcodes, config.str_map("iscript_opcode_impls")) };
	const auto& opcode{ info.opcodes.at(0x18) };
	require(info.base_opcode_count == 24 &&
		info.required_impls == std::vector<std::string>{
			"AtlasDevShowInventoryMenu" }, "opcode registration");
	require(opcode.args.empty() && opcode.size() == 1 &&
		opcode.flow == fi::Flow::Continue && !opcode.ends_stream, "opcode shape");
	require(klib::str::parse_enum_ci<fh::HackLib>(
		"AtlasDevShowInventoryMenu") == fh::HackLib::AtlasDevShowInventoryMenu,
		"implementation name");
}
}

int main() {
	try {
		test_opcode();
		test_region("us", 0x8a93);
		test_region("us-rev-a", 0x8a93);
		test_region("eu", 0x8a93);
		test_region("jp", 0x8a6b);
		std::cout << "inventory menu regression passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
