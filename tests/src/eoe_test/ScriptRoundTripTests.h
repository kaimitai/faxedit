#ifndef EOE_TEST_SCRIPTROUNDTRIPTEST_H
#define EOE_TEST_SCRIPTROUNDTRIPTEST_H

#include <filesystem>
#include <set>
#include <string>
#include <vector>
#include "fe/script/ScriptManager.h"
#include "fe/Config.h"

using byte = unsigned char;

namespace eoe_test {

	struct TestResult { int success, failure; };

	TestResult RoundTripTest(void);

	bool TestiScriptRoundTrip(const fe::Config& p_config, const std::vector<byte>& p_rom);

	fe::Config get_config(const std::vector<byte>& p_rom, const std::string& p_override_xml_filename);
	std::set<std::filesystem::path> get_test_roms(void);

}

#endif
