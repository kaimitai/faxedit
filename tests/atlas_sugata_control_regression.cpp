#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevSugataControl to known bytes for the default, a curse
// that takes no hp, the shortest warning, the longest warning with the most
// damage and a flag; the code goes in bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// sugata's curse, $ab26 to $ab66, with both sites
	const std::string ROUTINE_HEX{
		"2085a8d00ea9029dec02a50b0901850b2094a8deec02d028a50b29fe850ba90420e4d0a93c85ada5a5090285a5a9008dbc04a9"
		"0a8dbd04208ec0ae78034caca860" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevSugataControl", 0, {
		} },
		{ "AtlasDevSugataControl damage=0", 0, {
			{ 14, 0xab59, "00" },
		} },
		{ "AtlasDevSugataControl flash=255 damage=255", 0, {
			{ 14, 0xab2c, "ff" },
			{ 14, 0xab59, "ff" },
		} },
		{ "AtlasDevSugataControl flag=12", 0, {
		} },
		{ "AtlasDevSugataControl flash=255 damage=255 flag=12", 38, {
			{ 14, 0xab2b, "4ccefc" },
			{ 14, 0xab58, "4ce1fc" },
			{ 15, 0xfcce, "ad02012910f004a9" },
			{ 15, 0xfcd7, "d002a9029dec024c30abad02012910f004a9" },
			{ 15, 0xfcea, "d002a90a8dbd044c5dab" },
		} },
		{ "AtlasDevSugataControl damage=255 flag=12", 19, {
			{ 14, 0xab58, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f004a9" },
			{ 15, 0xfcd7, "d002a90a8dbd044c5dab" },
		} },
		{ "AtlasDevSugataControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0xab26, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevSugataControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevSugataControl damage=256", "AtlasDevSugataControl flash=0",
				"AtlasDevSugataControl flash=256", "AtlasDevSugataControl flag=248", "AtlasDevSugataControl mode=on",
				"AtlasDevSugataControl speed=3", "AtlasDevSugataControl mode=vanilla flash=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0xab59)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevSugataControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla damage is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0xab58, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevSugataControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevSugataControl flash=255 damage=255 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_sugata_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_sugata_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
