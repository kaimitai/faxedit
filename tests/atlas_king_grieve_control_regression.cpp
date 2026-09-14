#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevKingGrieveControl to known bytes for the default, the
// fastest and slowest fire, the longest hover with the shortest rest, a tuned
// shape, the full box and a flag; the code goes in bank 15 and the sites in
// bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// king grieve's routine, $9f03 to $9fe2, and its box record
	const std::string ROUTINE_HEX{
		"2085a8d015a9059d0403a9ff9df402a9009dec029de4022094a8bde4022903a8f00988f03d88f0734c869fa9018d7503a9008d74032094"
		"84bddc0209809ddc02bdf402a00220d18320c485b00cbdf40238e9029df402900160fee402a93c9dec0260ad8303290fd01abddc022901"
		"a8b5ba18799c9f8d8403b5c21869048d850320f6a0207b86deec02d00dfee402a9ff9df402a9009dec02600030bddc02297f9ddc02a900"
		"8d7603a9018d770320c485b006b5c2c910b008fee402a91e9dec026020828ca002bde4022903f009ad83034a4a4a2903a8b9df9f4c8e8c"
		"00010201" };
	const std::string BOX_HEX{ "00004018" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevKingGrieveControl", 0, {
		} },
		{ "AtlasDevKingGrieveControl shots=4", 0, {
			{ 14, 0x9f69, "03" },
		} },
		{ "AtlasDevKingGrieveControl shots=128", 0, {
			{ 14, 0x9f69, "7f" },
		} },
		{ "AtlasDevKingGrieveControl hover=255 rest=16", 0, {
			{ 14, 0x9f60, "ff" },
			{ 14, 0x9fbf, "10" },
		} },
		{ "AtlasDevKingGrieveControl shots=8 hover=90 rest=60", 0, {
			{ 14, 0x9f60, "5a" },
			{ 14, 0x9f69, "07" },
			{ 14, 0x9fbf, "3c" },
		} },
		{ "AtlasDevKingGrieveControl body=1", 0, {
			{ 14, 0xb33b, "f8f15e37" },
		} },
		{ "AtlasDevKingGrieveControl flag=12", 0, {
		} },
		{ "AtlasDevKingGrieveControl shots=8 hover=90 rest=60 flag=12", 71, {
			{ 14, 0x9f5f, "4cf3fc" },
			{ 14, 0x9f65, "4cd4fc" },
			{ 14, 0x9fbe, "4c04fd" },
			{ 15, 0xfcce, "ad020129106020cefcf00dad83032907f0034c869f4c6c9fad8303290ff0034c869f4c6c9f20cefcf004a95ad002a93c9dec024c649f20cefcf004a93cd002a91e9dec024cc39f" },
		} },
		{ "AtlasDevKingGrieveControl shots=4 flag=12", 33, {
			{ 14, 0x9f65, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f00dad83032903f0034c869f4c6c9fad8303290ff0034c869f4c6c9f" },
		} },
		{ "AtlasDevKingGrieveControl shots=4 rest=255 flag=12", 52, {
			{ 14, 0x9f65, "4ccefc" },
			{ 14, 0x9fbe, "4ceffc" },
			{ 15, 0xfcce, "ad02012910f00dad83032903f0034c869f4c6c9fad8303290ff0034c869f4c6c9fad02012910f004a9" },
			{ 15, 0xfcf8, "d002a91e9dec024cc39f" },
		} },
		{ "AtlasDevKingGrieveControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9f03, from_hex(ROUTINE_HEX));
		put(rom, 14, 0xb33b, from_hex(BOX_HEX));
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
			require(install(rom, "AtlasDevKingGrieveControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevKingGrieveControl shots=12", "AtlasDevKingGrieveControl shots=2",
				"AtlasDevKingGrieveControl shots=256", "AtlasDevKingGrieveControl hover=15", "AtlasDevKingGrieveControl hover=256",
				"AtlasDevKingGrieveControl rest=15", "AtlasDevKingGrieveControl rest=256", "AtlasDevKingGrieveControl body=2",
				"AtlasDevKingGrieveControl flag=248", "AtlasDevKingGrieveControl mode=on", "AtlasDevKingGrieveControl speed=3",
				"AtlasDevKingGrieveControl mode=vanilla shots=12" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9f66)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevKingGrieveControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla shots site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0xb33b)] = 0x05;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevKingGrieveControl body=1"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "with body=1 a changed box is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevKingGrieveControl shots=8 hover=90 rest=60 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_king_grieve_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_king_grieve_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
