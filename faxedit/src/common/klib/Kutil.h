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

	}
}

#endif
