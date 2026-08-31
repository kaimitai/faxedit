#ifndef KLIB_IMAGE_H
#define KLIB_IMAGE_H

#include <vector>

using byte = unsigned char;

namespace klib {

	struct RGB {
		byte r, g, b;
		bool operator==(const RGB&) const = default;
	};

	class Image {
		std::size_t m_width{};
		std::size_t m_height{};
		std::vector<RGB> m_pixels;

	public:
		Image(void) = default;
		Image(std::size_t p_width, std::size_t p_height);

		std::size_t width(void) const;
		std::size_t height(void) const;
		const RGB& at(std::size_t p_x, std::size_t p_y) const;
		RGB& at(std::size_t p_x, std::size_t p_y);
		RGB& at(int p_x, int p_y);
		const std::vector<RGB>& pixels(void) const;
		std::vector<RGB>& pixels(void);
	};

}

#endif
