#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevMaskmanControl to known bytes for the default, both
// ends of spear and a flag, and checks that it installs next to sir gawaine
// control
namespace {
	constexpr word ORG{ 0xbdb5 };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, DWARF_SIZE{ 360 };
	const std::vector<byte> DISPATCH_ORIG{ 0xbd, 0xcc, 0x02, 0xc9, 0x1f, 0xf0, 0x08, 0xc9, 0x20, 0xf0, 0x16, 0xc9, 0x21, 0xd0, 0x23 };
	const std::vector<byte> BRANCH_ORIG{ 0xbd, 0xe4, 0x02, 0xf0, 0x04, 0xc9, 0x04, 0xd0, 0x08 };
	const std::vector<byte> SITE_ORIG{ 0xa9, 0x75, 0x85, 0x02, 0xa9, 0x8a, 0x85, 0x03 };
	const std::vector<byte> TAIL_ORIG{ 0xa0, 0x00, 0xb5, 0xba, 0x18, 0x71, 0x02, 0x8d, 0xe2, 0x03, 0xb5, 0xc2, 0x18, 0xc8, 0x71, 0x02,
		0x8d, 0xe3, 0x03, 0xc8, 0xb1, 0x02, 0x8d, 0xe4, 0x03, 0xc8, 0xb1, 0x02, 0x8d, 0xe5, 0x03, 0x60 };
	const std::vector<byte> SPEAR_RECORD_ORIG{ 0x20, 0x10, 0xf8, 0x00 };
	const std::vector<byte> BODY_RECORD_ORIG{ 0x00, 0x00, 0x10, 0x20 };
	// sir gawaine control's sites, for the pair
	const std::vector<byte> FORK_ORIG{ 0x20, 0xf8, 0x82 };
	const std::vector<byte> GATE_ORIG{ 0xa5, 0xad, 0xf0, 0x01, 0x60 };
	const std::vector<byte> CONT_ORIG{ 0xad, 0x27, 0x04, 0x10, 0xf5 };
	const std::vector<byte> DEAD_ORIG{ 0xc9, 0x18, 0xf0, 0x1c, 0x90, 0x1b, 0x20, 0x7b, 0x86, 0xa9, 0x01, 0x8d, 0x75, 0x03, 0xa9, 0x00, 0x8d, 0x74, 0x03, 0x20, 0x94, 0x84, 0xad, 0x83, 0x03, 0x29, 0x08, 0xf0, 0x03, 0xfe, 0xe4, 0x02, 0x60, 0xa9, 0x01, 0x9d, 0xe4, 0x02, 0xa5, 0xa4, 0x29, 0x01, 0xd0, 0x13, 0x20, 0x7b, 0x86, 0xa0, 0x00, 0xad, 0x83, 0x03, 0x29, 0x10, 0xf0, 0x02, 0xa0, 0x02, 0x98, 0x9d, 0xe4, 0x02, 0x60 };
	const std::vector<byte> RUSH_ORIG{ 0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x20, 0x7b, 0x86, 0xa9, 0x02, 0x8d, 0x75, 0x03, 0xa9, 0x00, 0x8d, 0x74, 0x03, 0x20, 0x94, 0x84, 0xad, 0x83, 0x03, 0x29, 0x04, 0xd0, 0x03, 0xfe, 0xe4, 0x02, 0x60 };

	const std::string DEFAULT_CODE{ "bddc024ab5bab00738e910b002a9008de203b5c28de303a9208de403a9208de50360" };

	struct Shape { const char* spec; std::string code; };
	const std::vector<Shape> SHAPES{
		{ "AtlasDevMaskmanControl", DEFAULT_CODE },
		{ "AtlasDevMaskmanControl spear=0", "bddc024ab5bab00738e900b002a9008de203b5c28de303a9108de403a9208de50360" },
		{ "AtlasDevMaskmanControl spear=64", "bddc024ab5bab00738e940b002a9008de203b5c28de303a9508de403a9208de50360" },
		{ "AtlasDevMaskmanControl flag=12", "ad02012910d00ba9758502a98a85034c518a" + DEFAULT_CODE },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 0x8a1f, DISPATCH_ORIG); put(rom, 0x8a40, BRANCH_ORIG); put(rom, 0x8a49, SITE_ORIG);
		put(rom, 0x8a51, TAIL_ORIG); put(rom, 0x8a75, SPEAR_RECORD_ORIG); put(rom, 0xb2f3, BODY_RECORD_ORIG);
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

	// what must stay as it was around the site
	void require_untouched(const std::vector<byte>& rom, const std::string& spec) {
		require(hex_at(rom, 0x8a4c, 5) == "02a98a8503", spec + ": the five bytes after the jump must stay");
		require(hex_at(rom, 0x8a51, TAIL_ORIG.size()) == hex(TAIL_ORIG), spec + ": the tail must stay");
		require(hex_at(rom, 0x8a75, 4) == hex(SPEAR_RECORD_ORIG), spec + ": the spear record must stay");
		require(hex_at(rom, 0xb2f3, 4) == hex(BODY_RECORD_ORIG), spec + ": the body record must stay");
	}
}

int main() {
	try {
		for (const auto& shape : SHAPES) {
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, shape.spec) };
			const std::string spec{ shape.spec };
			require(n == shape.code.size() / 2, spec + ": size " + std::to_string(n));
			require(hex_at(rom, ORG, n) == shape.code, spec + ": code differs: " + hex_at(rom, ORG, n));
			require(hex_at(rom, 0x8a49, 3) == "4cb5bd", spec + ": site " + hex_at(rom, 0x8a49, 3));
			require_untouched(rom, spec);
		}
		{
			auto rom{ vanilla_rom() };
			const std::string spec{ "AtlasDevSirGawaineControl\nAtlasDevMaskmanControl" };
			const auto n{ install(rom, spec) };
			require(n == DWARF_SIZE + DEFAULT_CODE.size() / 2, "the pair: size " + std::to_string(n));
			require(hex_at(rom, 0x94b1, 3) == "4cb5bd", "the pair: sir gawaine's fork " + hex_at(rom, 0x94b1, 3));
			require(hex_at(rom, 0x8a49, 3) == "4c1dbf", "the pair: maskman's site " + hex_at(rom, 0x8a49, 3));
			require(hex_at(rom, 0xbf1d, DEFAULT_CODE.size() / 2) == DEFAULT_CODE, "the pair: maskman's code differs");
			require_untouched(rom, spec);
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevMaskmanControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevMaskmanControl spear=65", "AtlasDevMaskmanControl flag=248",
				"AtlasDevMaskmanControl mode=on", "AtlasDevMaskmanControl reach=3",
				"AtlasDevMaskmanControl mode=vanilla spear=65" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x8a4e)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevMaskmanControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 0x8a49, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevMaskmanControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		std::cout << "atlas_maskman_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_maskman_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
