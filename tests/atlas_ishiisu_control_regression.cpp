#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevIshiisuControl to known bytes for the default, the
// slowest walk, the shortest range and timings, the longest ones with the
// fastest walk, face without and with a flag, and a flag; the code goes in
// bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// ishiisu's routine, $9129 to $91b2
	const std::string ROUTINE_HEX{
		"2085a8d008a9009de4022094a8bde4024ab03ab5ba38e59eb00449ff6901c9209013207b86a9c08d7403a9008d75032019844c0785"
		"bddc022901a8a5a42940598c91d008fee402a91e9dec0260207b86deec02bdec02f007c914d0064ca0a0fee40260400020828cbde4"
		"024ab00da000ad83032904f00fa001d00ba002bdec02c914b002a003984c8e8c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevIshiisuControl", 0, {
		} },
		{ "AtlasDevIshiisuControl walk=1", 0, {
			{ 14, 0x914f, "40" },
		} },
		{ "AtlasDevIshiisuControl range=8 windup=1 recover=1", 0, {
			{ 14, 0x9148, "08" },
			{ 14, 0x9171, "02" },
			{ 14, 0x9182, "01" },
			{ 14, 0x91aa, "01" },
		} },
		{ "AtlasDevIshiisuControl walk=8 range=128 windup=64 recover=64", 0, {
			{ 14, 0x9148, "80" },
			{ 14, 0x914f, "00" },
			{ 14, 0x9154, "02" },
			{ 14, 0x9171, "80" },
			{ 14, 0x9182, "40" },
			{ 14, 0x91aa, "40" },
		} },
		{ "AtlasDevIshiisuControl face=1", 0, {
			{ 14, 0x915e, "4c6d91" },
		} },
		{ "AtlasDevIshiisuControl flag=12", 0, {
		} },
		{ "AtlasDevIshiisuControl face=1 flag=12", 16, {
			{ 14, 0x915e, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f0034c6d91bddc024c6191" },
		} },
		{ "AtlasDevIshiisuControl walk=8 range=128 windup=64 recover=64 flag=12", 118, {
			{ 14, 0x9147, "4ceefc" },
			{ 14, 0x914e, "4cd4fc" },
			{ 14, 0x9170, "4c05fd" },
			{ 14, 0x9181, "4c16fd" },
			{ 14, 0x91a9, "4c2dfd" },
			{ 15, 0xfcce, "ad020129106020cefcf00da9008d7403a9028d75034c5891a9c08d74034c53914820cefcd00668c9204cfdfc68c98090034c4b914c5e9120cefcf004a980d002a91e9dec024c75914820cefcd00668c9144c25fd68c940d0034c85914c8b914820cefcd00668c9144c3cfd68c940b0034cad914caf91" },
		} },
		{ "AtlasDevIshiisuControl range=8 flag=12", 25, {
			{ 14, 0x9147, "4ccefc" },
			{ 15, 0xfcce, "48ad02012910d00668c9204cdffc68c90890034c4b914c5e91" },
		} },
		{ "AtlasDevIshiisuControl range=8 walk=8 flag=12", 53, {
			{ 14, 0x9147, "4ceafc" },
			{ 14, 0x914e, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f00da9008d7403a9028d75034c5891a9c08d74034c539148ad02012910d00668c9204cfbfc68c90890034c4b914c5e91" },
		} },
		{ "AtlasDevIshiisuControl walk=8 face=1", 0, {
			{ 14, 0x914f, "00" },
			{ 14, 0x9154, "02" },
			{ 14, 0x915e, "4c6d91" },
		} },
		{ "AtlasDevIshiisuControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9129, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevIshiisuControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevIshiisuControl walk=0", "AtlasDevIshiisuControl walk=9",
				"AtlasDevIshiisuControl range=7", "AtlasDevIshiisuControl range=129", "AtlasDevIshiisuControl windup=0",
				"AtlasDevIshiisuControl windup=65", "AtlasDevIshiisuControl recover=0", "AtlasDevIshiisuControl recover=65",
				"AtlasDevIshiisuControl face=2", "AtlasDevIshiisuControl flag=248", "AtlasDevIshiisuControl mode=on",
				"AtlasDevIshiisuControl speed=3", "AtlasDevIshiisuControl mode=vanilla walk=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9148)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevIshiisuControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla range site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9170, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevIshiisuControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x915f)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevIshiisuControl face=1"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "with face=1 a non vanilla facing test is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevIshiisuControl face=1 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_ishiisu_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_ishiisu_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
