#include "fi/cli/ConfigPaths.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

	void require(bool p_condition, const std::string& p_message) {
		if (!p_condition)
			throw std::runtime_error(p_message);
	}

	void touch(const std::filesystem::path& p_path) {
		std::ofstream output(p_path);
		if (!output)
			throw std::runtime_error("Could not create test file " + p_path.string());
	}

}

int main(int argc, char** argv) try {
	const auto suffix{ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) };
	const auto root{ std::filesystem::temp_directory_path() / ("eoe-config-paths-" + suffix) };
	const auto executable_dir{ root / "application" };
	const auto project_dir{ root / "project" };
	std::filesystem::create_directories(executable_dir);
	std::filesystem::create_directories(project_dir);
	const auto resolved_executable_dir{ std::filesystem::weakly_canonical(executable_dir) };
	const auto resolved_project_dir{ std::filesystem::weakly_canonical(project_dir) };

	const auto application_override{ executable_dir / "eoe_config_override.xml" };
	const auto project_override{ project_dir / "eoe_config_override.xml" };
	touch(application_override);

	auto paths{ fi::paths::resolve_config_paths(executable_dir, project_dir) };
	require(paths.base == resolved_executable_dir / "eoe_config.xml",
		"Base configuration did not resolve relative to the executable");
	require(paths.override == resolved_executable_dir / "eoe_config_override.xml",
		"Application override was not used as the fallback");

	touch(project_override);
	paths = fi::paths::resolve_config_paths(executable_dir, project_dir);
	require(paths.override == resolved_project_dir / "eoe_config_override.xml",
		"Project-local override did not take precedence");

	const auto discovered_directory{ fi::paths::executable_directory(argc > 0 ? argv[0] : nullptr) };
	require(std::filesystem::is_directory(discovered_directory),
		"Running executable directory could not be discovered");

	std::filesystem::remove_all(root);
	return 0;
}
catch (const std::exception& ex) {
	std::cerr << ex.what() << '\n';
	return 1;
}
