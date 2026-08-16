#include "private_tests.h"
#include "hash.h"
#include "TestHelper.h"
#include "fe/Config.h"
#include "fe/script/ScriptManager.h"
#include "common/klib/Kfile.h"
#include <iostream>
#include <vector>

constexpr char TEST_DATA_FILE[]{ "test-hashes.txt" };

using byte = unsigned char;

void eoe::test::execute_tests(eoe::test::TestMode p_mode) {
	eoe::test::TestSuite suite(p_mode, TEST_DATA_FILE);

	const auto roms{ get_test_roms() };

	for (const auto& rom : roms) {
		std::cout << "Executing script tests for " << rom.string() << "\n";
		const auto rom_bytes{ klib::file::read_file_as_bytes(rom.string()) };
		execute_script_tests(suite, load_config(rom_bytes), rom_bytes);
	}

	suite.finalize();
}

void eoe::test::generate_tests(void) {
	execute_tests(TestMode::Generate);
}

void eoe::test::run_tests(void) {
	execute_tests(TestMode::Validate);
}
