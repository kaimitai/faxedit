#ifndef KLIB_KUTIL_H
#define KLIB_KUTIL_H

#include <vector>

namespace klib {
	namespace kutil {

		template<class T>
		std::vector<std::vector<T>> flat_vec_to_2d(const std::vector<T>& p_vec, std::size_t p_w) {
			std::vector<std::vector<T>> l_result;

			std::size_t i{ 0 };
			std::vector<T> l_tmp;

			for (const auto& t : p_vec) {
				l_tmp.push_back(t);

				++i;
				if (i == p_w) {
					l_result.push_back(l_tmp);
					l_tmp.clear();
					i = 0;
				}
			}

			return l_result;
		}

		template<class T>
			std::vector<T> flatten_2d_vec(const std::vector<std::vector<T>>& p_matrix) {

				std::vector<T> l_result;

				for (const auto& l_row : p_matrix)
					l_result.insert(end(l_result), begin(l_row), end(l_row));

				return l_result;

		}


	}
}

#endif
