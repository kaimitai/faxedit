#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevScreenTransition to known bytes: the stock table, which
// installs nothing, a table per direction, the h and v shorthands, two flags at
// either end of a flag byte, mode=vanilla, and the install after
// AtlasDevVerticalScroll, which chains to the gate that hack left. Every
// refusal must write nothing. Everything this hack touches is in bank 15
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// the gate and the blank path, $db86 to $dbae, and the redraw loop at $d2ce
	const std::string GATE_HEX{ "ad2f04f006a554c902901e2047cb2025ca2030c120b4c12025ca208dc220f7ca200fdd2017cb4c45db" };
	const std::string REDRAW_HEX{ "2048e020e7d2201dd6a554c902b005a50cd0ed60a557d0e860" };
	// jsr $fcce / bcc $dbaf / nop: the six bytes this hack writes over the gate
	const std::string SITE_HEX{ "20cefc901fea" };
	// the shape AtlasDevVerticalScroll leaves at the gate, calling its own stub
	// at $fdb5; this hack must recognise it and chain to that address
	const std::string VS_SITE_HEX{ "20b5fd901fea" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	const std::vector<Shape> SHAPES{
		// the stock table is left scroll, right scroll, up blink, down blink,
		// which is what the game already does, so nothing is installed
		{ "AtlasDevScreenTransition", 0, {
		} },
		{ "AtlasDevScreenTransition h=blink", 26, {
			{ 15, 0xdb8b, SITE_HEX.c_str() },
			{ 15, 0xfcce, "a654bde4fcf0023860e002900238601860a554c9026001010101" },
		} },
		{ "AtlasDevScreenTransition left=blink", 26, {
			{ 15, 0xdb8b, SITE_HEX.c_str() },
			{ 15, 0xfcce, "a654bde4fcf0023860e002900238601860a554c9026001000101" },
		} },
		{ "AtlasDevScreenTransition h=blink flag=176", 33, {
			{ 15, 0xdb8b, SITE_HEX.c_str() },
			{ 15, 0xfcce, "ad17012901f011a654bdebfcf0023860e002900238601860a554c9026001010101" },
		} },
		{ "AtlasDevScreenTransition h=blink flag=7", 33, {
			{ 15, 0xdb8b, SITE_HEX.c_str() },
			{ 15, 0xfcce, "ad01012980f011a654bdebfcf0023860e002900238601860a554c9026001010101" },
		} },
		{ "AtlasDevScreenTransition mode=vanilla", 0, {
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

	// a rom where AtlasDevVerticalScroll already owns the gate
	std::vector<byte> vertical_rom() {
		auto rom{ vanilla_rom() };
		put(rom, 15, 0xdb8b, from_hex(VS_SITE_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

	void expect(const std::vector<byte>& rom, std::vector<byte> want, const std::vector<Region>& regions,
		const std::string& spec) {
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
			// after AtlasDevVerticalScroll: up and down may scroll, and the stub
			// reads that hack's state byte first, then chains to its gate
			auto rom{ vertical_rom() };
			require(install(rom, "AtlasDevScreenTransition v=scroll") == 32, "after vertical scroll: size");
			expect(rom, vertical_rom(), {
				{ 15, 0xdb8b, SITE_HEX.c_str() },
				{ 15, 0xfcce, "ade204d00da654bdeafcf0023860e00290034cb5fd1860a554c9026000000000" },
			}, "AtlasDevScreenTransition v=scroll after AtlasDevVerticalScroll");
		}
		// a vertical scroll is the only thing that can supply a vertical slide
		refused(vanilla_rom(), "AtlasDevScreenTransition up=scroll",
			"up=scroll without AtlasDevVerticalScroll");
		refused(vanilla_rom(), "AtlasDevScreenTransition down=scroll",
			"down=scroll without AtlasDevVerticalScroll");
		for (const char* bad : { "AtlasDevScreenTransition h=sideways", "AtlasDevScreenTransition left=1",
			"AtlasDevScreenTransition h=blink flag=248", "AtlasDevScreenTransition mode=on",
			"AtlasDevScreenTransition speed=3", "AtlasDevScreenTransition mode=vanilla flag=248",
			"AtlasDevScreenTransition mode=vanilla h=sideways" })
			refused(vanilla_rom(), bad, bad);
		for (const word site : { word{ 0xdb8f }, word{ 0xdb9a }, word{ 0xd2d8 } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, site)] ^= 0x01;
			refused(rom, "AtlasDevScreenTransition h=blink", "a changed byte at $" + std::to_string(site));
		}
		{
			// a gate that is neither stock nor a transition hack's: stop rather than guess
			auto rom{ vanilla_rom() };
			put(rom, 15, 0xdb8b, from_hex("4c00c0eaeaea"));
			refused(rom, "AtlasDevScreenTransition h=blink", "a gate owned by something else");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			refused(rom, "AtlasDevScreenTransition h=blink", "used bank 15 space");
		}
		std::cout << "atlas_screen_transition_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_screen_transition_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
