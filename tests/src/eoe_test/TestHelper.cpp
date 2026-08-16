#include "TestHelper.h"
#include <format>
#include "fe/script/ScriptManager.h"
#include "common/klib/Kfile.h"
#include "common/klib/Kstring.h"

namespace {
	// should be turned off, but can be useful for peace of mind on initial windows testing
	constexpr bool HASH_AS_WINDOWS_FILES{ false };
	constexpr char TEST_ROM_FOLDER[]{ "test_data/roms" };
}

using byte = unsigned char;

// loading helpers
std::vector<byte> eoe::test::load_rom(const std::string& p_file_base_name) {
	return klib::file::read_file_as_bytes(get_rom_path(p_file_base_name));
}

fe::Config eoe::test::load_config(const std::vector<byte>& p_rom,
	const std::string& p_override_xml,
	const std::string& p_region_override) {
	return fe::Config("eoe_config.xml", get_override_xml_path(p_override_xml), p_rom,
		p_region_override);
}

std::string eoe::test::get_override_xml_path(const std::string& p_file_base_name) {
	if (p_file_base_name.empty())
		return {};
	else
		return std::format("test_data/config_overrides/{}.xml", p_file_base_name);
}

std::string eoe::test::get_rom_path(const std::string& p_file_base_name) {
	return std::format("{}/{}.nes", TEST_ROM_FOLDER, p_file_base_name);
}

std::set<std::filesystem::path> eoe::test::get_test_roms(void) {
	std::set<std::filesystem::path> roms;

	for (const auto& entry : std::filesystem::directory_iterator(TEST_ROM_FOLDER)) {
		if (entry.is_regular_file() && entry.path().extension() == ".nes")
			roms.insert(entry.path());
	}

	return roms;
}


// functionality helpers
std::string eoe::test::hash_iscript_disassembly(const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_shop_comments) {
	std::size_t dummy{ 0 };
	const auto asm_file{ disasm_iscript_layer(p_config, p_rom, p_shop_comments) };

	if constexpr (HASH_AS_WINDOWS_FILES) {
		return hash_windows_string(asm_file);
	}
	else {
		return hash_string(asm_file);
	}
}

std::string eoe::test::hash_bscript_disassembly(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	const auto asm_file{ disasm_bscript_layer(p_config, p_rom) };

	if constexpr (HASH_AS_WINDOWS_FILES) {
		return hash_windows_string(asm_file);
	}
	else {
		return hash_string(asm_file);
	}
}

std::string eoe::test::hash_mscript_disassembly(const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_note_names) {
	const auto asm_file{ disasm_mscript_layer(p_config, p_rom, p_note_names) };

	if constexpr (HASH_AS_WINDOWS_FILES) {
		return hash_windows_string(asm_file);
	}
	else {
		return hash_string(asm_file);
	}
}

std::string eoe::test::hash_misc_extract(const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_all_sprites) {
	const auto txt_file{ extract_misc_layer(p_config, p_rom, p_all_sprites) };

	if constexpr (HASH_AS_WINDOWS_FILES) {
		return hash_windows_string(txt_file);
	}
	else {
		return hash_string(txt_file);
	}
}

std::string eoe::test::hash_mml_decompilation(const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	const auto mml_file{ decompile_mml_layer(p_config, p_rom) };

	if constexpr (HASH_AS_WINDOWS_FILES) {
		return hash_windows_string(mml_file);
	}
	else {
		return hash_string(mml_file);
	}
}

void eoe::test::execute_script_tests(eoe::test::TestSuite& p_suite,
	const fe::Config& p_config, const std::vector<byte>& p_rom) {
	execute_iscript_tests(p_suite, p_config, p_rom, true, false);
	execute_iscript_tests(p_suite, p_config, p_rom, false, false);
	execute_bscript_tests(p_suite, p_config, p_rom, false);
	execute_mscript_tests(p_suite, p_config, p_rom, false);
	execute_mscript_tests(p_suite, p_config, p_rom, true);
	execute_misc_tests(p_suite, p_config, p_rom, false);
	execute_misc_tests(p_suite, p_config, p_rom, true);
	execute_mml_tests(p_suite, p_config, p_rom);
}

