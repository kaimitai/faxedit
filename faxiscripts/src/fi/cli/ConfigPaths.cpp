#include "ConfigPaths.h"
#include "application_constants.h"
#include <cstdlib>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace {

	std::filesystem::path normalized_absolute(const std::filesystem::path& p_path) {
		std::error_code ec;
		auto result{ std::filesystem::absolute(p_path, ec) };
		if (ec)
			result = p_path;

		auto canonical{ std::filesystem::weakly_canonical(result, ec) };
		return ec ? result.lexically_normal() : canonical;
	}

	std::filesystem::path native_executable_path(void) {
#ifdef _WIN32
		std::vector<wchar_t> buffer(512);
		for (;;) {
			const DWORD length{ GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size())) };
			if (length == 0)
				return {};
			if (length < buffer.size() - 1)
				return std::filesystem::path(std::wstring(buffer.data(), length));
			buffer.resize(buffer.size() * 2);
		}
#elif defined(__APPLE__)
		uint32_t size{ 0 };
		_NSGetExecutablePath(nullptr, &size);
		if (size == 0)
			return {};

		std::vector<char> buffer(size);
		if (_NSGetExecutablePath(buffer.data(), &size) != 0)
			return {};
		return std::filesystem::path(buffer.data());
#elif defined(__linux__)
		std::error_code ec;
		auto result{ std::filesystem::read_symlink("/proc/self/exe", ec) };
		return ec ? std::filesystem::path{} : result;
#else
		return {};
#endif
	}

	std::filesystem::path find_invoked_executable(const char* p_argv0) {
		if (p_argv0 == nullptr || *p_argv0 == '\0')
			return {};

		const std::filesystem::path invoked{ p_argv0 };
		if (invoked.has_parent_path())
			return invoked;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
		const char* path_value{ std::getenv("PATH") };
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		if (path_value == nullptr)
			return invoked;

#ifdef _WIN32
		constexpr char path_separator{ ';' };
#else
		constexpr char path_separator{ ':' };
#endif

		const std::string path_list{ path_value };
		std::size_t begin{ 0 };
		for (;;) {
			const auto end{ path_list.find(path_separator, begin) };
			const auto entry{ path_list.substr(begin, end - begin) };
			const auto candidate{ (entry.empty() ? std::filesystem::path{"."} : std::filesystem::path{entry}) / invoked };
			std::error_code ec;
			if (std::filesystem::is_regular_file(candidate, ec))
				return candidate;
			if (end == std::string::npos)
				break;
			begin = end + 1;
		}

		return invoked;
	}

}

std::filesystem::path fi::paths::executable_directory(const char* p_argv0) {
	auto executable{ native_executable_path() };
	if (executable.empty())
		executable = find_invoked_executable(p_argv0);
	if (executable.empty()) {
		std::error_code ec;
		return std::filesystem::current_path(ec);
	}

	return normalized_absolute(executable).parent_path();
}

fi::paths::ConfigPaths fi::paths::resolve_config_paths(
	const std::filesystem::path& p_executable_directory,
	const std::filesystem::path& p_working_directory) {

	const auto executable_dir{ normalized_absolute(p_executable_directory) };
	const auto working_dir{ normalized_absolute(p_working_directory) };
	const auto application_override{ executable_dir / fi::appc::CONFIG_OVERRIDE_FILE_NAME };
	const auto project_override{ working_dir / fi::appc::CONFIG_OVERRIDE_FILE_NAME };

	std::error_code ec;
	const bool project_override_exists{ std::filesystem::is_regular_file(project_override, ec) };

	return {
		executable_dir / fi::appc::CONFIG_XML,
		project_override_exists ? project_override : application_override
	};
}

fi::paths::ConfigPaths fi::paths::resolve_config_paths(const char* p_argv0) {
	std::error_code ec;
	auto working_directory{ std::filesystem::current_path(ec) };
	if (ec)
		working_directory = ".";

	return resolve_config_paths(executable_directory(p_argv0), working_directory);
}
