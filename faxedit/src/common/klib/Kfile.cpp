#include "Kfile.h"
#include <fstream>
#include <stdexcept>

std::vector<byte> klib::Kfile::read_file_as_bytes(const std::string& p_filename) const {
    std::ifstream file(p_filename, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + p_filename);

    // Seek to end to determine size
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    if (size < 0)
        throw std::runtime_error("Failed to determine file size: " + p_filename);
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
        throw std::runtime_error("Failed to read file: " + p_filename);

    return buffer;
}
