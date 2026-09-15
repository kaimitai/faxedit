#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevScreenBlink to known bytes for the plain install, two
// flags at either end of a flag byte and mode=vanilla, and checks that every
// refusal writes nothing; everything it touches is in bank 15
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// the gate and the redraw path, $db86 to $dbae, and the redraw loop at $d2ce
	const std::string GATE_HEX{ "ad2f04f006a554c902901e2047cb2025ca2030c120b4c12025ca208dc220f7ca200fdd2017cb4c45db" };
	const std::string REDRAW_HEX{ "2048e020e7d2201dd6a554c902b005a50cd0ed60a557d0e860" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	const std::vector<Shape> SHAPES{
		{ "AtlasDevScreenBlink", 0, {
			{ 15, 0xdb8f, "eaea" },
		} },
		{ "AtlasDevScreenBlink flag=168", 15, {
			{ 15, 0xdb8b, "20cefc901fea" },
			{ 15, 0xfcce, "a554c902b008ad16012901f0013860" },
		} },
		{ "AtlasDevScreenBlink flag=7", 15, {
			{ 15, 0xdb8b, "20cefc901fea" },
			{ 15, 0xfcce, "a554c902b008ad01012980f0013860" },
		} },
		{ "AtlasDevScreenBlink mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 15, 0xdb86, from_hex(GATE_HEX));
		put(rom, 15, 0xd2ce, from_hex(REDRAW_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
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
			auto want{ vanilla_rom() };
			for (const auto& r : shape.regions) {
				const auto bytes{ from_hex(r.hex) };
				const auto off{ klib::Asm6502::get_file_offset(r.bank, r.cpu) };
				for (std::size_t i{ 0 }; i < bytes.size(); ++i) want[off + i] = bytes[i];
			}
			for (std::size_t i{ 0 }; i < rom.size(); ++i)
				require(rom[i] == want[i], spec + ": file offset " + std::to_string(i) + " differs from the expected install");
		}
		for (const char* bad : { "AtlasDevScreenBlink flag=248", "AtlasDevScreenBlink mode=on",
			"AtlasDevScreenBlink speed=3", "AtlasDevScreenBlink mode=vanilla flag=248" })
			refused(vanilla_rom(), bad, bad);
		for (const word site : { word{ 0xdb8f }, word{ 0xdb9a }, word{ 0xd2d8 } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, site)] ^= 0x01;
			refused(rom, "AtlasDevScreenBlink", "a changed byte at $" + std::to_string(site));
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 15, 0xdb8b, from_hex("20cefc901fea"));   // installed once already
			refused(rom, "AtlasDevScreenBlink flag=168", "a second install");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			refused(rom, "AtlasDevScreenBlink flag=168", "used bank 15 space");
		}
		std::cout << "atlas_screen_blink_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_screen_blink_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
