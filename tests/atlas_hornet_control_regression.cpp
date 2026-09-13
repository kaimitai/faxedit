#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevHornetControl to known bytes for the default, flat and
// still, the fastest and laziest, the quickest wave and a flag, and checks
// that it installs next to maskman control
namespace {
	constexpr word ORG{ 0xbdb5 };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, MASKMAN_SIZE{ 34 };
	const std::vector<byte> INIT_ORIG{ 0x20, 0x85, 0xa8, 0xd0, 0x0b, 0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x9d, 0xec, 0x02, 0x20, 0x94, 0xa8 };
	const std::vector<byte> SITE_ORIG{ 0xa9, 0x00, 0x8d, 0x74, 0x03 };
	const std::vector<byte> BODY_ORIG{ 0xa9, 0x02, 0x8d, 0x75, 0x03, 0x20, 0x19, 0x84, 0xbd, 0xec, 0x02, 0x4a, 0x4a, 0x4a, 0x29, 0x07,
		0xa8, 0xb9, 0xb0, 0x8e, 0x8d, 0x76, 0x03, 0xb9, 0xb8, 0x8e, 0x8d, 0x77, 0x03, 0x20, 0x64, 0x85,
		0xfe, 0xec, 0x02, 0x60, 0x00, 0x00, 0x80, 0x40, 0x00, 0x40, 0x80, 0x00, 0x02, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01 };
	const std::vector<byte> MOVE_X_ORIG{ 0x20, 0x94, 0x84, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x01, 0x9d, 0xdc, 0x02, 0x60 };
	const std::vector<byte> MOVE_Y_ORIG{ 0x20, 0xca, 0x85, 0x20, 0x75, 0x85, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x80, 0x9d, 0xdc, 0x02, 0x60 };
	// maskman control's sites, for the pair
	const std::vector<byte> MM_DISPATCH_ORIG{ 0xbd, 0xcc, 0x02, 0xc9, 0x1f, 0xf0, 0x08, 0xc9, 0x20, 0xf0, 0x16, 0xc9, 0x21, 0xd0, 0x23 };
	const std::vector<byte> MM_BRANCH_ORIG{ 0xbd, 0xe4, 0x02, 0xf0, 0x04, 0xc9, 0x04, 0xd0, 0x08 };
	const std::vector<byte> MM_SITE_ORIG{ 0xa9, 0x75, 0x85, 0x02, 0xa9, 0x8a, 0x85, 0x03 };
	const std::vector<byte> MM_TAIL_ORIG{ 0xa0, 0x00, 0xb5, 0xba, 0x18, 0x71, 0x02, 0x8d, 0xe2, 0x03, 0xb5, 0xc2, 0x18, 0xc8, 0x71, 0x02,
		0x8d, 0xe3, 0x03, 0xc8, 0xb1, 0x02, 0x8d, 0xe4, 0x03, 0xc8, 0xb1, 0x02, 0x8d, 0xe5, 0x03, 0x60 };
	const std::vector<byte> MM_SPEAR_RECORD_ORIG{ 0x20, 0x10, 0xf8, 0x00 };
	const std::vector<byte> MM_BODY_RECORD_ORIG{ 0x00, 0x00, 0x10, 0x20 };

