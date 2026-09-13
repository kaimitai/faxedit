#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevGiantBeesControl to known bytes for the default, the
// slowest climb, the fastest everything with the longest hover, a faster dive
// with the shortest hover, and a flag; the code goes in bank 15 and the sites
// in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// the giant bees' routine, $92e0 to $93ab, with all three sites
	const std::string ROUTINE_HEX{
		"2085a8d008a9009de4022094a8bce402f00688f0364c6093a9008d7603a9048d7703209c85b5c2c920b011fee402a9ff9dec029df402"
		"207b8620918660fee402a9409dec02a9009df40260bdf402f0ed201b83c90890e6a9008d7403a9018d7503202784bdf402a00220d183"
		"209c85bdf40238e9049df402b005a9009df40260207b86a9c08d7403a9008d7503202784bdf402a00420e183a00420d183a9008d7703"
		"209c85fef402bdf402290fd008bddc0249809ddc02bdf402297fd00da9009de402bddc02297f9ddc0260" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevGiantBeesControl", 0, {
		} },
		{ "AtlasDevGiantBeesControl rise=1", 0, {
			{ 14, 0x92fe, "01" },
		} },
		{ "AtlasDevGiantBeesControl rise=8 dive=4 hover=256", 0, {
			{ 14, 0x92fe, "08" },
			{ 14, 0x933d, "04" },
			{ 14, 0x939b, "ff" },
		} },
		{ "AtlasDevGiantBeesControl flag=12", 0, {
		} },
		{ "AtlasDevGiantBeesControl rise=8 dive=4 hover=256 flag=12", 59, {
			{ 14, 0x92fd, "4cd4fc" },
			{ 14, 0x933c, "4ce5fc" },
			{ 14, 0x9397, "4cf6fc" },
			{ 15, 0xfcce, "ad020129106020cefcf004a908d002a9048d77034c029320cefcf004a904d002a9018d75034c419320cefcf008bdf40229" },
			{ 15, 0xfd00, "4c9c93bdf4024c9a93" },
		} },
		{ "AtlasDevGiantBeesControl hover=256 flag=12", 21, {
			{ 14, 0x9397, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f008bdf40229" },
			{ 15, 0xfcda, "4c9c93bdf4024c9a93" },
		} },
		{ "AtlasDevGiantBeesControl rise=8 hover=256 flag=12", 40, {
			{ 14, 0x92fd, "4ccefc" },
			{ 14, 0x9397, "4ce1fc" },
			{ 15, 0xfcce, "ad02012910f004a908d002a9048d77034c0293ad02012910f008bdf40229" },
			{ 15, 0xfced, "4c9c93bdf4024c9a93" },
		} },
		{ "AtlasDevGiantBeesControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x92e0, from_hex(ROUTINE_HEX));
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
			require(install(rom, "AtlasDevGiantBeesControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevGiantBeesControl rise=0", "AtlasDevGiantBeesControl rise=9",
				"AtlasDevGiantBeesControl dive=0", "AtlasDevGiantBeesControl dive=5", "AtlasDevGiantBeesControl hover=100",
				"AtlasDevGiantBeesControl hover=512", "AtlasDevGiantBeesControl flag=248", "AtlasDevGiantBeesControl mode=on",
				"AtlasDevGiantBeesControl speed=3", "AtlasDevGiantBeesControl mode=vanilla rise=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x939b)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevGiantBeesControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla hover mask is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x933c, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevGiantBeesControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevGiantBeesControl rise=8 dive=4 hover=256 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_giant_bees_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_giant_bees_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
