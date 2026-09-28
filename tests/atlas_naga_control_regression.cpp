#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevNagaControl to known bytes for the default, the slowest
// creep, the fastest creep with no gap ignored, the widest gap and a flag; the
// code goes in bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// naga's routine, $93e5 to $9441, with both sites, and the gap helper it calls
	const std::string ROUTINE_HEX{
		"2085a8d00ba9009df4029de4022094a8207b86bdf402a00420e183a00520d183ad770329018d770320c485fef402"
		"bdf402291fd008bddc0249809ddc02201b83c9109018bddc0248209186a9008d7703a9c08d760320c485689ddc0260" };
	const std::string GAP_HEX{ "b5c238e5a1b00649ff1869011860" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevNagaControl", 0, {
		} },
		{ "AtlasDevNagaControl chase=1", 0, {
			{ 14, 0x9436, "20" },
		} },
		{ "AtlasDevNagaControl chase=64 zone=0", 0, {
			{ 14, 0x9426, "00" },
			{ 14, 0x9431, "08" },
			{ 14, 0x9436, "00" },
		} },
		{ "AtlasDevNagaControl flag=12", 0, {
		} },
		{ "AtlasDevNagaControl chase=64 zone=0 flag=12", 44, {
			{ 14, 0x9422, "4ccefc" },
			{ 14, 0x9430, "4ce3fc" },
			{ 15, 0xfcce, "ad02012910f008201b83c9004c2794201b834c2594ad02012910f00aa9088d7703a9004c37948d77034c3594" },
		} },
		{ "AtlasDevNagaControl zone=0 flag=12", 21, {
			{ 14, 0x9422, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f008201b83c9004c2794201b834c2594" },
		} },
		{ "AtlasDevNagaControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x93e5, from_hex(ROUTINE_HEX));
		put(rom, 14, 0x831b, from_hex(GAP_HEX));
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
			require(install(rom, "AtlasDevNagaControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevNagaControl chase=0", "AtlasDevNagaControl chase=65",
				"AtlasDevNagaControl zone=256", "AtlasDevNagaControl flag=248", "AtlasDevNagaControl mode=on",
				"AtlasDevNagaControl speed=3", "AtlasDevNagaControl mode=vanilla chase=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9426)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNagaControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla gap check is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9430, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevNagaControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevNagaControl chase=64 zone=0 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_naga_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_naga_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
