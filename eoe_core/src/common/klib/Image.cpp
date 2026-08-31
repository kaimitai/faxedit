#include "Image.h"
#include <stdexcept>

klib::Image::Image(std::size_t p_width, std::size_t p_height) :
	m_width{ p_width },
	m_height{ p_height },
	m_pixels(p_width* p_height) {
}

std::size_t klib::Image::width(void) const { return m_width; }
std::size_t klib::Image::height(void) const { return m_height; }

const klib::RGB& klib::Image::at(std::size_t p_x, std::size_t p_y) const {
	return m_pixels.at(p_y * m_width + p_x);
}

klib::RGB& klib::Image::at(std::size_t p_x, std::size_t p_y) {
	return m_pixels.at(p_y * m_width + p_x);
}

klib::RGB& klib::Image::at(int p_x, int p_y) {
	if (p_x < 0 || p_y < 0)
		throw std::runtime_error("Image coordinate(s) negative");

	return at(static_cast<std::size_t>(p_x), static_cast<std::size_t>(p_y));
}

const std::vector<klib::RGB>& klib::Image::pixels(void) const {
	return m_pixels;
}

std::vector<klib::RGB>& klib::Image::pixels(void) {
	return m_pixels;
}
