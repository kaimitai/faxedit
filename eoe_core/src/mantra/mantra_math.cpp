#include "mantra_math.h"
#include <algorithm>
#include <stdexcept>

std::vector<int> fman::fm_math::str_to_bin(const std::string& s) const {
	std::vector<int> l_result;

	for (char c : s) {
		auto l_tmp{ dec_to_bin(char_to_dec(c)) };
		l_result.insert(end(l_result),
			begin(l_tmp), end(l_tmp));
	}

	return l_result;
}

int fman::fm_math::extract_value(const std::vector<int> b, std::size_t p_ind,
	std::size_t p_length) const {
	int l_result{ 0 };

	for (std::size_t i{ 0 }; i < p_length; ++i) {
		l_result *= 2;
		if (i + p_ind < b.size())
			l_result += b[i + p_ind];
	}

	return l_result;
}

std::vector<int> fman::fm_math::dec_to_bin(int d, int p_length) const {
	std::vector<int> l_result;

	for (int i = p_length - 1; i >= 0; --i) {
		l_result.push_back((d & (1 << i)) >> i);
	}

	return l_result;
}

std::vector<int> fman::fm_math::get_checksum_bin(const std::vector<int>& p_bin) const {
	int l_cs{ 0 };

	for (std::size_t i{ c::BITS_CHAR_COUNT }; i < p_bin.size(); i += c::BITS_CHECKSUM_LENGTH)
		l_cs += extract_value(p_bin, i, c::BITS_CHECKSUM_LENGTH);

	// take two's complement
	l_cs &= c::CHECKSUM_MASK;
	--l_cs;

	auto l_result{ dec_to_bin(l_cs, c::BITS_CHECKSUM_LENGTH) };
	for (int& n : l_result)
		n = (n == 0 ? 1 : 0);

	return l_result;
}

int fman::fm_math::get_checksum_dec(const std::vector<int>& p_bin) const {
	return extract_value(get_checksum_bin(p_bin), 0, c::BITS_CHECKSUM_LENGTH);
}

int fman::fm_math::char_to_dec(char c) const {
	if (c >= 'A' && c <= 'Z')
		return static_cast<int>(c - 'A');
	else if (c >= 'a' && c <= 'z')
		return static_cast<int>(c - 'a') + 26;
	else if (c >= '0' && c <= '9')
		return static_cast<int>(c - '0') + 52;
	else if (c == ',')
		return 62;
	else if (c == '?')
		return 63;
	else
		throw std::runtime_error(std::string("Invalid character \'") + c + '\'');
}

char fman::fm_math::bin_to_char(const std::vector<int>& p_bin, std::size_t p_idx) const {
	int l_val{ extract_value(p_bin, p_idx, c::BITS_PER_CHAR) };

	if (l_val < 26)
		return 'A' + l_val;
	else if (l_val < 52)
		return 'a' + l_val - 26;
	else if (l_val < 62)
		return '0' + l_val - 52;
	else if (l_val == 62)
		return ',';
	else
		return '?';
}

std::string fman::fm_math::bin_to_str(const std::vector<int>& p_bin) const {
	if (p_bin.size() % c::BITS_PER_CHAR != 0)
		throw std::runtime_error("Input bit count not a multiple of " +
			std::to_string(c::BITS_PER_CHAR));
	else {
		std::string l_result;

		for (std::size_t i{ 0 }; i < p_bin.size(); i += c::BITS_PER_CHAR)
			l_result.push_back(bin_to_char(p_bin, i));

		return l_result;
	}
}

int fman::fm_math::bits_needed(int p_count) const {
	if (p_count <= 1 || p_count > 256)
		return 1;

	int bits = 0;
	while ((static_cast<std::size_t>(1) << bits) < p_count)
		++bits;

	return bits;
}

void fman::fm_math::append_bin(std::vector<int>& b, int n, int p_length) const {
	std::vector<int> l_num{ dec_to_bin(n, p_length) };

	b.insert(end(b), begin(l_num), end(l_num));
}

void fman::fm_math::overwrite_bin(std::vector<int>& b, const std::vector<int>& p_val,
	std::size_t p_idx) const {
	for (std::size_t i{ 0 }; i < p_val.size(); ++i)
		b[p_idx + i] = p_val[i];
}
