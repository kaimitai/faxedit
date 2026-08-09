#ifndef FI_APPLICATION_CONSTANTS
#define FI_APPLICATION_CONSTANTS

#include <string>
#include <utility>
#include <vector>

namespace fi {

	namespace appc {

		constexpr char APP_NAME[]{ "FaxIScripts" };
		constexpr char APP_VERSION[]{ "0.91" };
		constexpr char APP_URL[]{ "https://github.com/kaimitai/FaxIScripts" };
		constexpr char CONFIG_XML[]{ "eoe_config.xml" };
		constexpr char CONFIG_OVERRIDE_FILE_NAME[]{ "eoe_config_override.xml" };

		inline const std::pair<std::string, std::string> CMD_EXTRACT{ "extract" , "x" };
		inline const std::pair<std::string, std::string> CMD_BUILD{ "build" , "b" };
		inline const std::pair<std::string, std::string> CMD_EXTRACT_MML{ "extract-mml" , "xmml" };
		inline const std::pair<std::string, std::string> CMD_BUILD_MML{ "build-mml" , "bmml" };
		inline const std::pair<std::string, std::string> CMD_MML_TO_MIDI{ "mml-to-midi" , "m2m" };
		inline const std::pair<std::string, std::string> CMD_ROM_TO_MIDI{ "rom-to-midi" , "r2m" };
		inline const std::pair<std::string, std::string> CMD_MML_TO_LILYPOND{ "mml-to-ly" , "m2l" };
		inline const std::pair<std::string, std::string> CMD_ROM_TO_LILYPOND{ "rom-to-ly" , "r2l" };
		inline const std::pair<std::string, std::string> CMD_EXTRACT_MUSIC{ "extract-music" , "xm" };
		inline const std::pair<std::string, std::string> CMD_BUILD_MUSIC{ "build-music" , "bm" };
		inline const std::pair<std::string, std::string> CMD_EXTRACT_BSCRIPTS{ "extract-bscript" , "xb" };
		inline const std::pair<std::string, std::string> CMD_BUILD_BSCRIPTS{ "build-bscript" , "bb" };
		inline const std::pair<std::string, std::string> CMD_EXTRACT_MISC{ "extract-misc" , "xmisc" };
		inline const std::pair<std::string, std::string> CMD_BUILD_MISC{ "build-misc" , "bmisc" };
		inline const std::pair<std::string, std::string> CMD_DUMP_CONFIG{ "dump-config" , "dc" };

		inline const std::vector<std::pair<std::string, std::string>> CLI_FLAGS{
			{"--no-shop-comments", "-p"},
			{"--original-size", "-o"},
			{"--force", "-f"},
			{"--no-notes", "-n"},
			{"--lilypond-percussion", "-lp"}
		};

		inline const std::pair<std::string, std::string> CLI_SOURCE_ROM
		{ "--source-rom", "-s" };

		inline const std::pair<std::string, std::string> CLI_REGION
		{ "--region", "-r" };

	}
}

#endif
