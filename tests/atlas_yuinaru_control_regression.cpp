#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevYuinaruControl to known bytes for the default, a half
// pixel sideways peak, the fastest and shortest swoops, a half pixel up and
// down peak and a flag, and checks that it installs next to hornet control
namespace {
	constexpr word ORG{ 0xbdb5 };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, HORNET_SIZE{ 7 };
	const std::vector<byte> INIT_ORIG{ 0x20, 0x85, 0xa8, 0xd0, 0x0d, 0xa9, 0x00, 0x9d, 0xec, 0x02, 0xa9, 0x80, 0x9d, 0xf4, 0x02, 0x20, 0x94, 0xa8 };
	const std::vector<byte> SITE_ORIG{ 0xbd, 0xec, 0x02 };
	const std::vector<byte> BODY_ORIG{ 0xa0, 0x01, 0x20, 0xe1, 0x83, 0xa0, 0x03, 0x20, 0xc1, 0x83, 0xad, 0x75, 0x03, 0x29, 0x03, 0x8d,
		0x75, 0x03, 0x20, 0x27, 0x84, 0xfe, 0xec, 0x02, 0xbd, 0xf4, 0x02, 0xbd, 0xf4, 0x02, 0xa0, 0x02,
		0x20, 0xe1, 0x83, 0xa0, 0x03, 0x20, 0xd1, 0x83, 0xad, 0x77, 0x03, 0x29, 0x01, 0x8d, 0x77, 0x03,
		0x20, 0x9c, 0x85, 0xfe, 0xf4, 0x02, 0x60 };
	const std::vector<byte> HELPERS_ORIG{ 0x8d, 0x74, 0x03, 0xa9, 0x00, 0x0e, 0x74, 0x03, 0x2a, 0x88, 0xd0, 0xf9, 0x8d, 0x75, 0x03, 0x60,
		0x8d, 0x76, 0x03, 0xa9, 0x00, 0x0e, 0x76, 0x03, 0x2a, 0x88, 0xd0, 0xf9, 0x8d, 0x77, 0x03, 0x60,
		0x48, 0x39, 0xf7, 0x83, 0x85, 0x00, 0x68, 0x39, 0xff, 0x83, 0xf0, 0x07, 0xb9, 0xf7, 0x83, 0x38,
		0xe5, 0x00, 0x60, 0xa5, 0x00, 0x60, 0xff, 0x7f, 0x3f, 0x1f, 0x0f, 0x07, 0x03, 0x01, 0x00, 0x80,
		0x40, 0x20, 0x10, 0x08, 0x04, 0x02 };
	const std::vector<byte> MOVE_X_ORIG{ 0x20, 0x9a, 0x84, 0xb5, 0xba, 0xc9, 0xf0, 0x90, 0x19, 0xbd, 0xdc, 0x02, 0x4a, 0xb5, 0xba, 0x29,
		0xf0, 0xb0, 0x03, 0x18, 0x69, 0x10, 0x95, 0xba, 0xbd, 0xdc, 0x02, 0x49, 0x01, 0x9d, 0xdc, 0x02,
		0x38, 0x60, 0x18, 0x60 };
	const std::vector<byte> MOVE_Y_ORIG{ 0x20, 0xca, 0x85, 0x20, 0xad, 0x85, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x80, 0x9d, 0xdc, 0x02,
		0x60, 0xb5, 0xc2, 0xc9, 0xe0, 0x90, 0x10, 0xbd, 0xdc, 0x02, 0x0a, 0xb5, 0xc2, 0x29, 0xf0, 0xb0,
		0x03, 0x18, 0x69, 0x10, 0x95, 0xc2, 0x38, 0x60 };
	// hornet control's sites, for the pair
	const std::vector<byte> HN_INIT_ORIG{ 0x20, 0x85, 0xa8, 0xd0, 0x0b, 0xa9, 0x00, 0x9d, 0xe4, 0x02, 0x9d, 0xec, 0x02, 0x20, 0x94, 0xa8 };
	const std::vector<byte> HN_SITE_ORIG{ 0xa9, 0x00, 0x8d, 0x74, 0x03 };
	const std::vector<byte> HN_BODY_ORIG{ 0xa9, 0x02, 0x8d, 0x75, 0x03, 0x20, 0x19, 0x84, 0xbd, 0xec, 0x02, 0x4a, 0x4a, 0x4a, 0x29, 0x07,
		0xa8, 0xb9, 0xb0, 0x8e, 0x8d, 0x76, 0x03, 0xb9, 0xb8, 0x8e, 0x8d, 0x77, 0x03, 0x20, 0x64, 0x85,
		0xfe, 0xec, 0x02, 0x60, 0x00, 0x00, 0x80, 0x40, 0x00, 0x40, 0x80, 0x00, 0x02, 0x01, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x01 };
	const std::vector<byte> HN_MOVE_X_ORIG{ 0x20, 0x94, 0x84, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x01, 0x9d, 0xdc, 0x02, 0x60 };
	const std::vector<byte> HN_MOVE_Y_ORIG{ 0x20, 0xca, 0x85, 0x20, 0x75, 0x85, 0x90, 0x08, 0xbd, 0xdc, 0x02, 0x49, 0x80, 0x9d, 0xdc, 0x02, 0x60 };

