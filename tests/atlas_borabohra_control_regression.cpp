#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevBorabohraControl to known bytes for the default, a
// faster glide, a shorter swell, the slowest glide, the fastest and shortest
// one, turn, the wide box, and a flag with and without turn; the code goes in
// bank 15 and the sites in bank 14
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// borabohra's routine, $9c89 to $9cf1, and its box record
	const std::string ROUTINE_HEX{
		"2085a8d010a9029d0403a9009dec029de4022094a8bde4024ab011feec02bdec02c9149006fee402207b8660bdec02a00220e183a002"
		"20c183209484feec02297fd003207b866020828cbde4024ab009bdec024a4aa84ced9cad83034a4a2906186908a8984a4c8e8c" };
	const std::string BOX_HEX{ "04001830" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevBorabohraControl", 0, {
		} },
		{ "AtlasDevBorabohraControl speed=16", 0, {
			{ 14, 0x9cbe, "03" },
		} },
		{ "AtlasDevBorabohraControl speed=2", 20, {
			{ 14, 0x9cb5, "4ccefc" },
			{ 15, 0xfcce, "bdec02a00220e1838d7403a9008d7503a84cc29c" },
		} },
		{ "AtlasDevBorabohraControl speed=32 loop=32", 0, {
			{ 14, 0x9cb9, "04" },
			{ 14, 0x9cbe, "06" },
		} },
		{ "AtlasDevBorabohraControl turn=32", 13, {
			{ 14, 0x9cc8, "4ccefc" },
			{ 15, 0xfcce, "bdec02291ff0034ccf9c4ccc9c" },
		} },
		{ "AtlasDevBorabohraControl body=32", 0, {
			{ 14, 0xb32f, "00" },
			{ 14, 0xb331, "20" },
		} },
		{ "AtlasDevBorabohraControl speed=16 loop=64 turn=32 body=32", 13, {
			{ 14, 0x9cb9, "03" },
			{ 14, 0x9cbe, "04" },
			{ 14, 0x9cc8, "4ccefc" },
			{ 14, 0xb32f, "00" },
			{ 14, 0xb331, "20" },
			{ 15, 0xfcce, "bdec02291ff0034ccf9c4ccc9c" },
		} },
		{ "AtlasDevBorabohraControl flag=12", 0, {
		} },
		{ "AtlasDevBorabohraControl turn=32 flag=12", 33, {
			{ 14, 0x9cc8, "4ccefc" },
			{ 15, 0xfcce, "48ad02012910f00e68bdec02291ff0034ccf9c4ccc9c68297fd0034ccc9c4ccf9c" },
		} },
		{ "AtlasDevBorabohraControl speed=16 loop=64 turn=32 body=32 flag=12", 62, {
			{ 14, 0x9cb5, "4ccefc" },
			{ 14, 0x9cc8, "4cebfc" },
			{ 14, 0xb32f, "00" },
			{ 14, 0xb331, "20" },
			{ 15, 0xfcce, "ad02012910f010bdec02a00320e183a00420c1834cc29cbdec024cb89c48ad02012910f00e68bdec02291ff0034ccf9c4ccc9c68297fd0034ccc9c4ccf9c" },
		} },
		{ "AtlasDevBorabohraControl speed=16 flag=12", 29, {
			{ 14, 0x9cb5, "4ccefc" },
			{ 15, 0xfcce, "ad02012910f010bdec02a00220e183a00320c1834cc29cbdec024cb89c" },
		} },
		{ "AtlasDevBorabohraControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 14, 0x9c89, from_hex(ROUTINE_HEX));
		put(rom, 14, 0xb32f, from_hex(BOX_HEX));
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
			require(install(rom, "AtlasDevBorabohraControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevBorabohraControl speed=3", "AtlasDevBorabohraControl speed=64",
				"AtlasDevBorabohraControl loop=16", "AtlasDevBorabohraControl loop=512",
				"AtlasDevBorabohraControl speed=2 loop=256", "AtlasDevBorabohraControl turn=3",
				"AtlasDevBorabohraControl turn=256", "AtlasDevBorabohraControl body=28", "AtlasDevBorabohraControl flag=248",
				"AtlasDevBorabohraControl mode=on", "AtlasDevBorabohraControl rise=3",
				"AtlasDevBorabohraControl mode=vanilla speed=3" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9cb6)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevBorabohraControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla glide site is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 14, 0x9cc8, { 0x4c, 0x00, 0x90 });   // something else owns the turn site
			bool threw{ false };
			try { install(rom, "AtlasDevBorabohraControl turn=32"); } catch (const std::exception&) { threw = true; }
			require(threw, "with turn a turn site patched by something else is refused");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0xb32f)] = 0x05;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevBorabohraControl body=32"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "with body=32 a changed box is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xfcd0)] = 0x00;   // bank 15 space already used
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevBorabohraControl speed=2"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "used bank 15 space is refused and nothing is written");
		}
		std::cout << "atlas_borabohra_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_borabohra_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
