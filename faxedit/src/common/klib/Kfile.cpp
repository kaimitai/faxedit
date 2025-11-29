#include "Kfile.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>

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

void klib::file::write_bytes_to_file(const std::vector<byte>& p_data, const std::string& p_filename) {
	std::ofstream file(p_filename, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Failed to open file: " + p_filename);
	}

	file.write(reinterpret_cast<const char*>(p_data.data()), p_data.size());
}

void klib::file::write_string_to_file(const std::string& p_data, const std::string& p_filename) {
	std::ofstream outputFile(p_filename);

	if (!outputFile.is_open())
		throw std::runtime_error("Failed to open file: " + p_filename);
	else
		outputFile << p_data;
}

void klib::file::create_folder_if_not_exists(const std::string& p_folder) {
	if (!std::filesystem::exists(p_folder)) {
		std::filesystem::create_directories(p_folder);
	}
}
