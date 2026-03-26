#ifndef KLIB_KSTRING_H
#define KLIB_KSTRING_H

#include <map>
#include <string>
#include <vector>

using byte = unsigned char;

namespace klib {

	namespace str {

		std::string strip_comment(const std::string& line, char p_comment_char = ';');
		bool str_begins_with(const std::string& p_line, const std::string& p_start);
		bool str_equals_icase(const std::string& p_str_a, const std::string& p_str_b);
		std::vector<std::string> split_whitespace(const std::string& p_line);
		std::vector<std::string> split_string(const std::string& input, char delim);

		std::map<std::string, std::string> extract_keyval_str(const std::string& p_str,
			char p_super_delim = ',', char p_delim = '=');
		std::map<std::string, std::string> to_lowercase_string_map(const std::map<std::string, std::string>& p_map);

		std::string trim(const std::string& str);
		std::string to_lower(const std::string& str);
		std::string to_upper(const std::string& str);

		std::pair<std::string, std::string> parse_define(const std::string& str);
		std::string to_binary(byte b);

		int parse_numeric(const std::string& token);

		template<class T, class U>
		std::map<U, T> invert_map(const std::map<T, U>& p_map) {
			std::map<U, T> result;

			for (const auto& kv : p_map)
				result.insert(std::make_pair(kv.second, kv.first));

			return result;
		}
	}

}

#endif
