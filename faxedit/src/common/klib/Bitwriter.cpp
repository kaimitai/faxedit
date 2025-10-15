#include "Bitwriter.h"

klib::Bitwriter::Bitwriter(void) : m_bitIndex{ 0 }
{
}

void klib::Bitwriter::write_bits(byte p_value, std::size_t p_bit_count) {
	for (std::size_t i = 0; i < p_bit_count; ++i) {
		bool bit = (p_value >> (p_bit_count - 1 - i)) & 1;

		if (m_bitIndex == 0) {
			m_data.push_back(0);
		}

		m_data.back() |= static_cast<byte>(bit << (7 - m_bitIndex));
		m_bitIndex = (m_bitIndex + 1) % 8;
	}
}

const std::vector<byte>& klib::Bitwriter::get_data(void) const {
	return m_data;
}

void klib::Bitwriter::reset(void) {
	m_data.clear();
	m_bitIndex = 0;
}

std::size_t klib::Bitwriter::get_index(void) const {
	return m_bitIndex;
}
