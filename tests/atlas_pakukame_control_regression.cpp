#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevPakukameControl to known bytes for the default, the
// shortest wait, the longest wait with the most liliths and the slowest
// windup, the fewest liliths with the fastest windup and a flag; the code goes
// in bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// pakukame's routine, $9cf2 to $9da4, with the three sites
	const std::string ROUTINE_HEX{
		"2085a8d010a9009de4029dec02a9049d04032094a8bde402c901b01dfeec02bdec02c94090122036a2b00d20789db008fee402a9"
		"1220e4d060ad83032907d012fee402bde402c9059009a9009de4029dec0260c903d0fb2036a2b0f6b5c2691099c200b5ba6908"
		"99ba00a99e992c03a90099dc02a90999cc02aabda9b5994403ae78034c02a2a007a9008500b9cc02c909d00ae600a500c90390"
		"0238608810ec1860a000bde402f004bce40288b9a59d4c8e8c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevPakukameControl", 0, {
		} },
		{ "AtlasDevPakukameControl delay=1", 0, {
			{ 14, 0x9d15, "01" },
		} },
		{ "AtlasDevPakukameControl delay=255 cap=8 windup=16", 0, {
			{ 14, 0x9d15, "ff" },
			{ 14, 0x9d2f, "0f" },
			{ 14, 0x9d8a, "08" },
		} },
		{ "AtlasDevPakukameControl flag=12", 0, {
		} },
		{ "AtlasDevPakukameControl delay=255 cap=8 windup=16 flag=12", 63, {
			{ 14, 0x9d11, "4cd4fc" },
			{ 14, 0x9d2b, "4ce7fc" },
			{ 14, 0x9d87, "4cfafc" },
			{ 15, 0xfcce, "ad020129106020cefcf008bdec02c9" },
			{ 15, 0xfcde, "4c169dbdec024c149d20cefcf008ad8303290f4c309dad83034c2e9d20cefcf007a500c9084c8b9da500c9034c8b9d" },
		} },
		{ "AtlasDevPakukameControl cap=8 flag=12", 21, {
			{ 14, 0x9d87, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f007a500c9084c8b9da500c9034c8b9d" },
		} },
		{ "AtlasDevPakukameControl cap=8 windup=16 flag=12", 42, {
			{ 14, 0x9d2b, "4ccefc" },
			{ 14, 0x9d87, "4ce3fc" },
			{ 15, 0xfcce, "ad02012910f008ad8303290f4c309dad83034c2e9dad02012910f007a500c9084c8b9da500c9034c8b9d" },
		} },
		{ "AtlasDevPakukameControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9cf2, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevPakukameControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevPakukameControl delay=0", "AtlasDevPakukameControl delay=256",
				"AtlasDevPakukameControl cap=0", "AtlasDevPakukameControl cap=9", "AtlasDevPakukameControl windup=3",
				"AtlasDevPakukameControl flag=248", "AtlasDevPakukameControl mode=on", "AtlasDevPakukameControl speed=3",
				"AtlasDevPakukameControl mode=vanilla delay=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9d15)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevPakukameControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla wait test is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9d87, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevPakukameControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevPakukameControl delay=255 cap=8 windup=16 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_pakukame_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_pakukame_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