// script tests

// general round-trip tests
void eoe::test::execute_iscript_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, const std::string& p_asm, bool p_shop_comments, bool p_strict) {
	// asm to rom and store hash
	const auto rom1{ asm_iscript_layer(p_config, p_rom, p_asm, p_strict) };
	p_suite.hash(hash_bytes(rom1));

	// disasm and assemble again
	const auto asm2{ disasm_iscript_layer(p_config, rom1, p_shop_comments) };
	p_suite.hash(hash_string(asm2));
	const auto rom2{ asm_iscript_layer(p_config, p_rom, asm2, p_strict) };

	// disasm again to verify that the assembled representation has stabilized
	const auto asm3{ disasm_iscript_layer(p_config, rom2, p_shop_comments) };

	if (asm2 != asm3)
		throw std::runtime_error("iScript ASM round-trip mismatch");
	if (rom1 != rom2)
		throw std::runtime_error("iScript ROM round-trip mismatch");
}

void eoe::test::execute_bscript_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, const std::string& p_asm, bool p_strict) {
	// asm to rom and store hash
	const auto rom1{ asm_bscript_layer(p_config, p_rom, p_asm, p_strict) };
	p_suite.hash(hash_bytes(rom1));

	// disasm and assemble again
	const auto asm2{ disasm_bscript_layer(p_config, rom1) };
	p_suite.hash(hash_string(asm2));
	const auto rom2{ asm_bscript_layer(p_config, p_rom, asm2, p_strict) };

	// disasm again to verify that the assembled representation has stabilized
	const auto asm3{ disasm_bscript_layer(p_config, rom2) };

	if (asm2 != asm3)
		throw std::runtime_error("bScript ASM round-trip mismatch");
	if (rom1 != rom2)
		throw std::runtime_error("bScript ROM round-trip mismatch");
}

void eoe::test::execute_mscript_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, const std::string& p_asm, bool p_note_names) {
	// asm to rom and store hash
	const auto rom1{ asm_mscript_layer(p_config, p_rom, p_asm) };
	p_suite.hash(hash_bytes(rom1));

	// disasm and assemble again
	const auto asm2{ disasm_mscript_layer(p_config, rom1, p_note_names) };
	p_suite.hash(hash_string(asm2));
	const auto rom2{ asm_mscript_layer(p_config, p_rom, asm2) };

	// disasm again to verify that the assembled representation has stabilized
	const auto asm3{ disasm_mscript_layer(p_config, rom2, p_note_names) };

	if (asm2 != asm3)
		throw std::runtime_error("mScript ASM round-trip mismatch");
	if (rom1 != rom2)
		throw std::runtime_error("mScript ROM round-trip mismatch");
}

void eoe::test::execute_misc_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, const std::string& p_asm, bool p_all_sprites) {
	// txt to rom and store hash
	const auto rom1{ build_misc_layer(p_config, p_rom, p_asm) };
	p_suite.hash(hash_bytes(rom1));

	// extract and build again
	const auto txt2{ extract_misc_layer(p_config, rom1, p_all_sprites) };
	p_suite.hash(hash_string(txt2));
	const auto rom2{ build_misc_layer(p_config, p_rom, txt2) };

	// extract again to verify that the representation has stabilized
	const auto txt3{ extract_misc_layer(p_config, rom2, p_all_sprites) };

	if (txt2 != txt3)
		throw std::runtime_error("Misc data round-trip mismatch");
	if (rom1 != rom2)
		throw std::runtime_error("Misc data round-trip mismatch");
}

void eoe::test::execute_mml_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, const std::string& p_mml) {
	// mml to rom and store hash
	const auto rom1{ compile_mml_layer(p_config, p_rom, p_mml) };
	p_suite.hash(hash_bytes(rom1));

	// extract and build again
	const auto mml2{ decompile_mml_layer(p_config, rom1) };
	p_suite.hash(hash_string(mml2));
	const auto rom2{ compile_mml_layer(p_config, p_rom, mml2) };

	// extract again to verify that the representation has stabilized
	const auto mml3{ decompile_mml_layer(p_config, rom2) };

	if (mml2 != mml3)
		throw std::runtime_error("MML round-trip mismatch");
	if (rom1 != rom2)
		throw std::runtime_error("MML round-trip mismatch");
}

