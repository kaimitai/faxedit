#include <filesystem>
#include <set>
#include "ScriptRoundTripTests.h"
#include "common/klib/Kfile.h"
#include "common/klib/Kstring.h"
#include "fi/fi_constants.h"

eoe_test::TestResult eoe_test::RoundTripTest(void) {
	int success{ 0 },
		failure{ 0 };

	const auto roms{ get_test_roms() };

	for (const auto& rom_file : roms) {
		const auto rom_data{ klib::file::read_file_as_bytes(rom_file.string()) };

		auto config{ get_config(rom_data,
			"./test_data/eoe_config_override.xml") };

		if (TestiScriptRoundTrip(config, rom_data))
			++success;
		else
			++failure;
	}

	return eoe_test::TestResult{
		.success = success,
		.failure = failure
	};
}

bool eoe_test::TestiScriptRoundTrip(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	const auto impls{ fi::load_iscript_opcodes_from_config(p_config.bmap_dense(fi::c::ID_ISCRIPT_OPCODES),
		p_config.str_map(fi::c::ID_ISCRIPT_OPCODE_IMPLS)) };

	std::size_t ep_count{ 0 }, ep_count_2{ 0 }, ep_count_3{ 0 };
	const auto asm_text{ fe::script::disasm_iscripts(p_config, p_rom, false, ep_count) };
	const auto asm_lines{ klib::str::split_string(asm_text, '\n') };
	const auto new_rom{ fe::script::asm_iscripts(p_config, p_rom, asm_lines, impls, false, nullptr) };

	const auto asm_text_2{ fe::script::disasm_iscripts(p_config, new_rom, false, ep_count_2) };
	const auto asm_lines_2{ klib::str::split_string(asm_text_2, '\n') };
	const auto new_rom_2{ fe::script::asm_iscripts(p_config, p_rom, asm_lines_2, impls, false, nullptr) };

	const auto asm_text_3{ fe::script::disasm_iscripts(p_config, new_rom_2, false, ep_count_3) };

	return new_rom == new_rom_2 &&
		asm_text_2 == asm_text_3 &&
		ep_count == ep_count_2 &&
		ep_count_2 == ep_count_3;
}

fe::Config eoe_test::get_config(const std::vector<byte>& p_rom, const std::string& p_override_xml_filename) {
	fe::Config config;
	config.load_definitions("eoe_config.xml", p_override_xml_filename);
	config.determine_region(p_rom);
	config.load_config_data("eoe_config.xml", p_override_xml_filename, p_rom);
	return config;
}

std::set<std::filesystem::path> eoe_test::get_test_roms(void) {
	std::set<std::filesystem::path> roms;

	for (const auto& entry : std::filesystem::directory_iterator("test_data/roms")) {
		if (entry.is_regular_file() && entry.path().extension() == ".nes")
			roms.insert(entry.path());
	}

	return roms;
}
