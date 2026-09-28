#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevZorugeriruControl to known bytes for the default, the
// shortest rest, the shortest windup with a single rock, the longest rest and
// windup with every rock and the slowest fall, the wide body, and a flag; the
// code goes in bank 15, the sites in bank 14, and body=32 sets one box byte
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// zorugeriru's routine and the rock's, $9da9 to $9f02, with all four sites
	const std::string ROUTINE_HEX{
		"2085a8d010a9049d0403a9009dec029de4022094a8bde4024ab01ddeec02bdec02290fd0122036a2b00d20f79db008fee402a900"
		"9dec0260feec02bdec02c920900bfee402a93c9dec024c139e60a007a9008500b9cc02c903d00ae600a500c904900238608810ec"
		"18602036a2b0faa90399cc02a90099e40299dc02a9ff993403b5ba1869108500a59ed5ba9007c500b00338e91099ba00a92099c2"
		"00bd2c03992c034c02a220828ca000bde4024a9009ad83034a4a4a2903a8b9699e4c8e8c000102012085a8d010a9009d0403a900"
		"9df4029de4022094a8bce40288f03488f04ebddc0209809ddc02bdf402a00520d18320c485b00efef402bdf402c9419011def402"
		"60fee402a9059dec02a90720e4d060bdec0229010a0a38e9021875c295c2deec02d008a90f9dec02fee40260deec02d005a9ff9d"
		"cc026020828ca003bde402c902900fbdec02c90ab008a003c905b002a005984c8e8c" };
	const std::string BOX_HEX{ "00001020" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevZorugeriruControl", 0, {
		} },
		{ "AtlasDevZorugeriruControl rest=1", 0, {
			{ 14, 0x9def, "31" },
		} },
		{ "AtlasDevZorugeriruControl rest=16 windup=255 cap=7 fall=1", 0, {
			{ 14, 0x9de8, "ff" },
			{ 14, 0x9def, "30" },
			{ 14, 0x9e09, "07" },
			{ 14, 0x9e97, "04" },
		} },
		{ "AtlasDevZorugeriruControl body=32", 0, {
			{ 14, 0xb339, "20" },
		} },
		{ "AtlasDevZorugeriruControl flag=12", 0, {
		} },
		{ "AtlasDevZorugeriruControl rest=16 windup=255 cap=7 fall=1 flag=12", 80, {
			{ 14, 0x9de4, "4cd4fc" },
			{ 14, 0x9dee, "4ce7fc" },
			{ 14, 0x9e06, "4cf8fc" },
			{ 14, 0x9e93, "4c0bfd" },
			{ 15, 0xfcce, "ad020129106020cefcf008bdec02c9" },
			{ 15, 0xfcde, "4ce99dbdec024ce79d20cefcf004a930d002a93c9dec024cf39d20cefcf007a500c9074c0a9ea500c9044c0a9e20cefcf008bdf402a0044c989ebdf4024c969e" },
		} },
		{ "AtlasDevZorugeriruControl windup=255 flag=12", 21, {
			{ 14, 0x9de4, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f008bdec02c9" },
			{ 15, 0xfcda, "4ce99dbdec024ce79d" },
		} },
		{ "AtlasDevZorugeriruControl windup=255 fall=1 flag=12", 42, {
			{ 14, 0x9de4, "4ccefc" },
			{ 14, 0x9e93, "4ce3fc" },
			{ 15, 0xfcce, "ad02012910f008bdec02c9" },
			{ 15, 0xfcda, "4ce99dbdec024ce79dad02012910f008bdf402a0044c989ebdf4024c969e" },
		} },
		{ "AtlasDevZorugeriruControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9da9, from_hex(ROUTINE_HEX));
		put(rom, 14, 0xb337, from_hex(BOX_HEX));
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
			require(install(rom, "AtlasDevZorugeriruControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevZorugeriruControl rest=0", "AtlasDevZorugeriruControl rest=17",
				"AtlasDevZorugeriruControl windup=0", "AtlasDevZorugeriruControl windup=256", "AtlasDevZorugeriruControl cap=0",
				"AtlasDevZorugeriruControl cap=8", "AtlasDevZorugeriruControl fall=3", "AtlasDevZorugeriruControl body=24",
				"AtlasDevZorugeriruControl flag=248", "AtlasDevZorugeriruControl mode=on", "AtlasDevZorugeriruControl speed=3",
				"AtlasDevZorugeriruControl mode=vanilla rest=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		for (const word flip : { word{ 0x9de5 }, word{ 0xb339 } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, flip)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevZorugeriruControl body=32"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla site or box is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9e06, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevZorugeriruControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevZorugeriruControl rest=16 windup=255 cap=7 fall=1 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_zorugeriru_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_zorugeriru_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
