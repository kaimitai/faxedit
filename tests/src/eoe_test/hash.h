#ifndef EOE_TEST_HASH_H
#define EOE_TEST_HASH_H

#include <string>
#include <vector>
#include "./../common/picosha2.h"

using byte = unsigned char;

namespace eoe::test {

	std::string hash_bytes(const std::vector<byte>& p_bytes);
	std::string hash_file(const std::string& p_filepath);
	std::string hash_string(const std::string& p_string);
	std::string hash_lines(const std::vector<std::string>& p_lines);

	// optional helper for validating strings vs hash of files on disk
	std::string hash_windows_string(const std::string& p_string);
}

#endif
