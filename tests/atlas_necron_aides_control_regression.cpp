#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevNecronAidesControl to known bytes for the default, the
// slow and fast climbs, the fastest walk, cling with and without a flag, and a
// flag; the code goes in bank 15, the climb and walk sites in bank 14, and
// cling=1 retargets one instruction of the enemy touch code
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// the necron aides' routine with its tables, $8da3 to $8e43
	const std::string ROUTINE_HEX{
		"2085a8d00ba9009dec029df4022094a8ae7803bde4023053bddc022a2a2901a8b5c218793a8e85b6b5ba85b5206ce820c3e8ae78"
		"03c902f008bddc0249809ddc02ad8303c917f004c975d008bddc0249809ddc02ad83034a4a2903a8b93c8e8d7603b9408e8d7703"
		"4cca85201f8690034c4a86ad8303c926f008c948f004c993d008bddc0249019ddc02a9008d7403a9018d75034c9484ff2020408000"
		"00000001" };
	// the enemy touch code, $89ae to $89d2
	const std::string TOUCH_HEX{ "a5adf00160ad270410f5bdcc02c905d008bde40209809de402a90420e4d020d5894c798a60" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevNecronAidesControl", 0, {
		} },
		{ "AtlasDevNecronAidesControl climb=1", 19, {
			{ 14, 0x8dff, "4ccefc" },
			{ 15, 0xfcce, "b93c8e8d7603b9408e4a6e76038d77034cca85" },
		} },
		{ "AtlasDevNecronAidesControl climb=4 walk=4", 19, {
			{ 14, 0x8dff, "4ccefc" },
			{ 14, 0x8e33, "04" },
			{ 15, 0xfcce, "b93c8e8d7603b9408e0e76032a8d77034cca85" },
		} },
		{ "AtlasDevNecronAidesControl cling=1", 0, {
			{ 14, 0x89bf, "4cc789" },
		} },
		{ "AtlasDevNecronAidesControl flag=12", 0, {
		} },
		{ "AtlasDevNecronAidesControl cling=1 flag=12", 16, {
			{ 14, 0x89bf, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f0034cc789bde4024cc289" },
		} },
		{ "AtlasDevNecronAidesControl climb=4 walk=4 flag=12", 51, {
			{ 14, 0x8dff, "4ccefc" },
			{ 14, 0x8e32, "4ceefc" },
			{ 15, 0xfcce, "ad02012910f013b93c8e8d7603b9408e0e76032a8d77034cca85b93c8e4c028ead02012910f004a904d002a9018d75034c378e" },
		} },
		{ "AtlasDevNecronAidesControl walk=4 flag=12", 19, {
			{ 14, 0x8e32, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f004a904d002a9018d75034c378e" },
		} },
		{ "AtlasDevNecronAidesControl climb=1 cling=1", 19, {
			{ 14, 0x89bf, "4cc789" },
			{ 14, 0x8dff, "4ccefc" },
			{ 15, 0xfcce, "b93c8e8d7603b9408e4a6e76038d77034cca85" },
		} },
		{ "AtlasDevNecronAidesControl climb=4 walk=4 cling=1", 19, {
			{ 14, 0x89bf, "4cc789" },
			{ 14, 0x8dff, "4ccefc" },
			{ 14, 0x8e33, "04" },
			{ 15, 0xfcce, "b93c8e8d7603b9408e0e76032a8d77034cca85" },
		} },
		{ "AtlasDevNecronAidesControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x8da3, from_hex(ROUTINE_HEX));
		put(rom, 14, 0x89ae, from_hex(TOUCH_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

	std::string hex_at(const std::vector<byte>& rom, byte bank, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) { s += d[rom[off + i] >> 4]; s += d[rom[off + i] & 15]; }
		return s;
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
			require(install(rom, "AtlasDevNecronAidesControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevNecronAidesControl climb=0", "AtlasDevNecronAidesControl climb=3",
				"AtlasDevNecronAidesControl walk=0", "AtlasDevNecronAidesControl walk=5", "AtlasDevNecronAidesControl cling=2",
				"AtlasDevNecronAidesControl flag=248", "AtlasDevNecronAidesControl mode=on", "AtlasDevNecronAidesControl speed=3",
				"AtlasDevNecronAidesControl mode=vanilla climb=3" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x8e00)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNecronAidesControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla climb site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x8e32, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevNecronAidesControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x89c0)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNecronAidesControl cling=1"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "with cling=1 a non vanilla touch code is refused and nothing is written");
			auto other{ before };
			install(other, "AtlasDevNecronAidesControl");
			require(hex_at(other, 14, 0x89bf, 3) == hex_at(before, 14, 0x89bf, 3), "with cling=0 the touch code is left alone");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNecronAidesControl climb=1"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_necron_aides_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_necron_aides_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