	// the hornet with a period of 16 (which needs the rewritten routine), placed after maskman control
	const std::string PAIR_CODE{ "4a4a4a4a4c9a8e" };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevHornetControl", 0, {
		} },
		{ "AtlasDevHornetControl speed=0 bob=0", 0, {
			{ 14, 0x8e8d, "00" },
			{ 14, 0x8eb2, "0000" },
			{ 14, 0x8eb5, "0000" },
			{ 14, 0x8eb8, "0000" },
			{ 14, 0x8ebf, "00" },
		} },
		{ "AtlasDevHornetControl speed=64 bob=64 period=32", 0, {
			{ 14, 0x8e8d, "08" },
			{ 14, 0x8e9a, "4a4a" },
			{ 14, 0x8eb2, "0000" },
			{ 14, 0x8eb5, "0000" },
			{ 14, 0x8eb8, "08040201" },
			{ 14, 0x8ebd, "010204" },
		} },
		{ "AtlasDevHornetControl period=1", 0, {
			{ 14, 0x8e97, "eaeaea" },
		} },
		{ "AtlasDevHornetControl period=16", 7, {
			{ 14, 0x8e97, "4cb5bd" },
			{ 14, 0xbdb5, "4a4a4a4a4c9a8e" },
		} },
		{ "AtlasDevHornetControl flag=12", 0, {
		} },
		{ "AtlasDevHornetControl speed=64 bob=64 period=32 flag=12", 63, {
			{ 14, 0x8e87, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910d0068d74034c8c8ea9008d7403a9088d7503201984bdec024a4a4a4a4aa8b9e4bd8d7603b9ecbd4ca68e00000000000000000804020100010204" },
		} },
		{ "AtlasDevHornetControl speed=64 flag=12", 26, {
			{ 14, 0x8e87, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910d0068d74034c8c8ea9008d7403a9088d75034c918e" },
		} },
		{ "AtlasDevHornetControl speed=64 period=32 flag=12", 38, {
			{ 14, 0x8e87, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910d0068d74034c8c8ea9008d7403a9088d7503201984bdec024a4a4a4a4aa84c9d8e" },
		} },
		{ "AtlasDevHornetControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 0x8e77, INIT_ORIG); put(rom, 0x8e87, SITE_ORIG); put(rom, 0x8e8c, BODY_ORIG);
		put(rom, 0x8419, MOVE_X_ORIG); put(rom, 0x8564, MOVE_Y_ORIG);
		put(rom, 0x8a1f, MM_DISPATCH_ORIG); put(rom, 0x8a40, MM_BRANCH_ORIG); put(rom, 0x8a49, MM_SITE_ORIG);
		put(rom, 0x8a51, MM_TAIL_ORIG); put(rom, 0x8a75, MM_SPEAR_RECORD_ORIG); put(rom, 0xb2f3, MM_BODY_RECORD_ORIG);
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

	// what must stay as it was around the site
	// the parts of the hornet no build writes: an in-place build rewrites the speeds, the shifts and the
	// tables inside the routine itself, and each shape above is checked against the whole image anyway
	void require_untouched(const std::vector<byte>& rom, const std::string& spec) {
		require(hex_at(rom, 0x8e77, INIT_ORIG.size()) == hex(INIT_ORIG), spec + ": the setup must stay");
		require(hex_at(rom, 0x8e8a, 2) == "7403", spec + ": the two bytes after the jump must stay");
		require(hex_at(rom, 0x8419, MOVE_X_ORIG.size()) == hex(MOVE_X_ORIG), spec + ": the sideways mover must stay");
		require(hex_at(rom, 0x8564, MOVE_Y_ORIG.size()) == hex(MOVE_Y_ORIG), spec + ": the up and down mover must stay");
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
			auto rom{ vanilla_rom() };
			const std::string spec{ "AtlasDevMaskmanControl\nAtlasDevHornetControl period=16" };
			const auto n{ install(rom, spec) };
			require(n == MASKMAN_SIZE + PAIR_CODE.size() / 2, "the pair: size " + std::to_string(n));
			require(hex_at(rom, 0x8a49, 3) == "4cb5bd", "the pair: maskman's site " + hex_at(rom, 0x8a49, 3));
			require(hex_at(rom, 0x8e97, 3) == "4cd7bd", "the pair: the hornet's site " + hex_at(rom, 0x8e97, 3));
			require(hex_at(rom, 0xbdd7, PAIR_CODE.size() / 2) == PAIR_CODE, "the pair: the hornet's code differs");
			require_untouched(rom, spec);
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevHornetControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevHornetControl speed=65", "AtlasDevHornetControl bob=65",
				"AtlasDevHornetControl period=3", "AtlasDevHornetControl period=64", "AtlasDevHornetControl flag=248",
				"AtlasDevHornetControl mode=on", "AtlasDevHornetControl wobble=3",
				"AtlasDevHornetControl mode=vanilla period=3" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x8e95)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevHornetControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla routine is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 0x8e87, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevHornetControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		std::cout << "atlas_hornet_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_hornet_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
