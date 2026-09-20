#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using Asm = klib::Asm6502;
	void require(bool ok, const std::string& message) {
		if (!ok) throw std::runtime_error(message);
	}
	void put(std::vector<byte>& rom, word addr, const std::string& hex) {
		for (std::size_t i{ 0 }; i < hex.size(); i += 2)
			rom.at(Asm::get_file_offset(15, addr) + i / 2) =
				static_cast<byte>(std::stoul(hex.substr(i, 2), nullptr, 16));
	}
	std::vector<byte> fixture() {
		// instruction-only queue fixture; no cartridge data is needed.
		std::vector<byte> rom(0x40010, 0xff);
		std::fill_n(rom.begin(), 16, 0);
		rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a;
		rom[4] = 16; rom[6] = 0x11;
		put(rom, 0xcf5b, "a9d08522a9068521");
		put(rom, 0xcf63, "a61fe420f04aa50a29fba8bd00051006297fc8c8c8c88c0020bd0005297fa8e8bd00058d0620e8bd00058d0620e8981865228522");
		put(rom, 0xcf97, "bd0005e88d072088d0f6");
		put(rom, 0xcfa1, "861fc621f00cbc000588c0f9b004a52230b0a9008d06208d062060");
		put(rom, 0xc9af, "a9078d1440"); put(rom, 0xc9de, "8d0120a55a");
		return rom;
	}
	std::size_t install(std::vector<byte>& rom, const std::string& spec = "AtlasDevPpuDrainUnroll",
		word origin = 0xfcce, std::size_t end = 0xffe0, byte bank = 15) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, bank, origin, end,
			fh::parse_general_hacks(spec), nullptr);
	}
	void reject(std::vector<byte> rom, const std::string& spec = "AtlasDevPpuDrainUnroll",
		word origin = 0xfcce, std::size_t end = 0xffe0, byte bank = 15) {
		const auto before{ rom }; bool threw{ false };
		try { install(rom, spec, origin, end, bank); }
		catch (const std::exception&) { threw = true; }
		require(threw, "invalid install accepted: " + spec);
		require(before == rom, "rejected install changed ROM");
	}
	// execute the emitted copier and check every queue read and PPU write.
	void run(const std::vector<byte>& rom, word origin, byte x, byte y) {
		byte a{ 0 }; bool carry{ false }, zero{ false }; word pc{ origin };
		const unsigned count{ y ? unsigned(y) : 256 }, start{ x };
		unsigned writes{ 0 }, reads{ 0 }, steps{ 0 };
		const auto next = [&]() { return rom.at(Asm::get_file_offset(15, pc++)); };
		const auto address = [&]() { const word lo{ next() }; return word(lo | (word(next()) << 8)); };
		const auto nz = [&](byte value) { zero = value == 0; return value; };
		const auto branch = [&](bool take) {
			const auto delta{ static_cast<signed char>(next()) };
			if (take) pc = static_cast<word>(pc + delta);
		};
		while (pc != 0xcfa1 && ++steps < 10000) {
			switch (next()) {
			case 0xc0: { const auto v{ next() }; carry = y >= v; zero = y == v; break; }
			case 0xe0: { const auto v{ next() }; carry = x >= v; zero = x == v; break; }
			case 0x90: branch(!carry); break;
			case 0xb0: branch(carry); break;
			case 0xd0: branch(!zero); break;
			case 0xf0: branch(zero); break;
			case 0xbd: {
				const auto addr{ address() + x };
				require(addr == 0x0500 + ((start + reads) & 255), "out-of-ring or out-of-order read");
				a = nz(static_cast<byte>(addr)); ++reads; break;
			}
			case 0x8d:
				require(address() == 0x2007 && writes < count && a == byte(start + writes), "incorrect PPU output");
				++writes; break;
			case 0xe8: x = nz(byte(x + 1)); break;
			case 0x88: y = nz(byte(y - 1)); break;
			case 0x8a: a = nz(x); break;
			case 0x98: a = nz(y); break;
			case 0xaa: x = nz(a); break;
			case 0xa8: y = nz(a); break;
			case 0x18: carry = false; break;
			case 0x38: carry = true; break;
			case 0x69: { const unsigned v{ a + unsigned(next()) + carry }; a = nz(byte(v)); carry = v > 255; break; }
			case 0xe9: { const int v{ a - int(next()) - !carry }; a = nz(byte(v)); carry = v >= 0; break; }
			case 0x4c: pc = address(); break;
			default: throw std::runtime_error("unexpected copier instruction");
			}
		}
		require(pc == 0xcfa1 && writes == count && reads == count && y == 0
			&& x == byte(start + count), "copier continuation mismatch");
	}
	void test() {
		for (const word cursor : { 0xc100, 0xfcce, 0xfd00, 0xfd01, 0xfe80 }) {
			auto rom{ fixture() }; const auto before{ rom };
			const word origin{ static_cast<word>((unsigned(cursor) + 255) & ~255u) };
			require(install(rom, "AtlasDevPpuDrainUnroll", cursor) == origin + 131u - cursor, "padding not allocated");
			const auto hook{ Asm::get_file_offset(15, 0xcf97) };
			require(rom[hook] == 0x4c && rom[hook + 1] == 0 && rom[hook + 2] == origin / 256, "wrong hook");
			for (std::size_t i{ 0 }; i < rom.size(); ++i)
				if (!(i >= hook && i < hook + 10) && !(i >= Asm::get_file_offset(15, origin)
					&& i < Asm::get_file_offset(15, origin) + 131))
					require(rom[i] == before[i], "write outside owned spans");
			for (unsigned x{ 0 }; x < 256; ++x)
				for (unsigned y{ 0 }; y < 256; ++y) run(rom, origin, byte(x), byte(y));
		}
		for (unsigned budget{ 1 }; budget <= 64; ++budget) {
			auto rom{ fixture() }; install(rom, "AtlasDevPpuDrainUnroll budget=" + std::to_string(budget));
			require(rom[Asm::get_file_offset(15, 0xcf5c)] == byte(0 - budget), "wrong threshold");
		}
		for (const std::string value : { "0", "65", "127", "256", "-1", "junk" })
			reject(fixture(), "AtlasDevPpuDrainUnroll budget=" + value);
		reject(fixture(), "AtlasDevPpuDrainUnroll unknown=1");
		reject(fixture(), "AtlasDevPpuDrainUnroll\nAtlasDevPpuDrainUnroll");
		reject(fixture(), "AtlasDevPpuDrainUnroll", 0xfcce, 0xfd82);
		auto exact{ fixture() }; require(install(exact, "AtlasDevPpuDrainUnroll", 0xfcce, 0xfd83) == 181, "exact fit");
		reject(fixture(), "AtlasDevPpuDrainUnroll", 0xff80, 0x10000);
		reject(fixture(), "AtlasDevPpuDrainUnroll", 0x8000, 0xbfff, 14);
		for (const word addr : { 0xcf5b, 0xcf63, 0xcf97, 0xcfa1, 0xfcce, 0xfd00, 0xfd82 }) {
			auto rom{ fixture() }; rom[Asm::get_file_offset(15, addr)] ^= 1; reject(rom);
		}
		for (const byte flags : { 0x10, 0x11, 0x12, 0x13 }) {
			auto rom{ fixture() }; rom[6] = flags; rom[15] = 0xab; install(rom);
		}
		for (const auto [index, value] : { std::pair{0, 0}, {4, 32}, {5, 1}, {6, 0x15}, {7, 0x10} }) {
			auto rom{ fixture() }; rom[index] = byte(value); reject(rom);
		}
		auto short_rom{ fixture() }; short_rom.resize(16); reject(short_rom);
		for (const std::string spec : { "AtlasDevFrameScheduler\nAtlasDevPpuDrainUnroll",
			"AtlasDevPpuDrainUnroll\nAtlasDevFrameScheduler" }) {
			auto rom{ fixture() }; install(rom, spec);
		}
	}
}

int main(int argc, char** argv) {
	try {
		test();
		if (argc == 3) {
			std::ifstream input(argv[1], std::ios::binary);
			require(bool(input), "cannot open source ROM");
			const std::vector<byte> source{ std::istreambuf_iterator<char>(input), {} };
			std::filesystem::create_directories(argv[2]);
			for (const unsigned budget : { 48, 64 }) {
				auto rom{ source }; install(rom, "AtlasDevPpuDrainUnroll budget=" + std::to_string(budget));
				std::ofstream output(std::filesystem::path(argv[2]) / ("drain-" + std::to_string(budget) + ".nes"), std::ios::binary);
				output.write(reinterpret_cast<const char*>(rom.data()), rom.size());
				require(bool(output), "cannot write patched ROM");
			}
		}
		std::cout << "ppu drain regression passed\n";
		return 0;
	} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
