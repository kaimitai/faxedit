#ifndef FM_MATH_H
#define FM_MATH_H

#include "mantra_constants.h"
#include <string>
#include <vector>

namespace fman {

	class fm_math {

	public:
		fm_math(void) = default;

		std::vector<int> str_to_bin(const std::string& s) const;
		int char_to_dec(char c) const;
		char bin_to_char(const std::vector<int>& p_bin, std::size_t p_idx) const;
		std::string bin_to_str(const std::vector<int>& p_bin) const;
		int bits_needed(int p_count) const;

		std::vector<int> dec_to_bin(int d, int p_length = c::BITS_PER_CHAR) const;
		std::vector<int> get_checksum_bin(const std::vector<int>& p_bin) const;
		int get_checksum_dec(const std::vector<int>& p_bin) const;
		int extract_value(const std::vector<int> b, std::size_t p_ind,
			std::size_t p_length) const;

		void append_bin(std::vector<int>& b, int n, int p_length) const;
		void overwrite_bin(std::vector<int>& b, const std::vector<int>& p_val,
			std::size_t p_idx) const;
	};

}

#endif
