#include "Image.h"

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

const std::vector<klib::RGB>& klib::Image::pixels(void) const {
	return m_pixels;
}

std::vector<klib::RGB>& klib::Image::pixels(void) {
	return m_pixels;
}
