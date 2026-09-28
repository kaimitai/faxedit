#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevExecutionHoodControl to known bytes for the default,
// the slowest walk, the fastest walk with the longest walk, the shortest walk
// with the shortest pause, a tuned shape and a flag; the code goes in bank 15
// and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// execution hood's routine, $91b3 to $9238
	const std::string ROUTINE_HEX{
		"2085a8d00ba9009df4029de4022094a8bde4022903c903f03fa9808d7403a9008d7503201984bdf402a00520e183a00520d183a9008d77"
		"03209c85fef402bdf4022907d003201084bdf402290fd008fee402a90f9dec0260207b86deec02d00bfee402a9009df4024c93a06020"
		"828ca000bde4022903c903d008ad83032904f001c8984c8e8c" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevExecutionHoodControl", 0, {
		} },
		{ "AtlasDevExecutionHoodControl walk=1", 0, {
			{ 14, 0x91cd, "20" },
		} },
		{ "AtlasDevExecutionHoodControl walk=16 length=192", 0, {
			{ 14, 0x91cd, "00" },
			{ 14, 0x91d2, "02" },
			{ 14, 0x91ff, "3f" },
		} },
		{ "AtlasDevExecutionHoodControl length=24 pause=1", 0, {
			{ 14, 0x91ff, "07" },
			{ 14, 0x9206, "01" },
		} },
		{ "AtlasDevExecutionHoodControl walk=8 length=96 pause=30", 0, {
			{ 14, 0x91cd, "00" },
			{ 14, 0x91d2, "01" },
			{ 14, 0x91ff, "1f" },
			{ 14, 0x9206, "1e" },
		} },
		{ "AtlasDevExecutionHoodControl flag=12", 0, {
		} },
		{ "AtlasDevExecutionHoodControl walk=8 length=96 pause=30 flag=12", 85, {
			{ 14, 0x91cc, "4cd4fc" },
			{ 14, 0x91fb, "4cf3fc" },
			{ 14, 0x9205, "4c12fd" },
			{ 15, 0xfcce, "ad020129106020cefcf00da9008d7403a9018d75034cd691a9808d7403a9008d75034cd69120cefcf00dbdf402291ff0034c0a924c0292bdf402290ff0034c0a924c029220cefcf004a91ed002a90f9dec024c0a92" },
		} },
		{ "AtlasDevExecutionHoodControl pause=1 flag=12", 19, {
			{ 14, 0x9205, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f004a901d002a90f9dec024c0a92" },
		} },
		{ "AtlasDevExecutionHoodControl pause=1 length=192 flag=12", 52, {
			{ 14, 0x91fb, "4ccefc" },
			{ 14, 0x9205, "4ceffc" },
			{ 15, 0xfcce, "ad02012910f00dbdf402293ff0034c0a924c0292bdf402290ff0034c0a924c0292ad02012910f004a901d002a90f9dec024c0a92" },
		} },
		{ "AtlasDevExecutionHoodControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x91b3, from_hex(ROUTINE_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

}

int main() {
	try {
		require(ROUTINE_HEX.size() == 0x86 * 2, "the routine is 134 bytes");
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
			require(install(rom, "AtlasDevExecutionHoodControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevExecutionHoodControl walk=0", "AtlasDevExecutionHoodControl walk=17",
				"AtlasDevExecutionHoodControl length=36", "AtlasDevExecutionHoodControl length=384",
				"AtlasDevExecutionHoodControl pause=0", "AtlasDevExecutionHoodControl pause=256",
				"AtlasDevExecutionHoodControl flag=248", "AtlasDevExecutionHoodControl mode=on",
				"AtlasDevExecutionHoodControl speed=3", "AtlasDevExecutionHoodControl mode=vanilla walk=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x91ff)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevExecutionHoodControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla length site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9205, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevExecutionHoodControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevExecutionHoodControl walk=8 length=96 pause=30 flag=12"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_execution_hood_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_execution_hood_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
