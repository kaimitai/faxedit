#include "hash.h"
#include "common/klib/Kfile.h"

using byte = unsigned char;

std::string eoe::test::hash_bytes(const std::vector<byte>& p_bytes) {
	return picosha2::hash256_hex_string(p_bytes);
}

std::string eoe::test::hash_file(const std::string& p_filepath) {
	return hash_bytes(klib::file::read_file_as_bytes(p_filepath));
}

std::string eoe::test::hash_string(const std::string& p_string) {
	return picosha2::hash256_hex_string(p_string);
}

std::string eoe::test::hash_lines(const std::vector<std::string>& p_lines) {
	picosha2::hash256_one_by_one hasher;
	const std::string newline{ "\n" };

	hasher.init();

	for (const auto& line : p_lines) {
		hasher.process(line.begin(), line.end());
		hasher.process(begin(newline), end(newline));
	}

	hasher.finish();

	std::string result;
	picosha2::get_hash_hex_string(hasher, result);

	return result;
}

std::string eoe::test::hash_windows_string(const std::string& p_string) {
	std::string l_win_string;

	for (char c : p_string) {
		if (c == '\n')
			l_win_string.push_back('\r');
		l_win_string.push_back(c);
	}

	return picosha2::hash256_hex_string(l_win_string);
}
