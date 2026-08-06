#ifndef KLIB_BITREADER_H
#define KLIB_BITREADER_H

#include <string>
#include <vector>

using byte = unsigned char;

namespace klib {

	class Bitreader {

		std::size_t m_byte_index, m_bit_index;

	public:
		Bitreader(std::size_t p_byte_index = 0, std::size_t p_bit_index = 0);

		unsigned int read_int(const std::vector<byte>& p_bytes, std::size_t p_bits);

		static byte digit_to_hex(byte p_b);
		static std::string byte_to_hex(byte p_b);
	};

}

#endif
