#ifndef KLIB_KFILE_H
#define KLIB_KFILE_H

#include <string>
#include <vector>

using byte = unsigned char;

namespace klib {

	namespace file {

		std::vector<byte> read_file_as_bytes(const std::string& p_filename);

	}

}

#endif
