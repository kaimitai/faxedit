#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevNashControl to known bytes for the default, the
// shortest hide, the longest windup and attack with the latest throw, the
// earliest throw, a tuned shape and a flag; the code goes in bank 15 and the
// sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// nash's routine, $9060 to $910e
	const std::string ROUTINE_HEX{
		"2085a8d008a9789dec022094a8bce402f01d88f059deec02f00abdec02c90ad00d4ca0a0a9009de402a9789dec026020bfa8deec02d036"
		"20cea8fee402a93c9dec02a5a42940d015a59e18692095bab004c9f09018a59e38e92095ba60a59e38e92095bab007a59e18692095ba60"
		"207b86deec02d008a93c9dec02fee4026020828cbde402f00fc902f00cad83032904d004a000f01560a000bdec0238e90ac928b008a001"
		"c914b002a002984c8e8c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevNashControl", 0, {
		} },
		{ "AtlasDevNashControl hide=16", 0, {
			{ 14, 0x9066, "10" },
			{ 14, 0x908a, "10" },
		} },
		{ "AtlasDevNashControl windup=255 attack=255 throw=254", 0, {
			{ 14, 0x907e, "fe" },
			{ 14, 0x909e, "ff" },
			{ 14, 0x90d7, "ff" },
		} },
		{ "AtlasDevNashControl throw=1", 0, {
			{ 14, 0x907e, "01" },
		} },
		{ "AtlasDevNashControl hide=60 windup=30 attack=90 throw=45", 0, {
			{ 14, 0x9066, "3c" },
			{ 14, 0x907e, "2d" },
			{ 14, 0x908a, "3c" },
			{ 14, 0x909e, "1e" },
			{ 14, 0x90d7, "5a" },
		} },
		{ "AtlasDevNashControl flag=12", 0, {
		} },
		{ "AtlasDevNashControl hide=60 windup=30 attack=90 throw=45 flag=12", 97, {
			{ 14, 0x9065, "4cd4fc" },
			{ 14, 0x907d, "4ce5fc" },
			{ 14, 0x9089, "4cfcfc" },
			{ 14, 0x909d, "4c0dfd" },
			{ 14, 0x90d6, "4c1efd" },
			{ 15, 0xfcce, "ad020129106020cefcf004a93cd002a9789dec024c6a904820cefcd00668c90a4cf4fc68c92dd0034c81904c8e9020cefcf004a93cd002a9789dec024c8e9020cefcf004a91ed002a93c9dec024ca29020cefcf004a95ad002a93c9dec024cdb90" },
		} },
		{ "AtlasDevNashControl throw=45 flag=12", 25, {
			{ 14, 0x907d, "4ccefc" },
			{ 15, 0xfcce, "48ad02012910d00668c90a4cdffc68c92dd0034c81904c8e90" },
		} },
		{ "AtlasDevNashControl throw=45 hide=60 flag=12", 63, {
			{ 14, 0x9065, "4cd4fc" },
			{ 14, 0x907d, "4ce5fc" },
			{ 14, 0x9089, "4cfcfc" },
			{ 15, 0xfcce, "ad020129106020cefcf004a93cd002a9789dec024c6a904820cefcd00668c90a4cf4fc68c92dd0034c81904c8e9020cefcf004a93cd002a9789dec024c8e90" },
		} },
		{ "AtlasDevNashControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9060, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevNashControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevNashControl hide=15", "AtlasDevNashControl hide=256",
				"AtlasDevNashControl windup=15", "AtlasDevNashControl windup=256", "AtlasDevNashControl attack=15",
				"AtlasDevNashControl attack=256", "AtlasDevNashControl throw=0", "AtlasDevNashControl throw=60",
				"AtlasDevNashControl flag=248", "AtlasDevNashControl mode=on", "AtlasDevNashControl speed=3",
				"AtlasDevNashControl mode=vanilla hide=15" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x907e)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNashControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla throw site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x909d, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevNashControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNashControl hide=60 windup=30 attack=90 throw=45 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_nash_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_nash_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
