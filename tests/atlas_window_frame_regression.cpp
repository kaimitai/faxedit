#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// pins install_AtlasDevWindowFrame: no parameters keep the frame, a band with
// each corner shape writes its six tiles, a tile parameter replaces one tile, and
// nothing outside the 96 frame bytes changes. the frame here is made-up bytes
// with the original frame's crc (the hack checks the crc, not the bytes), so no
// game data is in the test. bad parameters and a changed frame are refused and
// leave the rom as it was.
namespace {
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr byte BANK{ 10 };
	constexpr word FRAME{ 0x8150 };
	constexpr word ORG{ 0xfcce };

	const std::string STAND_IN{
		"0b30557a9fc4e90e33587da2c7ec11365b80a5caef14395e83a8cdf2173c6186abd0f51a3f6489aed3f81d42678cb1d6"
		"fb20456a8fb4d9fe23486d92b7dc01264b7095badf04294e7398bde2072c51769bc0e50a2f54799ec3e80d32a99a285e" };
	// band=0123 with each corner shape, band=0203 cut and band=0003 rounded and diamond
	const std::string BAND_SQUARE{
		"007f405f5f585b5a00003f3f3f3f3c3c00ff00ffff00ff000000ffffffff000000fe02fafa1ada5a0000fcfcfcfc3c3c"
		"5a5a5a5a5a5a5a5a3c3c3c3c3c3c3c3c5a5b585f5f407f003c3c3f3f3f3f00005ada1afafa02fe003c3cfcfcfcfc0000" };
	const std::string LINE_ROUNDED{
		"000000070f1c1818000000070f1c1818000000ffff000000000000ffff000000000000e0f0381818000000e0f0381818"
		"1818181818181818181818181818181818181c0f0700000018181c0f07000000181838f0e0000000181838f0e0000000" };
	const std::string DOUBLE_CUT{
		"0000001f1f181818001f205f5f585b5a000000ffff00000000ff00ffff00ff00000000f8f818181800f804fafa1ada5a"
		"18181818181818185a5a5a5a5a5a5a5a1818181f1f0000005a5b585f5f201f00181818f8f80000005ada1afafa04f800" };
	const std::string LINE_DIAMOND{
		"0000001d1d1800180018245f5f3c1818000000ffff000000000000ffff000000000000b8b8180018001824fafa3c1818"
		"181818181818181818181818181818181800181d1d00000018183c5f5f241800180018b8b800000018183cfafa241800" };

	void require(bool p_ok, const std::string& p_what) {
		if (!p_ok)
			throw std::runtime_error(p_what);
	}

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	std::vector<byte> rom_with(const std::string& p_frame) {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		const byte header[]{ 'N', 'E', 'S', 0x1a, 16, 0, 0x12, 0 };
		for (std::size_t i{ 0 }; i < sizeof header; ++i) rom[i] = header[i];
		const auto frame{ from_hex(p_frame) };
		const auto off{ klib::Asm6502::get_file_offset(BANK, FRAME) };
		for (std::size_t i{ 0 }; i < frame.size(); ++i) rom[off + i] = frame[i];
		return rom;
	}

	void install(std::vector<byte>& p_rom, const std::string& p_spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(p_spec)) };
		fh::HackManager{}.install_general_hacks(fe::Config{}, p_rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

	// the rom after the install is the original with the frame replaced, and no other byte
	void expect(const std::string& p_spec, const std::string& p_frame) {
		auto rom{ rom_with(STAND_IN) };
		install(rom, p_spec);
		const auto want{ rom_with(p_frame) };
		for (std::size_t i{ 0 }; i < rom.size(); ++i)
			require(rom[i] == want[i], p_spec + ": file offset " + std::to_string(i) + " differs from the expected install");
	}

	void refused(const std::string& p_spec, const std::string& p_frame, const std::string& p_why) {
		auto rom{ rom_with(p_frame) };
		const auto before{ rom };
		bool threw{ false };
		try { install(rom, p_spec); } catch (const std::exception&) { threw = true; }
		require(threw, p_spec + ": accepted, but " + p_why);
		require(rom == before, p_spec + ": refused but changed the rom");
	}
}

