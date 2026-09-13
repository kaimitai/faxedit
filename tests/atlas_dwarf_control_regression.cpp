#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevDwarfControl (AtlasDevSirGawaineControl and
// AtlasDevWolfmanControl) to known bytes: the 332 bytes of code are the same
// for every configuration, only the two 14 byte rows differ
namespace {
	constexpr word ORG{ 0xbdb5 }, GATE{ 0xbeb2 }, ROWS{ 0xbf01 };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, CODE_SIZE{ 332 }, ROWS_SIZE{ 28 };
	const std::vector<byte> FORK_ORIG{ 0x20, 0xf8, 0x82 };
	const std::vector<byte> GATE_ORIG{ 0xa5, 0xad, 0xf0, 0x01, 0x60 };
	const std::vector<byte> CONT_ORIG{ 0xad, 0x27, 0x04, 0x10, 0xf5 };
	const std::vector<byte> DEAD_ORIG{ 0xc9, 0x18, 0xf0, 0x1c, 0x90, 0x1b, 0x20, 0x7b, 0x86, 0xa9, 0x01, 0x8d, 0x75, 0x03, 0xa9, 0x00, 0x8d, 0x74, 0x03, 0x20, 0x94, 0x84, 0xad, 0x83, 0x03, 0x29, 0x08, 0xf0, 0x03, 0xfe, 0xe4, 0x02, 0x60, 0xa9, 0x01, 0x9d, 0xe4, 0x02, 0xa5, 0xa4, 0x29, 0x01, 0xd0, 0x13, 0x20, 0x7b, 0x86, 0xa0, 0x00, 0xad, 0x83, 0x03, 0x29, 0x10, 0xf0, 0x02, 0xa0, 0x02, 0x98, 0x9d, 0xe4, 0x02, 0x60 };
	const std::vector<byte> RUSH_ORIG{ 0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x20, 0x7b, 0x86, 0xa9, 0x02, 0x8d, 0x75, 0x03, 0xa9, 0x00, 0x8d, 0x74, 0x03, 0x20, 0x94, 0x84, 0xad, 0x83, 0x03, 0x29, 0x04, 0xd0, 0x03, 0xfe, 0xe4, 0x02, 0x60 };

	const std::string EXPECTED_CODE{
		"2091beb90ebff017b90dbfa8b90101482091be68390ebfd00620f8824cb49420f882482091bebdec02f00ed901bf9009"
		"d905bfb004684c2ebe68d907bff01a9019a9009dec02207b86b908bf209dbead83032908f003fee40260a5a42901f019"
		"a9009de402207b86b909bf209dbead83032904d0e4fee40260207b86bd4c03f005a9009dec02feec02bdec02d904bf90"
		"05a9009dec02bdec02d901bf9038d902bf901dd903bfb02ea9029de402b90bbff02920f882f0242091beb90bbf4c9dbe"
		"a9009de402b90abff01120a9beb90abf209dbe4ca9bea9009de40260a000bdcc02c921d002a00e608d7503a9008d7403"
		"20948460bddc0249019ddc0260a5add047bdcc02c91ff004c921d03d2091beb90ebff014b90dbfa8b90101482091be68"
		"390ebfd0034cb389b906bfd01cbde402c902d014b90cbff01020f882482091be68d90cbf9003f001604cb389" };

