#ifndef KF_BITREADER_H
#define KF_BITREADER_H

#include <vector>

using byte = unsigned char;

namespace kf {

	class Bitreader {

		std::size_t m_byte_index, m_bit_index;

	public:
		Bitreader(std::size_t p_byte_index = 0, std::size_t p_bit_index = 0);

		unsigned int read_int(const std::vector<byte>& p_bytes, std::size_t p_bits);
	};

}

#endif
