#include "Bitreader.h"

#include <stdexcept>

kf::Bitreader::Bitreader(std::size_t p_byte_index, std::size_t p_bit_index) :
	m_byte_index{ p_byte_index }, m_bit_index{ p_bit_index }
{ }

unsigned int kf::Bitreader::read_int(const std::vector<byte>& p_data,
	std::size_t p_bits) {

    if (p_bits == 0)
        return 0;
    else if (p_bits > 32)
        throw std::invalid_argument("Can only read up to 32 bits into an unsigned int");

    unsigned int value = 0;
    std::size_t bits_read = 0;

    while (bits_read < p_bits) {
        if (m_byte_index >= p_data.size())
            throw std::out_of_range("Read past end of data");

        unsigned char current_byte = p_data[m_byte_index];
        std::size_t bits_left_in_byte = 8 - m_bit_index;
        std::size_t bits_to_read = std::min(bits_left_in_byte, p_bits - bits_read);

        // Shift out unwanted high bits, then mask the bits we want
        unsigned char mask = static_cast<unsigned char>(
            ((1u << bits_to_read) - 1u) << (bits_left_in_byte - bits_to_read)
            );
        unsigned char extracted = static_cast<unsigned char>(
            (current_byte & mask) >> (bits_left_in_byte - bits_to_read)
            );

        // Shift the value left and append the new bits
        value = (value << bits_to_read) | extracted;

        // Update indices
        bits_read += bits_to_read;
        m_bit_index += bits_to_read;

        if (m_bit_index == 8) {
            m_bit_index = 0;
            ++m_byte_index;
        }
    }

    return value;
}