	struct Shape { const char* spec; const char* rows; };
	const std::vector<Shape> SHAPES{
		{ "AtlasDevSirGawaineControl",
			"0c18243c240018010201021000001010202010011801020000000000" },
		{ "AtlasDevWolfmanControl",
			"10102020100118010200000000000c18243c24001801020102100000" },
		{ "AtlasDevSirGawaineControl\nAtlasDevWolfmanControl",
			"0c18243c240018010201021000000c18243c24001801020102100000" },
		{ "AtlasDevWolfmanControl\nAtlasDevSirGawaineControl",
			"0c18243c240018010201021000000c18243c24001801020102100000" },
		{ "AtlasDevSirGawaineControl bodyhurt=1\nAtlasDevWolfmanControl bodyhurt=1",
			"0c18243c240118010201021000000c18243c24011801020102100000" },
		{ "AtlasDevSirGawaineControl flag=12\nAtlasDevWolfmanControl flag=247 sword=0",
			"0c18243c240018010201021001100c18243c24001801020102001e80" },
		{ "AtlasDevSirGawaineControl windup=8\nAtlasDevWolfmanControl windup=30 tell=4 back=3 lunge=0 swing=40 recover=100 reach=40 approach=2 chase=0",
			"0008142c140018010201021000001a1e46aa46002802000300100000" },
		{ "AtlasDevSirGawaineControl\nAtlasDevWolfmanControl mode=vanilla",
			"0c18243c240018010201021000001010202010011801020000000000" },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 0x94b1, FORK_ORIG); put(rom, 0x89ae, GATE_ORIG); put(rom, 0x89b3, CONT_ORIG);
		put(rom, 0x94b4, DEAD_ORIG); put(rom, 0x94f3, RUSH_ORIG);
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(14, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 14, ORG, 0xc000, hacks, nullptr);
	}

	std::string hex_at(const std::vector<byte>& rom, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) { s += d[rom[off + i] >> 4]; s += d[rom[off + i] & 15]; }
		return s;
	}

	std::string hex(const std::vector<byte>& bytes) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		for (byte b : bytes) { s += d[b >> 4]; s += d[b & 15]; }
		return s;
	}
}

int main() {
	try {
		for (const auto& shape : SHAPES) {
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, shape.spec) };
			const std::string spec{ shape.spec };
			require(n == CODE_SIZE + ROWS_SIZE, spec + ": size " + std::to_string(n));
			require(hex_at(rom, ORG, CODE_SIZE) == EXPECTED_CODE, spec + ": code differs: " + hex_at(rom, ORG, CODE_SIZE));
			require(hex_at(rom, ROWS, ROWS_SIZE) == shape.rows, spec + ": rows differ: " + hex_at(rom, ROWS, ROWS_SIZE));
			require(hex_at(rom, 0x94b1, 3) == "4cb5bd", spec + ": fork site " + hex_at(rom, 0x94b1, 3));
			require(hex_at(rom, 0x89ae, 5) == "4cb2beeaea", spec + ": gate site " + hex_at(rom, 0x89ae, 5));
			// a clear flag falls back to these, so they stay vanilla
			require(hex_at(rom, 0x94b4, DEAD_ORIG.size()) == hex(DEAD_ORIG), spec + ": the fallback at $94b4 must stay");
			require(hex_at(rom, 0x94f3, RUSH_ORIG.size()) == hex(RUSH_ORIG), spec + ": the rush path must stay");
			require(hex_at(rom, 0x89b3, CONT_ORIG.size()) == hex(CONT_ORIG), spec + ": the touch continuation must stay");
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevSirGawaineControl mode=vanilla\nAtlasDevWolfmanControl mode=vanilla") == 0,
				"vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevSirGawaineControl windup=0", "AtlasDevSirGawaineControl swing=0",
				"AtlasDevSirGawaineControl swing=256", "AtlasDevSirGawaineControl recover=256",
				"AtlasDevSirGawaineControl reach=0", "AtlasDevSirGawaineControl approach=4",
				"AtlasDevSirGawaineControl chase=4", "AtlasDevSirGawaineControl bodyhurt=2",
				"AtlasDevSirGawaineControl tell=25", "AtlasDevSirGawaineControl windup=8 tell=9",
				"AtlasDevSirGawaineControl back=4", "AtlasDevSirGawaineControl lunge=4",
				"AtlasDevSirGawaineControl sword=256", "AtlasDevSirGawaineControl flag=248",
				"AtlasDevSirGawaineControl windup=200 swing=60", "AtlasDevSirGawaineControl mode=on",
				"AtlasDevWolfmanControl mode=vanilla flag=248", "AtlasDevWolfmanControl push=1",
				"AtlasDevSirGawaineControl\nAtlasDevWolfmanControl tell=30" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x94c0)] ^= 0x01;
			bool threw{ false };
			try { install(rom, "AtlasDevSirGawaineControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a non vanilla fallback is refused");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 0x94b1, { 0x4c, 0x00, 0x90 });   // something else owns the fork
			bool threw{ false };
			try { install(rom, "AtlasDevWolfmanControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a fork patched by something else is refused");
		}
		std::cout << "atlas_dwarf_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_dwarf_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
