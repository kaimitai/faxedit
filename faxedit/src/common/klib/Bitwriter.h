#ifndef KLIB_BITWRITER_H
#define KLIB_BITWRITER_H

#include <vector>

using byte = unsigned char;

namespace klib {

    class Bitwriter {
        std::vector<byte> m_data;
        std::size_t m_bitIndex;

    public:
        Bitwriter(void);
        void reset(void);
        void write_bits(byte p_value, std::size_t p_bit_count);
        std::size_t get_index(void) const;

        const std::vector<byte>& get_data(void) const;
    };

}

#endif

