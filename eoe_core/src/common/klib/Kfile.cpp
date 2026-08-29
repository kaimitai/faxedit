#include "Kfile.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
	struct DestinationMetadata {
		bool exists{ false };
		std::filesystem::perms permissions{ std::filesystem::perms::unknown };
	};

	DestinationMetadata inspect_destination(const std::filesystem::path& p_destination) {
		std::error_code ec;
		const auto status{ std::filesystem::symlink_status(p_destination, ec) };
		if (ec && status.type() != std::filesystem::file_type::not_found)
			throw std::runtime_error(
				"Could not inspect " + p_destination.string() + ": " + ec.message());

		if (!std::filesystem::exists(status))
			return {};
		if (std::filesystem::is_symlink(status))
			throw std::runtime_error(
				"Refusing to replace symbolic link: " + p_destination.string());
		if (!std::filesystem::is_regular_file(status))
			throw std::runtime_error(
				"Refusing to replace non-regular file: " + p_destination.string());

		return DestinationMetadata{ true, status.permissions() };
	}

	std::filesystem::path make_temporary_path(const std::filesystem::path& p_destination) {
		static std::atomic<unsigned long long> sequence{ 0 };
		const auto stamp{ static_cast<unsigned long long>(
			std::chrono::steady_clock::now().time_since_epoch().count()) };
		const auto parent{ p_destination.parent_path() };
		std::random_device random;
		const auto entropy{ (static_cast<unsigned long long>(random()) << 32) ^ random() };

		for (unsigned int attempt{ 0 }; attempt < 128; ++attempt) {
			const auto suffix{ stamp ^ entropy ^ sequence.fetch_add(1, std::memory_order_relaxed) };
			const auto candidate{ parent / (".eoe-tmp-" + std::to_string(suffix)) };
			std::error_code ec;
			const auto status{ std::filesystem::symlink_status(candidate, ec) };
			if ((!ec || status.type() == std::filesystem::file_type::not_found)
				&& !std::filesystem::exists(status))
				return candidate;
		}

		throw std::runtime_error("Could not allocate a temporary output beside " + p_destination.string());
	}

	void replace_file(const std::filesystem::path& p_temporary,
		const std::filesystem::path& p_destination, bool p_destination_exists) {
#ifdef _WIN32
		const bool replaced{ p_destination_exists
			? ReplaceFileW(p_destination.c_str(), p_temporary.c_str(), nullptr,
				REPLACEFILE_WRITE_THROUGH, nullptr, nullptr) != 0
			: MoveFileExW(p_temporary.c_str(), p_destination.c_str(),
				MOVEFILE_WRITE_THROUGH) != 0 };
		if (!replaced) {
			const std::error_code ec(static_cast<int>(GetLastError()), std::system_category());
			throw std::runtime_error("Could not replace " + p_destination.string() + ": " + ec.message());
		}
#else
		(void)p_destination_exists;
		std::error_code ec;
		std::filesystem::rename(p_temporary, p_destination, ec);
		if (ec)
			throw std::runtime_error("Could not replace " + p_destination.string() + ": " + ec.message());
#endif
	}

	template<typename Writer>
	void write_atomically(const std::filesystem::path& p_destination,
		std::ios::openmode p_mode, Writer p_writer) {
		const auto destination{ inspect_destination(p_destination) };
		const auto temporary{ make_temporary_path(p_destination) };

		try {
			std::ofstream file(temporary, p_mode | std::ios::trunc);
			if (!file)
				throw std::runtime_error("Failed to open temporary file for " + p_destination.string());

			p_writer(file);
			if (!file)
				throw std::runtime_error("Failed to write file: " + p_destination.string());

			file.flush();
			if (!file)
				throw std::runtime_error("Failed to flush file: " + p_destination.string());

			file.close();
			if (file.fail())
				throw std::runtime_error("Failed to close file: " + p_destination.string());

#ifndef _WIN32
			if (destination.exists) {
				std::error_code ec;
				std::filesystem::permissions(temporary, destination.permissions,
					std::filesystem::perm_options::replace, ec);
				if (ec)
					throw std::runtime_error(
						"Could not preserve permissions for " + p_destination.string()
						+ ": " + ec.message());
			}
#endif

			replace_file(temporary, p_destination, destination.exists);
		}
		catch (...) {
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			throw;
		}
	}

	void write_bytes(std::ofstream& p_file, const byte* p_data, std::size_t p_size) {
		constexpr auto max_chunk{ static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) };
		std::size_t offset{ 0 };

		while (offset < p_size) {
			const std::size_t size{ std::min(p_size - offset, max_chunk) };
			p_file.write(reinterpret_cast<const char*>(p_data + offset),
				static_cast<std::streamsize>(size));
			if (!p_file)
				return;
			offset += size;
		}
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
	write_atomically(p_filename, std::ios::binary,
		[&p_data](std::ofstream& p_file) {
			write_bytes(p_file, p_data.data(), p_data.size());
		});
}

void klib::file::write_string_to_file(const std::string& p_data, const std::string& p_filename) {
	write_atomically(p_filename, std::ios::out,
		[&p_data](std::ofstream& p_file) {
			write_bytes(p_file, reinterpret_cast<const byte*>(p_data.data()), p_data.size());
		});
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
