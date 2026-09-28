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

// pins install_AtlasDevPreventTextbox to known bytes for the default value and
// for three chosen values, refuses the values that would take the plain box or
// a portrait, refuses a changed byte at either end of both checked sites and a
// second install, and checks that every refused install leaves the rom as it
// was. the jp rom calls a different routine at $82b4, just before the checked
// end site, and must still install. the hooks are in bank 12 and the stub in
// bank 15
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// the script start $825a to $8269 and the script end $82b4 to $82c4, as in the us roms
	const std::string ENTRY_HEX{ "8d01021008297f204df2201f8220e281" };
	const std::string END_HEX{ "20b087ad010210062081f24c2b824cfb81" };

	std::string body(const std::string& p_value) {
		return "ad0102c9" + p_value + "f0034ce28160" + "ad0102c9" + p_value + "f0034cfb8160";
	}

	struct Region { byte bank; word cpu; std::string hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	const std::vector<Region> HOOKS{ { 12, 0x8267, "20cefc" }, { 12, 0x82c2, "4cd9fc" } };
	const std::vector<Shape> SHAPES{
		{ "AtlasDevPreventTextbox", 22, { HOOKS[0], HOOKS[1], { 15, ORG, body("7f") } } },
		{ "AtlasDevPreventTextbox textbox=$40", 22, { HOOKS[0], HOOKS[1], { 15, ORG, body("40") } } },
		{ "AtlasDevPreventTextbox textbox=1", 22, { HOOKS[0], HOOKS[1], { 15, ORG, body("01") } } },
		{ "AtlasDevPreventTextbox textbox=127", 22, { HOOKS[0], HOOKS[1], { 15, ORG, body("7f") } } },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 12, 0x825a, from_hex(ENTRY_HEX));
		put(rom, 12, 0x82b4, from_hex(END_HEX));
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
			// the jp rom: $82b4 calls $8788 where the other roms call $87b0
			auto jp{ vanilla_rom() };
			put(jp, 12, 0x82b4, from_hex("208887"));
			auto rom{ jp };
			require(install(rom, "AtlasDevPreventTextbox") == 22, "the jp rom: size");
			expect(rom, jp, { HOOKS[0], HOOKS[1], { 15, ORG, body("7f") } }, "the jp rom");
		}
		for (const char* bad : { "AtlasDevPreventTextbox textbox=0", "AtlasDevPreventTextbox textbox=128",
			"AtlasDevPreventTextbox textbox=$8a", "AtlasDevPreventTextbox textbox=255",
			"AtlasDevPreventTextbox textbox=256", "AtlasDevPreventTextbox flag=7" })
			refused(vanilla_rom(), bad, bad);
		for (const word site : { word{ 0x825a }, word{ 0x8267 }, word{ 0x8269 }, word{ 0x82b7 }, word{ 0x82c2 },
			word{ 0x82c4 } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(12, site)] ^= 0x01;
			refused(rom, "AtlasDevPreventTextbox", "a changed byte at $" + std::to_string(site));
		}
		{
			auto rom{ vanilla_rom() };
			install(rom, "AtlasDevPreventTextbox");   // installed once already
			refused(rom, "AtlasDevPreventTextbox", "a second install");
		}
		{
			// bank 15 space already used: at the stub's first byte, and at its last byte
			for (const word site : { ORG, word{ ORG + 21 } }) {
				auto rom{ vanilla_rom() };
				rom[klib::Asm6502::get_file_offset(15, site)] = 0x00;
				refused(rom, "AtlasDevPreventTextbox", "used bank 15 space at $" + std::to_string(site));
			}
			// and the byte after the stub is not needed
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, static_cast<word>(ORG + 22))] = 0x00;
			require(install(rom, "AtlasDevPreventTextbox") == 22, "the byte after the stub may be used");
		}
		std::cout << "atlas_prevent_textbox_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_prevent_textbox_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
