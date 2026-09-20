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
			rom.at(Asm::get_file_offset(15, addr) + i / 2) = byte(std::stoul(hex.substr(i, 2), nullptr, 16));
	}
	std::vector<byte> fixture() {
		// authored instruction fixture, with no cartridge graphics or tables.
		std::vector<byte> rom(0x40010, 0xff); std::fill_n(rom.begin(), 16, 0);
		rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1a; rom[4] = 16; rom[6] = 0x11;
		put(rom, 0xc27c, "a000ad8b03c9379001c8b959c28d860360");
		put(rom, 0xc28d, "ad780348206fcda2078e7803bdcc02c9fff03c8d8b03207cc220b5cdad8b03c930d019ad8b0348a59a48a9098d8b03207cc220b5cd68859a688d8b03ae7803a59a9d2c03b5c2c9f09005a9ff9dcc02ae7803ca10b4688d78034cf4cf");
		put(rom, 0xcd6f, "a9098599a900859860");
		put(rom, 0xcd78, "ad000148ae8603201accad00808502ad01801869808503ad8b03c9379002e9370aa8b1028596c8b1021869808597ad8b03a8b91bce859b68aa201acc60");
		put(rom, 0xcdb5, "a5988500a59906002a06002a06002a06002a859a2078cda59bd00160ad000148ae8603201acca59985e9a59885e8a91020dccfa000b1969d0005e8c8c01090f5862068aa201acca5961869108596a59769008597a5981869108598a59969008599c69bd0b760");
		put(rom, 0xc9af, "a9078d1440"); put(rom, 0xc9de, "8d0120a55a");
		return rom;
	}
	std::size_t install(std::vector<byte>& rom, const std::string& spec = "AtlasDevQueueLess",
		word origin = 0xfcce, std::size_t end = 0xffe0, byte bank = 15) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, bank, origin, end,
			fh::parse_general_hacks(spec), nullptr);
	}
	void reject(std::vector<byte> rom, const std::string& spec = "AtlasDevQueueLess",
		word origin = 0xfcce, std::size_t end = 0xffe0, byte bank = 15) {
		const auto before{ rom }; bool threw{ false };
		try { install(rom, spec, origin, end, bank); } catch (const std::exception&) { threw = true; }
		require(threw, "invalid install accepted: " + spec);
		require(rom == before, "failed install changed ROM");
	}
	void run(const std::vector<byte>& rom, word origin, byte id, byte slot, int match) {
		std::array<byte, 0x400> ram{};
		ram[0x378] = slot; ram[0x38b] = id; ram[0x9a] = 0xab;
		for (unsigned i{ 0 }; i < 8; ++i) { ram[0x2cc + i] = 0xff; ram[0x32c + i] = byte(i * 17); }
		if (match >= 0) ram[0x2cc + match] = id;
		ram[0x2cc + slot] = id;
		const auto before{ ram }; byte a{}, x{ 0xba }; word pc{ origin }; bool carry{}, zero{}, returned{};
		const auto next = [&]() { return rom.at(Asm::get_file_offset(15, pc++)); };
		const auto address = [&]() { const word lo{ next() }; return word(lo | (word(next()) << 8)); };
		const auto branch = [&](bool take) {
			const auto delta{ static_cast<signed char>(next()) }; if (take) pc = word(pc + delta);
		};
		for (unsigned steps{ 0 }; steps < 100 && pc != 0xcdb5 && !returned; ++steps) {
			switch (next()) {
			case 0xad: a = ram.at(address()); break;
			case 0xae: x = ram.at(address()); break;
			case 0xbd: a = ram.at(address() + x); break;
			case 0xc9: { const auto v{ next() }; carry = a >= v; zero = a == v; break; }
			case 0xcd: { const auto v{ ram.at(address()) }; carry = a >= v; zero = a == v; break; }
			case 0xe0: { const auto v{ next() }; carry = x >= v; zero = x == v; break; }
			case 0xe8: ++x; zero = x == 0; break;
			case 0xb0: branch(carry); break;
			case 0xf0: branch(zero); break;
			case 0xd0: branch(!zero); break;
			case 0x85: { const auto addr{ next() }; require(addr == 0x9a, "unexpected RAM write"); ram[addr] = a; break; }
			case 0x60: returned = true; break;
			case 0x4c: pc = address(); break;
			default: throw std::runtime_error("unexpected dedup instruction");
			}
		}
		const bool reuse{ id != 0x30 && match > slot };
		require(returned == reuse && (reuse || pc == 0xcdb5), "wrong upload decision");
		require(ram[0x9a] == (reuse ? byte(match * 17) : byte(0xab)), "wrong tile base");
		ram[0x9a] = before[0x9a]; require(ram == before, "modified non-output RAM");
	}
	void test() {
		for (const word origin : { 0xc100, 0xfcce, 0xfeff }) {
			auto rom{ fixture() }; const auto before{ rom };
			require(install(rom, "AtlasDevQueueLess", origin) == 32, "wrong allocation");
			const auto hook{ Asm::get_file_offset(15, 0xc2a6) }, off{ Asm::get_file_offset(15, origin) };
			require(rom[hook] == 0x20 && rom[hook + 1] == byte(origin) && rom[hook + 2] == byte(origin >> 8), "wrong hook");
			for (std::size_t i{ 0 }; i < rom.size(); ++i)
				if (!(i >= hook && i < hook + 3) && !(i >= off && i < off + 32))
					require(rom[i] == before[i], "write outside owned spans");
			for (unsigned id{ 0 }; id < 255; ++id) for (byte slot{ 0 }; slot < 8; ++slot)
				for (int match{ -1 }; match < 8; ++match) run(rom, origin, byte(id), slot, match);
		}
		auto disabled{ fixture() }; const auto before{ disabled };
		// project export repacks the two sprite banks and updates all three
		// split operands together. a partly changed loader must be rejected.
		for (const byte split : { 0x30, 0x37, 0x39, 0x40 }) {
			auto rom{ fixture() };
			for (const word addr : { 0xc282, 0xcd93, 0xcd97 }) rom[Asm::get_file_offset(15, addr)] = split;
			install(rom);
		}
		for (const word addr : { 0xc282, 0xcd93, 0xcd97 }) {
			auto rom{ fixture() }; rom[Asm::get_file_offset(15, addr)] ^= 1; reject(rom);
		}
		require(install(disabled, "AtlasDevQueueLess dedup=0") == 0 && disabled == before, "off is not a no-op");
		for (const std::string spec : { "AtlasDevQueueLess dedup=2", "AtlasDevQueueLess canvas=1",
			"AtlasDevQueueLess\nAtlasDevQueueLess", "AtlasDevQueueLess dedup=bad" }) reject(fixture(), spec);
		for (const word addr : { 0xc27c, 0xc28d, 0xc2a6, 0xcd6f, 0xcd78, 0xcdb5, 0xce1a, 0xfcce, 0xfced }) {
			auto rom{ fixture() }; rom[Asm::get_file_offset(15, addr)] ^= 1; reject(rom);
		}
		reject(fixture(), "AtlasDevQueueLess", 0xfcce, 0xfced);
		auto exact{ fixture() }; require(install(exact, "AtlasDevQueueLess", 0xfcce, 0xfcee) == 32, "exact fit failed");
		reject(fixture(), "AtlasDevQueueLess", 0xffe0, 0x10000);
		reject(fixture(), "AtlasDevQueueLess", 0x8000, 0xbfff, 14);
		for (const byte flags : { 0x10, 0x11, 0x12, 0x13 }) { auto rom{ fixture() }; rom[6] = flags; rom[15] = 99; install(rom); }
		for (const auto [index, value] : { std::pair{0, 0}, {4, 32}, {5, 1}, {6, 0x15}, {7, 0x10} }) {
			auto rom{ fixture() }; rom[index] = byte(value); reject(rom);
		}
		auto short_rom{ fixture() }; short_rom.resize(16); reject(short_rom);
		for (const std::string spec : { "AtlasDevFrameScheduler\nAtlasDevQueueLess", "AtlasDevQueueLess\nAtlasDevFrameScheduler" }) {
			auto rom{ fixture() }; require(install(rom, spec) == 188, "scheduler composition size");
		}
	}
}

int main(int argc, char** argv) {
	try {
		test();
		if (argc == 3) {
			std::ifstream input(argv[1], std::ios::binary); require(bool(input), "cannot open ROM");
			const std::vector<byte> source{ std::istreambuf_iterator<char>(input), {} };
			std::filesystem::create_directories(argv[2]);
			auto rom{ source }; install(rom);
			std::ofstream output(std::filesystem::path(argv[2]) / "queue-less.nes", std::ios::binary);
			output.write(reinterpret_cast<const char*>(rom.data()), rom.size()); require(bool(output), "cannot write ROM");
			std::array<std::string, 3> specs{ "AtlasDevPpuDrainUnroll", "AtlasDevQueueLess", "AtlasDevSpriteSpeed" };
			do {
				auto composed{ source };
				const auto used{ install(composed, specs[0] + "\n" + specs[1] + "\n" + specs[2]) };
				require(used <= 786, "composition capacity");
				std::cout << "composition: " << specs[0] << ", " << specs[1] << ", " << specs[2] << ": " << used << " bytes\n";
			} while (std::next_permutation(specs.begin(), specs.end()));
		}
		std::cout << "queue less regression passed\n"; return 0;
	} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
