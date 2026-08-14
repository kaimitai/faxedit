#include "Kfile.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace {

	void write_checked(std::ofstream& p_file, const char* p_data,
		std::size_t p_size, const std::string& p_filename) {
		if (p_size > static_cast<std::size_t>(
				std::numeric_limits<std::streamsize>::max()))
			throw std::runtime_error("File is too large to write: " + p_filename);
		p_file.write(p_data, static_cast<std::streamsize>(p_size));
		p_file.flush();
		if (!p_file)
			throw std::runtime_error("Failed to write file: " + p_filename);
		p_file.close();
		if (p_file.fail())
			throw std::runtime_error("Failed to close file: " + p_filename);
	}

	void replace_file(const std::filesystem::path& p_source,
		const std::filesystem::path& p_target) {
#ifdef _WIN32
		if (!MoveFileExW(p_source.c_str(), p_target.c_str(),
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			throw std::runtime_error("Failed to replace file: "
				+ p_target.string() + " (" + std::system_category().message(
					static_cast<int>(GetLastError())) + ")");
#else
		std::error_code error;
		std::filesystem::rename(p_source, p_target, error);
		if (error)
			throw std::runtime_error("Failed to replace file: "
				+ p_target.string() + " (" + error.message() + ")");
#endif
	}

}

std::vector<byte> klib::file::read_file_as_bytes(const std::string& p_filename) {
	std::ifstream file(p_filename, std::ios::binary);
	if (!file)
		throw std::runtime_error("Failed to open file: " + p_filename);

	// Seek to end to determine size
	file.seekg(0, std::ios::end);
	std::streamsize size = file.tellg();
	if (size < 0)
		throw std::runtime_error("Failed to determine file size: " + p_filename);
	file.seekg(0, std::ios::beg);

	std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		throw std::runtime_error("Failed to read file: " + p_filename);

	return buffer;
}

std::vector<std::string> klib::file::read_file_as_strings(const std::string& p_filename) {
	std::vector<std::string> result;
	std::ifstream input_file(p_filename);

	if (input_file.is_open()) {
		std::string line;
		while (std::getline(input_file, line)) {
			result.push_back(line);
		}
		input_file.close();
	}
	else
		throw std::runtime_error("Failed to read file: " + p_filename);
	return result;
}

bool klib::file::file_exists(const std::string& p_filename) {
	std::ifstream file(p_filename);
	return file.good();
}

void klib::file::write_bytes_to_file(const std::vector<byte>& p_data, const std::string& p_filename) {
	std::ofstream file(p_filename, std::ios::binary);
	if (!file)
		throw std::runtime_error("Failed to open file: " + p_filename);
	write_checked(file, reinterpret_cast<const char*>(p_data.data()),
		p_data.size(), p_filename);
}

void klib::file::write_bytes_to_file_atomic(const std::vector<byte>& p_data,
	const std::string& p_filename) {
	const std::filesystem::path target{ p_filename };
	auto temporary{ target };
	temporary += ".tmp";
	bool temporary_opened{ false };
	try {
		std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
		if (!file)
			throw std::runtime_error("Failed to open temporary file: "
				+ temporary.string());
		temporary_opened = true;
		write_checked(file, reinterpret_cast<const char*>(p_data.data()),
			p_data.size(), temporary.string());
		replace_file(temporary, target);
	}
	catch (...) {
		if (temporary_opened) {
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
		}
		throw;
	}
}

void klib::file::write_string_to_file(const std::string& p_data, const std::string& p_filename) {
	std::ofstream file(p_filename);
	if (!file)
		throw std::runtime_error("Failed to open file: " + p_filename);
	write_checked(file, p_data.data(), p_data.size(), p_filename);
}

void klib::file::create_directories(const std::string& p_dir) {
	klib::file::create_directories(std::filesystem::path{ p_dir });
}

void klib::file::create_directories(const std::filesystem::path& p_dir) {
	try {
		std::filesystem::create_directories(p_dir);
	}
	catch (const std::filesystem::filesystem_error& e) {
		throw std::runtime_error(e.what());
	}
}
