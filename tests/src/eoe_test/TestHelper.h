#ifndef EOE_TEST_HELPER_H
#define EOE_TEST_HELPER_H

#include <filesystem>
#include <set>
#include <vector>
#include "hash.h"
#include "TestSuite.h"
#include "fe/Config.h"

using byte = unsigned char;

namespace eoe::test {

	// loader helpers
	std::vector<byte> load_rom(const std::string& p_file_base_name);
	fe::Config load_config(const std::vector<byte>& p_rom,
		const std::string& p_override_xml = std::string(),
		const std::string& p_region_override = std::string());

	std::set<std::filesystem::path> get_test_roms(void);
	std::string get_rom_path(const std::string& p_file_base_name);
	std::string get_override_xml_path(const std::string& p_file_base_name);

	// functionality helpers
	std::string hash_iscript_disassembly(const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_shop_comments = true);
	std::string hash_bscript_disassembly(const fe::Config& p_config,
		const std::vector<byte>& p_rom);
	std::string hash_mscript_disassembly(const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_note_names);
	std::string hash_misc_extract(const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_all_sprites);
	std::string hash_mml_decompilation(const fe::Config& p_config,
		const std::vector<byte>& p_rom);

	void execute_script_tests(eoe::test::TestSuite& p_suite,
		const fe::Config& p_config, const std::vector<byte>& p_rom);

	// scripting tests
	void execute_iscript_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, const std::string& p_asm, bool p_shop_comments, bool p_strict);
	void execute_bscript_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, const std::string& p_asm, bool p_strict);
	void execute_mscript_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, const std::string& p_asm, bool p_note_names);
	void execute_misc_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, const std::string& p_txt, bool p_all_sprites);
	void execute_mml_round_trip_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, const std::string& p_mml);

	void execute_iscript_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_shop_comments, bool p_strict);
	void execute_bscript_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_strict);
	void execute_mscript_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_note_names);
	void execute_misc_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom, bool p_all_sprites);
	void execute_mml_tests(TestSuite& p_suite, const fe::Config& p_config,
		const std::vector<byte>& p_rom);

	// disasm helpers
	std::string disasm_iscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		bool p_shop_comments);
	std::string disasm_bscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom);
	std::string disasm_mscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		bool p_note_names);
	std::string extract_misc_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		bool p_all_sprites);
	std::string decompile_mml_layer(const fe::Config& p_config, const std::vector<byte>& p_rom);

	// asm helpers
	std::vector<byte> asm_iscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm, bool p_strict);
	std::vector<byte> asm_bscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm, bool p_strict);
	std::vector<byte> asm_mscript_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_asm);
	std::vector<byte> build_misc_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_txt);
	std::vector<byte> compile_mml_layer(const fe::Config& p_config, const std::vector<byte>& p_rom,
		const std::string& p_mml);
}

#endif
