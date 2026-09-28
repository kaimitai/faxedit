#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevShadowEuraControl to known bytes for the default, the
// shortest walk, the longest walk with the shortest pause and the first throw
// off, the second throw off with the longest pause, a tuned shape with a
// longer step and the full box, and a flag; the code goes in bank 15 and the
// sites, the step table and the box in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// shadow eura's routine, $9fe3 to $a092, with its step table at $a064
	const std::string ROUTINE_HEX{
		"2085a8d01fa9069d0403a9009dfc029df4029de402a9149dec02207b862094a8a90a85fa207b86bde4024ab043fef402bdf4022907d038"
		"fefc02bdfc02c90a9005a9009dfc0248c903f004c908d0032077a068a8b964a08d7503a9008d7403209484deec02d008fee402a91e9dec"
		"0260207b86deec02d008fee402a9149dec02600000080808000008080820828cbdfc024c8e8cbddc022901a8b5ba187991a08d8403b5c2"
		"1869108d85034c2da10020" };
	const std::string TABLE_HEX{ "00000808080000080808" };
	const std::string BOX_HEX{ "08002850" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevShadowEuraControl", 0, {
		} },
		{ "AtlasDevShadowEuraControl walk=1", 0, {
			{ 14, 0x9ff9, "01" },
			{ 14, 0xa05f, "01" },
		} },
		{ "AtlasDevShadowEuraControl walk=255 pause=16 fire1=0", 0, {
			{ 14, 0x9ff9, "ff" },
			{ 14, 0xa02b, "ff" },
			{ 14, 0xa04e, "10" },
			{ 14, 0xa05f, "ff" },
		} },
		{ "AtlasDevShadowEuraControl fire2=0 pause=255", 0, {
			{ 14, 0xa02f, "ff" },
			{ 14, 0xa04e, "ff" },
		} },
		{ "AtlasDevShadowEuraControl walk=10 pause=60 fire1=2 fire2=6 step=12 body=1", 0, {
			{ 14, 0x9ff9, "0a" },
			{ 14, 0xa02b, "02" },
			{ 14, 0xa02f, "06" },
			{ 14, 0xa04e, "3c" },
			{ 14, 0xa05f, "0a" },
			{ 14, 0xa066, "0c0c0c" },
			{ 14, 0xa06b, "0c0c0c" },
			{ 14, 0xb33f, "00f83058" },
		} },
		{ "AtlasDevShadowEuraControl flag=12", 0, {
		} },
		{ "AtlasDevShadowEuraControl walk=10 pause=60 fire1=2 fire2=6 step=12 body=1 flag=12", 90, {
			{ 14, 0x9ff8, "4cd4fc" },
			{ 14, 0xa029, "4ce5fc" },
			{ 14, 0xa04d, "4c06fd" },
			{ 14, 0xa05e, "4c17fd" },
			{ 14, 0xa066, "0c0c0c" },
			{ 14, 0xa06b, "0c0c0c" },
			{ 14, 0xb33f, "00f83058" },
			{ 15, 0xfcce, "ad020129106020cefcf004a90ad002a9149dec024cfd9f4820cefcd00b6848c903f00fc9084cfefc6848c902f004c906d0034c32a04c35a020cefcf004a93cd002a91e9dec024c52a020cefcf004a90ad002a9149dec024c63a0" },
		} },
		{ "AtlasDevShadowEuraControl fire1=1 flag=12", 35, {
			{ 14, 0xa029, "4ccefc" },
			{ 15, 0xfcce, "48ad02012910d00b6848c903f00fc9084ce9fc6848c901f004c908d0034c32a04c35a0" },
		} },
		{ "AtlasDevShadowEuraControl fire1=1 pause=255 flag=12", 54, {
			{ 14, 0xa029, "4ccefc" },
			{ 14, 0xa04d, "4cf1fc" },
			{ 15, 0xfcce, "48ad02012910d00b6848c903f00fc9084ce9fc6848c901f004c908d0034c32a04c35a0ad02012910f004a9" },
			{ 15, 0xfcfa, "d002a91e9dec024c52a0" },
		} },
		{ "AtlasDevShadowEuraControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9fe3, from_hex(ROUTINE_HEX));
		put(rom, 14, 0xb33f, from_hex(BOX_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

}

int main() {
	try {
		require(ROUTINE_HEX.size() == 0xb0 * 2 && ROUTINE_HEX.substr(0x102, 0x14) == TABLE_HEX,
			"the routine is 176 bytes with the step table at $a064");
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
			require(install(rom, "AtlasDevShadowEuraControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevShadowEuraControl walk=0", "AtlasDevShadowEuraControl walk=256",
				"AtlasDevShadowEuraControl pause=15", "AtlasDevShadowEuraControl pause=256",
				"AtlasDevShadowEuraControl fire1=10", "AtlasDevShadowEuraControl fire2=10",
				"AtlasDevShadowEuraControl step=0", "AtlasDevShadowEuraControl step=17",
				"AtlasDevShadowEuraControl body=2", "AtlasDevShadowEuraControl flag=248",
				"AtlasDevShadowEuraControl mode=on", "AtlasDevShadowEuraControl speed=3",
				"AtlasDevShadowEuraControl mode=vanilla walk=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0xa02a)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevShadowEuraControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla fire site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0xa05e, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevShadowEuraControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0xa066)] = 0x07;   // a step table someone else changed
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevShadowEuraControl step=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a changed step table is refused with step set and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0xb33f)] = 0x05;   // a box someone else changed
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevShadowEuraControl body=1"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a changed box is refused with body=1 and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevShadowEuraControl walk=10 pause=60 fire1=2 fire2=6 step=12 body=1 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_shadow_eura_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_shadow_eura_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
