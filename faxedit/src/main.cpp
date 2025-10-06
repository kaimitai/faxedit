#include <vector>
#include <iostream>
#include "./../../common/source/Bitreader.h"
#include "./../../common/source/kfile/Kfile.h"

using byte = unsigned char;

static std::string byte_to_hex(byte p_b) {
	std::string result; // { "0x" };

	if (p_b < 16)
		result += '0';
	else {
		byte l_val{ static_cast<byte>(p_b / 16) };

		if (l_val < 10)
			l_val += '0';
		else
			l_val += 'a' - 10;

		result += l_val;
	}

	byte l_val{ static_cast<byte>(p_b % 16) };

	if (l_val < 10)
		l_val += '0';
	else
		l_val += 'a' - 10;

	result += l_val;

	return result;
}

// return 16x13 screen
static std::vector<byte> parse_screen(const std::vector<byte>& p_rom,
	std::size_t p_offset) {
	std::vector<byte> result{
		std::vector<byte>(13 * 16, 0)
	};

	kf::Bitreader reader(p_offset);

	std::size_t idx{ 0 };

	while (idx < (16 * 13)) {
		unsigned int l_control{ reader.read_int(p_rom, 2) };
		byte l_tile_val{ 0 };

		switch (l_control) {

		case 0:
			l_tile_val = result[idx - 1];
			break;

		case 1:
			l_tile_val = result[idx - 16];
			break;

		case 2:
			l_tile_val = result[idx - 17];
			break;

		case 3:
			l_tile_val = static_cast<byte>(reader.read_int(p_rom, 8));
			break;

		default:
			// can't fall through to here
			break;
		}

		result[idx++] = l_tile_val;

	}

	return result;
}

int main(int argc, char** argv) {

	kf::Kfile l_file;
	auto l_bytes{ l_file.read_file_as_bytes("c:/Temp/Faxanadu (USA) (Rev A).nes") };

	auto l_screen{ parse_screen(l_bytes, 0x28) };

	for (int y{ 0 }; y < 13; ++y) {
		std::cout << "\n";
		for (int x{ 0 }; x < 16; ++x) {
			std::cout << byte_to_hex(l_screen.at(y * 16 + x)) << " ";
		}
	}
}
