#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevRipasheikuControl to known bytes for the default, the
// slowest drift, the fastest drift with the shortest sit and fire, the longest
// fire and a flag; the code goes in bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// ripasheiku's routine, $9b45 to $9c86, with the three sites
	const std::string ROUTINE_HEX{
		"ad8303293fc920d01abddc022901a8b5ba1879819b8d8403b5c21869208d850320f6a0207b86deec02d010a9019de402"
		"a9009dec029df402207b86601000ad85a8d012a9039d0403a91e9dec02a9009de4022094a8bce402f00988f01a88d0a0"
		"4c0e9c207b86deec02d00bfee402a9009dec029df4026020439cbddc02297f9ddc02b5c2c9309031bdf402100ca9048d"
		"7703a9008d7603f00aa00320e183a00520d18320c485b010fef402bdf402291fd005a9ff9df4026060a9009dec02a9ff"
		"9df402fee4024c7b8620439cbddc0209809ddc02bdf402a00220d18320c485b00bbdf40238e9049df402b011fee402a9"
		"009dec029df4029dfc02207b8660a9018d7503a9008d74032019846020828ca000bde402f00fc903f00fa001ad830329"
		"08f002a002984c8e8ca001ad83032920f0f3a003d0ef20828cad83034a4a4a29034c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevRipasheikuControl", 0, {
		} },
		{ "AtlasDevRipasheikuControl drift=1", 0, {
			{ 14, 0x9c44, "00" },
			{ 14, 0x9c49, "20" },
		} },
		{ "AtlasDevRipasheikuControl drift=64 sit=1 fire=16", 10, {
			{ 14, 0x9b49, "0f" },
			{ 14, 0x9b4b, "08" },
			{ 14, 0x9c34, "4ccefc" },
			{ 14, 0x9c44, "08" },
			{ 15, 0xfcce, "a9019dec02a9004c399c" },
		} },
		{ "AtlasDevRipasheikuControl flag=12", 0, {
		} },
		{ "AtlasDevRipasheikuControl drift=64 sit=1 fire=16 flag=12", 65, {
			{ 14, 0x9b45, "4cd4fc" },
			{ 14, 0x9c34, "4ce9fc" },
			{ 14, 0x9c43, "4cf8fc" },
			{ 15, 0xfcce, "ad020129106020cefcf00aad8303290fc9084c4c9bad83034c489b20cefcf002a9019dec02a9004c399c20cefcf00aa9088d7503a9004c4a9ca9018d75034c489c" },
		} },
		{ "AtlasDevRipasheikuControl sit=1 flag=12", 17, {
			{ 14, 0x9c34, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f002a9019dec02a9004c399c" },
		} },
		{ "AtlasDevRipasheikuControl sit=1 fire=16 flag=12", 40, {
			{ 14, 0x9b45, "4ccefc" },
			{ 14, 0x9c34, "4ce5fc" },
			{ 15, 0xfcce, "ad02012910f00aad8303290fc9084c4c9bad83034c489bad02012910f002a9019dec02a9004c399c" },
		} },
		{ "AtlasDevRipasheikuControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9b45, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevRipasheikuControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevRipasheikuControl drift=0", "AtlasDevRipasheikuControl drift=65",
				"AtlasDevRipasheikuControl sit=0", "AtlasDevRipasheikuControl sit=257", "AtlasDevRipasheikuControl fire=48",
				"AtlasDevRipasheikuControl flag=248", "AtlasDevRipasheikuControl mode=on", "AtlasDevRipasheikuControl speed=3",
				"AtlasDevRipasheikuControl mode=vanilla drift=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9b49)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevRipasheikuControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla shot test is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9c43, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevRipasheikuControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevRipasheikuControl drift=64 sit=1 fire=16"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_ripasheiku_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_ripasheiku_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