// rom-derived round-trip tests
void eoe::test::execute_iscript_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_shop_comments, bool p_strict) {
	// disasm layer and store hash
	const auto asm1{ disasm_iscript_layer(p_config, p_rom, p_shop_comments) };
	p_suite.hash(hash_string(asm1));
	// execute the round-trip test for this data
	execute_iscript_round_trip_tests(p_suite, p_config, p_rom, asm1, p_shop_comments, p_strict);
}

void eoe::test::execute_bscript_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_strict) {
	// disasm layer and store hash
	const auto asm1{ disasm_bscript_layer(p_config, p_rom) };
	p_suite.hash(hash_string(asm1));
	// execute the round-trip test for this data
	execute_bscript_round_trip_tests(p_suite, p_config, p_rom, asm1, p_strict);
}

void eoe::test::execute_mscript_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_note_names) {
	// disasm layer and store hash
	const auto asm1{ disasm_mscript_layer(p_config, p_rom, p_note_names) };
	p_suite.hash(hash_string(asm1));
	// execute the round-trip test for this data
	execute_mscript_round_trip_tests(p_suite, p_config, p_rom, asm1, p_note_names);
}

void eoe::test::execute_misc_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom, bool p_all_sprites) {
	// extract data and store hash
	const auto txt1{ extract_misc_layer(p_config, p_rom, p_all_sprites) };
	p_suite.hash(hash_string(txt1));
	// execute the round-trip test for this data
	execute_misc_round_trip_tests(p_suite, p_config, p_rom, txt1, p_all_sprites);
}

void eoe::test::execute_mml_tests(TestSuite& p_suite, const fe::Config& p_config,
	const std::vector<byte>& p_rom) {
	// decompile mml and store hash
	const auto mml1{ decompile_mml_layer(p_config, p_rom) };
	p_suite.hash(hash_string(mml1));
	// execute the round-trip test for this data
	execute_mml_round_trip_tests(p_suite, p_config, p_rom, mml1);
}

// disasm helpers
std::string eoe::test::disasm_iscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	bool p_shop_comments) {
	std::size_t dummy{ 0 };
	return fe::script::disasm_iscripts(p_config, p_rom,
		fe::script::get_iscript_opcode_info(p_config).opcodes, p_shop_comments, dummy);
}

std::string eoe::test::disasm_bscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	return fe::script::disasm_bscripts(p_config, p_rom);
}

std::string eoe::test::disasm_mscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	bool p_note_names) {
	return fe::script::disasm_mscripts(p_config, p_rom, p_note_names, nullptr);
}

std::string eoe::test::extract_misc_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	bool p_all_sprites) {
	return fe::script::extract_misc(p_config, p_rom, p_all_sprites);
}

std::string eoe::test::decompile_mml_layer(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	return fe::script::decompile_mml(p_config, p_rom);
}

// asm helpers
std::vector<byte> eoe::test::asm_iscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_asm, bool p_strict) {
	return fe::script::asm_iscripts(p_config, p_rom, klib::str::split_string(p_asm, '\n'),
		fe::script::get_iscript_opcode_info(p_config), p_strict, nullptr);
}

std::vector<byte> eoe::test::asm_bscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_asm, bool p_strict) {
	return fe::script::asm_bscripts(p_config, p_rom, klib::str::split_string(p_asm, '\n'), p_strict, nullptr);
}

std::vector<byte> eoe::test::asm_mscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_asm) {
	return fe::script::asm_mscripts(p_config, p_rom, klib::str::split_string(p_asm, '\n'), nullptr);
}

std::vector<byte> eoe::test::build_misc_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_txt) {
	return fe::script::build_misc(p_config, p_rom, klib::str::split_string(p_txt, '\n'), nullptr);
}

std::vector<byte> eoe::test::compile_mml_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
	const std::string& p_mml) {
	return fe::script::compile_mml(p_config, p_rom, klib::str::split_string(p_mml, '\n'), nullptr);
}
