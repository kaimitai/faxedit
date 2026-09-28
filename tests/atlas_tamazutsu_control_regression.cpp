#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevTamazutsuControl to known bytes for the default, the
// shortest time underground, the longest time up, the shortest warning, the
// longest times underground and warning with the shortest time up, and a flag;
// the code goes in bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// tamazutsu's routine, $9865 to $9916
	const std::string ROUTINE_HEX{
		"2085a8d00da93c9dec02a9009de4022094a8207b86bce402f00888f00d88f012d01820bfa8a90b4ca79820bfa8a93c4ca79820cea8"
		"a90b4ca79820bfa8a93c4ca798deec02d006fee4029dec0260bde40229039de40220828cbce402f00888f01688f020d02bbdec02c9"
		"1eb0e0a0002908f028a002d024bdec024a4aa8b90599a84c0099a00abdec022908f00ea00cd00abdec024a4aa8b90899a8984a4c8e"
		"8c080604040608ad83034a4a4a4a29034c8e8c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevTamazutsuControl", 0, {
		} },
		{ "AtlasDevTamazutsuControl hide=16", 0, {
			{ 14, 0x986b, "10" },
			{ 14, 0x98a3, "10" },
		} },
		{ "AtlasDevTamazutsuControl up=255", 0, {
			{ 14, 0x9893, "ff" },
		} },
		{ "AtlasDevTamazutsuControl warn=1", 0, {
			{ 14, 0x98cf, "01" },
		} },
		{ "AtlasDevTamazutsuControl hide=30 up=120 warn=10", 0, {
			{ 14, 0x986b, "1e" },
			{ 14, 0x9893, "78" },
			{ 14, 0x98a3, "1e" },
			{ 14, 0x98cf, "0a" },
		} },
		{ "AtlasDevTamazutsuControl flag=12", 0, {
		} },
		{ "AtlasDevTamazutsuControl hide=30 up=120 warn=10 flag=12", 74, {
			{ 14, 0x986a, "4cd4fc" },
			{ 14, 0x9892, "4cf3fc" },
			{ 14, 0x98a2, "4ce5fc" },
			{ 14, 0x98ce, "4c01fd" },
			{ 15, 0xfcce, "ad020129106020cefcf004a91ed002a93c9dec024c6f9820cefcf004a91ed002a93c4ca79820cefcf004a978d002a93c4ca7984820cefcd00668c91e4c10fd68c90ab0034cd2984cb298" },
		} },
		{ "AtlasDevTamazutsuControl warn=1 flag=12", 25, {
			{ 14, 0x98ce, "4ccefc" },
			{ 15, 0xfcce, "48ad02012910d00668c91e4cdffc68c901b0034cd2984cb298" },
		} },
		{ "AtlasDevTamazutsuControl warn=1 up=255 flag=12", 41, {
			{ 14, 0x9892, "4ccefc" },
			{ 14, 0x98ce, "4cdefc" },
			{ 15, 0xfcce, "ad02012910f004a9" },
			{ 15, 0xfcd7, "d002a93c4ca79848ad02012910d00668c91e4ceffc68c901b0034cd2984cb298" },
		} },
		{ "AtlasDevTamazutsuControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9865, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevTamazutsuControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevTamazutsuControl hide=15", "AtlasDevTamazutsuControl hide=256",
				"AtlasDevTamazutsuControl up=15", "AtlasDevTamazutsuControl up=256", "AtlasDevTamazutsuControl warn=0",
				"AtlasDevTamazutsuControl warn=256", "AtlasDevTamazutsuControl flag=248", "AtlasDevTamazutsuControl mode=on",
				"AtlasDevTamazutsuControl speed=3", "AtlasDevTamazutsuControl mode=vanilla hide=15" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9893)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevTamazutsuControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla up site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x98ce, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevTamazutsuControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevTamazutsuControl hide=30 up=120 warn=10 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_tamazutsu_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_tamazutsu_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
