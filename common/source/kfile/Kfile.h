#ifndef KFILE_H
#define KFILE_H

#include <string>
#include <vector>

using byte = unsigned char;

namespace kf {

	class Kfile {

	public:
		Kfile(void) = default;

		std::vector<byte> read_file_as_bytes(const std::string& p_filename) const;
	};

}

#endif