int main() {
	try {
		expect("AtlasDevWindowFrame", STAND_IN);
		expect("AtlasDevWindowFrame band=0123", BAND_SQUARE);
		expect("AtlasDevWindowFrame band=0123 corner=square", BAND_SQUARE);
		expect("AtlasDevWindowFrame band=0003 corner=rounded", LINE_ROUNDED);
		expect("AtlasDevWindowFrame band=0203 corner=cut", DOUBLE_CUT);
		expect("AtlasDevWindowFrame band=0003 corner=diamond", LINE_DIAMOND);
		expect("AtlasDevWindowFrame band=0003 corner=diamond diamond=2", LINE_DIAMOND);
		// each style is the band and corner it names
		for (const auto& [style, band] : std::vector<std::pair<std::string, std::string>>{
			{ "thin", "band=0030" }, { "bevel", "band=0123" }, { "inset", "band=3210" },
			{ "rounded", "band=0003 corner=rounded" }, { "double", "band=0203" },
			{ "chamfer", "band=0203 corner=cut" }, { "soft", "band=0102 corner=rounded" },
			{ "ornate", "band=0003 corner=diamond diamond=1" } }) {
			auto a{ rom_with(STAND_IN) }, b{ rom_with(STAND_IN) };
			install(a, "AtlasDevWindowFrame style=" + style);
			install(b, "AtlasDevWindowFrame " + band);
			require(a == b, "style=" + style + " is " + band);
			require(a != rom_with(STAND_IN), "style=" + style + " changes the frame");
		}
		// a tile over a band, its rows written with slashes
		expect("AtlasDevWindowFrame band=0123 tl=......../......../......../......../......../......../......../........",
			std::string(32, '0') + BAND_SQUARE.substr(32));
		// one tile over a band: the bottom right in color 3, the rest the band's
		expect("AtlasDevWindowFrame band=0123 br=" + std::string(64, '3'),
			BAND_SQUARE.substr(0, 160) + std::string(32, 'f'));
		// one tile over the classic frame: the top left blank, the rest as they were
		expect("AtlasDevWindowFrame tl=" + std::string(64, '.'), std::string(32, '0') + STAND_IN.substr(32));

		refused("AtlasDevWindowFrame style=hollow", STAND_IN, "the style is unknown (the hack holds no graphics)");
		refused("AtlasDevWindowFrame style=bevel band=0123", STAND_IN, "style or band, not both");
		refused("AtlasDevWindowFrame style=rounded corner=cut", STAND_IN, "a style is its own corner");
		refused("AtlasDevWindowFrame band=012", STAND_IN, "a band is four colors");
		refused("AtlasDevWindowFrame band=0124", STAND_IN, "a band color is 0 to 3");
		refused("AtlasDevWindowFrame corner=rounded", STAND_IN, "corner shapes a band");
		refused("AtlasDevWindowFrame band=0123 corner=oval", STAND_IN, "the shape is unknown");
		refused("AtlasDevWindowFrame band=0003 diamond=2", STAND_IN, "diamond needs corner=diamond");
		refused("AtlasDevWindowFrame band=0003 corner=diamond diamond=4", STAND_IN, "a diamond color is 1 to 3");
		refused("AtlasDevWindowFrame tl=0123", STAND_IN, "a tile is 64 pixels");
		refused("AtlasDevWindowFrame side=" + std::string(63, '1') + "4", STAND_IN, "a pixel is 0 to 3");
		refused("AtlasDevWindowFrame band=0123", BAND_SQUARE, "the frame is not the original");
		refused("AtlasDevWindowFrame color=3", STAND_IN, "color is not a parameter");
		std::cout << "atlas_window_frame_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_window_frame_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
