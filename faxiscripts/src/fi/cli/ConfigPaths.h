#ifndef FI_CONFIG_PATHS_H
#define FI_CONFIG_PATHS_H

#include <filesystem>

namespace fi::paths {

	struct ConfigPaths {
		std::filesystem::path base;
		std::filesystem::path override;
	};

	// Resolve the directory containing the running executable. Native platform
	// APIs are used where available, with argv[0]/PATH as a portable fallback.
	std::filesystem::path executable_directory(const char* p_argv0);

	// The shipped base configuration always comes from the application directory.
	// A project-local override in the caller's working directory takes precedence
	// over an application-wide override beside the executable.
	ConfigPaths resolve_config_paths(
		const std::filesystem::path& p_executable_directory,
		const std::filesystem::path& p_working_directory);

	ConfigPaths resolve_config_paths(const char* p_argv0);

}

#endif