	// yuinaru with a half-pixel peak on its 256-frame loop (which needs the rewritten movement), placed after
	// the hornet with a period of 16
	const std::string PAIR_CODE{ "8d7403a9008d750360" };

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
		{ "AtlasDevYuinaruControl", 0, {
		} },
		{ "AtlasDevYuinaruControl xspeed=4", 9, {
			{ 14, 0x9004, "b5bd" },
			{ 14, 0x900a, "00" },
			{ 14, 0xbdb5, "8d7403a9008d750360" },
		} },
		{ "AtlasDevYuinaruControl xspeed=64 xloop=8 yspeed=64 yloop=8", 0, {
			{ 14, 0x8ffd, "06" },
			{ 14, 0x9002, "09" },
			{ 14, 0x900a, "07" },
			{ 14, 0x901b, "06" },
			{ 14, 0x9020, "09" },
			{ 14, 0x9028, "07" },
		} },
		{ "AtlasDevYuinaruControl flag=12", 0, {
		} },
		{ "AtlasDevYuinaruControl xspeed=64 xloop=8 yspeed=64 yloop=8 flag=12", 64, {
			{ 14, 0x8ff9, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910d006bdec024cfc8fbdec02a00620e183a00920c183ad750329078d7503202784feec02bdf402a00620e183a00920d183ad770329078d77034c2c90" },
		} },
		{ "AtlasDevYuinaruControl xloop=8 flag=12", 26, {
			{ 14, 0x8ff9, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910d006bdec024cfc8fbdec02a00620e183a0084c0390" },
		} },
		{ "AtlasDevYuinaruControl yspeed=64 yloop=8 flag=12", 37, {
			{ 14, 0x9017, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910d006bdf4024c1a90bdf402a00620e183a00920d183ad770329078d77034c2c90" },
		} },
		{ "AtlasDevYuinaruControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 0x8fe7, INIT_ORIG); put(rom, 0x8ff9, SITE_ORIG); put(rom, 0x8ffc, BODY_ORIG);
		put(rom, 0x83c1, HELPERS_ORIG); put(rom, 0x8427, MOVE_X_ORIG); put(rom, 0x859c, MOVE_Y_ORIG);
		put(rom, 0x8e77, HN_INIT_ORIG); put(rom, 0x8e87, HN_SITE_ORIG); put(rom, 0x8e8c, HN_BODY_ORIG);
		put(rom, 0x8419, HN_MOVE_X_ORIG); put(rom, 0x8564, HN_MOVE_Y_ORIG);
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
	// the parts of the movement no build writes: an in-place build rewrites each axis's index, shift and
	// cap inside the routine itself, and each shape above is checked against the whole image anyway
	void require_untouched(const std::vector<byte>& rom, const std::string& spec) {
		require(hex_at(rom, 0x8fe7, INIT_ORIG.size()) == hex(INIT_ORIG), spec + ": the setup must stay");
		require(hex_at(rom, 0x83c1, HELPERS_ORIG.size()) == hex(HELPERS_ORIG), spec + ": the wave and shift routines must stay");
		require(hex_at(rom, 0x8427, MOVE_X_ORIG.size()) == hex(MOVE_X_ORIG), spec + ": the sideways mover must stay");
		require(hex_at(rom, 0x859c, MOVE_Y_ORIG.size()) == hex(MOVE_Y_ORIG), spec + ": the up and down mover must stay");
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
			const std::string spec{ "AtlasDevHornetControl period=16\nAtlasDevYuinaruControl xspeed=4" };
			const auto n{ install(rom, spec) };
			require(n == HORNET_SIZE + PAIR_CODE.size() / 2, "the pair: size " + std::to_string(n));
			require(hex_at(rom, 0x8e97, 3) == "4cb5bd", "the pair: the hornet's site " + hex_at(rom, 0x8e97, 3));
			require(hex_at(rom, 0x9004, 2) == "bcbd", "the pair: yuinaru's scaler call " + hex_at(rom, 0x9004, 2));
			require(hex_at(rom, 0xbdbc, PAIR_CODE.size() / 2) == PAIR_CODE, "the pair: yuinaru's code differs");
			require_untouched(rom, spec);
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevYuinaruControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevYuinaruControl xspeed=24", "AtlasDevYuinaruControl yspeed=128",
				"AtlasDevYuinaruControl xloop=512", "AtlasDevYuinaruControl yloop=4", "AtlasDevYuinaruControl flag=248",
				"AtlasDevYuinaruControl mode=on", "AtlasDevYuinaruControl speed=3",
				"AtlasDevYuinaruControl mode=vanilla xloop=3" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x83e5)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevYuinaruControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla wave routine is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 0x8ff9, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevYuinaruControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		std::cout << "atlas_yuinaru_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_yuinaru_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
