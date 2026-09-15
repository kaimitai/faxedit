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

// pins install_AtlasDevFastBlink to known bytes for the plain install, two
// flags at either end of a flag byte, mode=vanilla, and an install after
// AtlasDevScreenBlink in both of its forms; pins that the reverse order is
// refused; and checks that every refused install leaves the rom as it was
// (the install driver works on a copy and keeps it only when every hack
// succeeds). everything the hack touches is in bank 15
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// the sites the hack checks: the gate and blank path $db86 to $dbae, the
	// flush wait $caf7, the reset $cb17, the queue draw $cf3c, its capacity
	// wait $cfca, its clear wait $cff4 and the drain all $cffb
	const std::string GATE_PATH_HEX{ "ad2f04f006a554c902901e2047cb2025ca2030c120b4c12025ca208dc220f7ca200fdd2017cb4c45db" };
	const std::string FLUSH_HEX{ "a520c51fd0faa90085148513855ba514c90290fa60" };
	const std::string RESET_HEX{ "2047cba901851360" };
	const std::string DRAW_HEX{ "a51fc520f0f9" };
	const std::string WFC_HEX{ "20d0cf90fb60" };
	const std::string WUC_HEX{ "a520c51fd0fa60" };
	const std::string DRAW_ALL_HEX{ "203ccfa520c51fd0f760" };
	// AtlasDevScreenBlink's redraw loop site, so it can be installed first
	const std::string REDRAW_HEX{ "2048e020e7d2201dd6a554c902b005a50cd0ed60a557d0e860" };

	const std::string PLAIN_BODY{
		"a90085148513855b8d012020fbcf2047cb2030c120b4c1208dc220fbcf200fdd2017cb4c45db"
		"20d0cfb00aa513d0f7203ccf4cf4fc60a513f007a520c51fd0fa604cfbcf" };
	const std::string FLAG_TAIL{
		"2047cb2025ca2030c120b4c12025ca208dc220f7ca200fdd2017cb4c45db"
		"a90085148513855b8d012020fbcf2047cb2030c120b4c1208dc220fbcf200fdd2017cb4c45db"
		"20d0cfb00aa513d0f7203ccf4c19fd60a513f007a520c51fd0fa604cfbcf" };
	const std::string PATH_HOOK{ "4ccefceaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaea" };

	struct Region { byte bank; word cpu; std::string hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	const std::vector<Shape> SHAPES{
		{ "AtlasDevFastBlink", 68, {
			{ 15, 0xdb91, PATH_HOOK },
			{ 15, 0xcfca, "4cf4fceaeaea" },
			{ 15, 0xcff4, "4c04fdeaeaeaea" },
			{ 15, 0xfcce, PLAIN_BODY },
		} },
		{ "AtlasDevFastBlink flag=176", 105, {
			{ 15, 0xdb91, PATH_HOOK },
			{ 15, 0xcfca, "4c19fdeaeaea" },
			{ 15, 0xcff4, "4c29fdeaeaeaea" },
			{ 15, 0xfcce, "ad17012901d01e" + FLAG_TAIL },
		} },
		{ "AtlasDevFastBlink flag=7", 105, {
			{ 15, 0xdb91, PATH_HOOK },
			{ 15, 0xcfca, "4c19fdeaeaea" },
			{ 15, 0xcff4, "4c29fdeaeaeaea" },
			{ 15, 0xfcce, "ad01012980d01e" + FLAG_TAIL },
		} },
		{ "AtlasDevFastBlink mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 15, 0xdb86, from_hex(GATE_PATH_HEX));
		put(rom, 15, 0xcaf7, from_hex(FLUSH_HEX));
		put(rom, 15, 0xcb17, from_hex(RESET_HEX));
		put(rom, 15, 0xcf3c, from_hex(DRAW_HEX));
		put(rom, 15, 0xcfca, from_hex(WFC_HEX));
		put(rom, 15, 0xcff4, from_hex(WUC_HEX));
		put(rom, 15, 0xcffb, from_hex(DRAW_ALL_HEX));
		put(rom, 15, 0xd2ce, from_hex(REDRAW_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec, word org = ORG) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, org, 0xfff0, hacks, nullptr);
	}

	void expect(const std::vector<byte>& rom, std::vector<byte> want, const std::vector<Region>& regions, const std::string& spec) {
		for (const auto& r : regions) {
			const auto bytes{ from_hex(r.hex) };
			const auto off{ klib::Asm6502::get_file_offset(r.bank, r.cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) want[off + i] = bytes[i];
		}
		for (std::size_t i{ 0 }; i < rom.size(); ++i)
			require(rom[i] == want[i], spec + ": file offset " + std::to_string(i) + " differs from the expected install");
	}

	void refused(std::vector<byte> rom, const std::string& spec, const std::string& why) {
		const auto before{ rom };
		bool threw{ false };
		try { install(rom, spec); } catch (const std::exception&) { threw = true; }
		require(threw, "must refuse: " + why);
		require(rom == before, "refusal must not write: " + why);
	}
}

int main() {
	try {
		for (const auto& shape : SHAPES) {
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, shape.spec) };
			const std::string spec{ shape.spec };
			require(n == shape.size, spec + ": size " + std::to_string(n));
			expect(rom, vanilla_rom(), shape.regions, spec);
		}
		{
			// after the plain AtlasDevScreenBlink (two nops at $db8f, no free space) the install is the plain shape
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevScreenBlink") == 0, "the plain screen blink takes no free space");
			require(install(rom, "AtlasDevFastBlink") == 68, "fast blink after screen blink: size");
			auto regions{ SHAPES[0].regions };
			regions.push_back({ 15, 0xdb8f, "eaea" });
			expect(rom, vanilla_rom(), regions, "AtlasDevScreenBlink then AtlasDevFastBlink");
		}
		{
			// after AtlasDevScreenBlink flag=168 (a 15 byte stub at the origin) the body and its two wait hooks move up 15 bytes
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevScreenBlink flag=168") == 15, "the flagged screen blink takes 15 bytes");
			require(install(rom, "AtlasDevFastBlink", static_cast<word>(ORG + 15)) == 68, "fast blink after the flagged screen blink: size");
			expect(rom, vanilla_rom(), {
				{ 15, 0xdb8b, "20cefc901fea" },
				{ 15, 0xfcce, "a554c902b008ad16012901f0013860" },
				{ 15, 0xdb91, "4cddfceaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaea" },
				{ 15, 0xcfca, "4c03fdeaeaea" },
				{ 15, 0xcff4, "4c13fdeaeaeaea" },
				{ 15, 0xfcdd, "a90085148513855b8d012020fbcf2047cb2030c120b4c1208dc220fbcf200fdd2017cb4c45db20d0cfb00aa513d0f7203ccf4c03fd60a513f007a520c51fd0fa604cfbcf" },
			}, "AtlasDevScreenBlink flag=168 then AtlasDevFastBlink");
		}
		{
			// the reverse order is refused: AtlasDevScreenBlink checks the stock blank path this hack rewrites
			auto rom{ vanilla_rom() };
			install(rom, "AtlasDevFastBlink");
			refused(rom, "AtlasDevScreenBlink", "AtlasDevScreenBlink listed after AtlasDevFastBlink");
			refused(rom, "AtlasDevScreenBlink flag=168", "AtlasDevScreenBlink flag=168 listed after AtlasDevFastBlink");
		}
		for (const char* bad : { "AtlasDevFastBlink flag=248", "AtlasDevFastBlink mode=on",
			"AtlasDevFastBlink speed=3", "AtlasDevFastBlink mode=vanilla flag=248" })
			refused(vanilla_rom(), bad, bad);
		for (const word site : { word{ 0xdb92 }, word{ 0xcaf8 }, word{ 0xcb18 }, word{ 0xcf3d }, word{ 0xcfcb },
			word{ 0xcff5 }, word{ 0xcffc } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, site)] ^= 0x01;
			refused(rom, "AtlasDevFastBlink", "a changed byte at $" + std::to_string(site));
		}
		{
			auto rom{ vanilla_rom() };
			install(rom, "AtlasDevFastBlink");   // installed once already
			refused(rom, "AtlasDevFastBlink", "a second install");
		}
		for (const auto& [spec, last] : { std::pair{ "AtlasDevFastBlink", word{ 0xfd11 } }, std::pair{ "AtlasDevFastBlink flag=176", word{ 0xfd36 } } }) {
			// bank 15 space already used: at the body's third byte, and at its last byte
			for (const word site : { word{ 0xfcd0 }, last }) {
				auto rom{ vanilla_rom() };
				rom[klib::Asm6502::get_file_offset(15, site)] = 0x00;
				refused(rom, spec, std::string{ spec } + " with used bank 15 space at $" + std::to_string(site));
			}
			// and the byte after the body is not needed
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, static_cast<word>(last + 1))] = 0x00;
			require(install(rom, spec) > 0, std::string{ spec } + ": the byte after the body may be used");
		}
		std::cout << "atlas_fast_blink_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_fast_blink_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
