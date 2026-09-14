#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevMagmanControl to known bytes for the default, the
// shortest hide, the longest stay, the farthest appearance, a tuned shape and
// a flag; the code goes in bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// magman's routine, $95c0 to $9631
	const std::string ROUTINE_HEX{
		"2085a8d015a93c9dec02a9009de402a9d095c2a9f095ba2094a8bde4024ab022deec02d01cfee402a9789dec02a030a5a42940d002a0d0"
		"9818659e95baa5a195c260207b86deec02d010fee402a93c9dec02a9d095c2a9f095ba6020828cbde4024a90f6a000ad83032908f001c8"
		"984c8e8c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevMagmanControl", 0, {
		} },
		{ "AtlasDevMagmanControl hide=16", 0, {
			{ 14, 0x95c6, "10" },
			{ 14, 0x960e, "10" },
		} },
		{ "AtlasDevMagmanControl stay=255", 0, {
			{ 14, 0x95e9, "ff" },
		} },
		{ "AtlasDevMagmanControl distance=112", 0, {
			{ 14, 0x95ee, "70" },
			{ 14, 0x95f6, "90" },
		} },
		{ "AtlasDevMagmanControl hide=30 stay=60 distance=80", 0, {
			{ 14, 0x95c6, "1e" },
			{ 14, 0x95e9, "3c" },
			{ 14, 0x95ee, "50" },
			{ 14, 0x95f6, "b0" },
			{ 14, 0x960e, "1e" },
		} },
		{ "AtlasDevMagmanControl flag=12", 0, {
		} },
		{ "AtlasDevMagmanControl hide=30 stay=60 distance=80 flag=12", 88, {
			{ 14, 0x95c5, "4cd4fc" },
			{ 14, 0x95e8, "4cf6fc" },
			{ 14, 0x95ed, "4c07fd" },
			{ 14, 0x960d, "4ce5fc" },
			{ 15, 0xfcce, "ad020129106020cefcf004a91ed002a93c9dec024cca9520cefcf004a91ed002a93c9dec024c129620cefcf004a93cd002a9789dec024ced9520cefcf00da050a5a42940d002a0b04cf795a030a5a42940d002a0d04cf795" },
		} },
		{ "AtlasDevMagmanControl distance=112 flag=12", 33, {
			{ 14, 0x95ed, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f00da070a5a42940d002a0904cf795a030a5a42940d002a0d04cf795" },
		} },
		{ "AtlasDevMagmanControl distance=112 stay=255 flag=12", 52, {
			{ 14, 0x95e8, "4ccefc" },
			{ 14, 0x95ed, "4ce1fc" },
			{ 15, 0xfcce, "ad02012910f004a9" },
			{ 15, 0xfcd7, "d002a9789dec024ced95ad02012910f00da070a5a42940d002a0904cf795a030a5a42940d002a0d04cf795" },
		} },
		{ "AtlasDevMagmanControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x95c0, from_hex(ROUTINE_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

}

int main() {
	try {
		for (const auto& shape : SHAPES) {
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, shape.spec) };
			const std::string spec{ shape.spec };
			require(n == shape.size, spec + ": size " + std::to_string(n));
			auto want{ vanilla_rom() };
			for (const auto& r : shape.regions) {
				const auto bytes{ from_hex(r.hex) };
				const auto off{ klib::Asm6502::get_file_offset(r.bank, r.cpu) };
				for (std::size_t i{ 0 }; i < bytes.size(); ++i) want[off + i] = bytes[i];
			}
			for (std::size_t i{ 0 }; i < rom.size(); ++i)
				require(rom[i] == want[i], spec + ": file offset " + std::to_string(i) + " differs from the expected install");
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevMagmanControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevMagmanControl hide=15", "AtlasDevMagmanControl hide=256",
				"AtlasDevMagmanControl stay=15", "AtlasDevMagmanControl stay=256", "AtlasDevMagmanControl distance=7",
				"AtlasDevMagmanControl distance=113", "AtlasDevMagmanControl flag=248", "AtlasDevMagmanControl mode=on",
				"AtlasDevMagmanControl speed=3", "AtlasDevMagmanControl mode=vanilla hide=15" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x95ee)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevMagmanControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla distance site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x95e8, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevMagmanControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevMagmanControl hide=30 stay=60 distance=80 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_magman_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_magman_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
