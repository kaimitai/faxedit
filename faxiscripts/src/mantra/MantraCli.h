#ifndef FM_CLI_H
#define FM_CLI_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include "mantra/Mantra.h"

namespace fman {

	class MantraCli {

		std::map<std::string, std::vector<int>> m_arguments;
		std::string m_mantra;
		bool m_terse;
		int m_spawn_count;

		void validate_argument(int p_idx, int argc, char** argv) const;
		void parse_argument(int& p_idx, int argc, char** argv);

		std::vector<int> decode_param_values(const std::string& p_type_key, const std::vector<std::string>& p_values) const;
		std::vector<std::string> split_by_comma(const std::string& input) const;

		std::size_t get_index(const std::string& p_type_key, const std::string& p_needle) const;
		bool is_match(const std::string& p_needle, const std::string& p_str) const;
		std::vector<std::pair<std::size_t, std::string>> find_matches(const std::string& p_needle, const std::vector<std::string>& p_haystack) const;
		std::string decode_type_key(const std::string& p_type_key) const;
		bool is_valid_argument(const std::string& p_type_key) const;

		std::string join_with_commas(const std::vector<std::string>& p_strings) const;
		std::string join_with_commas(const std::vector<std::pair<std::size_t, std::string>>& p_pairs) const;

	public:
		MantraCli(int argc, char** argv);

		bool is_terse(void) const;
		fman::Mantra get_mantra(void) const;
	};

}

#endif
