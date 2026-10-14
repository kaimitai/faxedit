#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// the placement helper: bank 15, bank=n with its trampoline, sram=true on
// both rom sizes, and the refusals, which must leave the rom as it was
namespace {
	constexpr word ORG{ 0xfcce };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> blank_rom(bool expand = false) {
		std::vector<byte> rom(expand ? 0x80010 : 0x40010, 0xff);
		const byte header[]{ 'N', 'E', 'S', 0x1a, static_cast<byte>(expand ? 32 : 16), 0, 0x10, 0 };
		for (std::size_t i{ 0 }; i < sizeof header; ++i) rom[i] = header[i];
		return rom;
	}

	fe::Config config(const std::vector<byte>& rom, bool expand = false) {
		fe::Config result;
		result.load_definitions(EOE_TEST_CONFIG_PATH, "");
		result.set_region(expand ? "us-512" : "us");
		result.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		return result;
	}

	fh::GeneralHack hack(const std::string& params) {
		return fh::parse_general_hacks("StageDoorDirection " + params).at(0);
	}

	// nops, then a jmp back to the start
	klib::Asm6502 body(std::size_t size) {
		klib::Asm6502 code;
		code.label("@entry");
		for (std::size_t i{ 3 }; i < size; ++i) code.nop();
		code.jmp("@entry");
		return code;
	}

	byte at(const std::vector<byte>& rom, byte bank, word cpu) { return rom[klib::Asm6502::get_file_offset(bank, cpu)]; }

	template<typename F>
	void refuses(std::vector<byte>& rom, const std::string& reason, F call) {
		const auto before{ rom };
		bool threw{ false };
		try { call(); }
		catch (const std::runtime_error& e) {
			threw = true;
			require(std::string{ e.what() }.find(reason) != std::string::npos, reason + ": " + e.what());
		}
		require(threw, "must refuse: " + reason);
		require(rom == before, "a refusal wrote to the rom: " + reason);
	}
}

int main() {
	try {
		// bank 15: at the cursor, called directly
		{
			auto rom{ blank_rom() };
			const auto cfg{ config(rom) };
			fh::HackManager m;
			auto p{ m.place_hack(cfg, rom, hack(""), "Test", ORG, 8) };
			auto code{ body(8) };
			word cursor{ m.install_placed(rom, code, p, ORG, "Test") };
			require(cursor == ORG + 8 && at(rom, 15, ORG + 6) == 0xce, "bank 15");
			require(m.far_entry(cfg, rom, p, ORG, cursor, "Test") == ORG && cursor == ORG + 8, "bank 15: no trampoline");
			require(m.far_entry_nmi(rom, p, ORG, cursor, "Test") == ORG && cursor == ORG + 8, "bank 15: no nmi trampoline");
		}
		// bank=11: the trailing free space, and a trampoline at the cursor
		{
			auto rom{ blank_rom() };
			const auto cfg{ config(rom) };
			fh::HackManager m;
			auto p{ m.place_hack(cfg, rom, hack("bank=11"), "Test", ORG, 8) };
			require(p.addr == 0x8010, "bank=11: trailing free space");
			auto code{ body(8) };
			word cursor{ m.install_placed(rom, code, p, ORG, "Test") };
			require(at(rom, 11, 0x8016) == 0x10, "bank=11: code in bank 11");
			require(m.far_entry(cfg, rom, p, 0x8010, cursor, "Test") == ORG && cursor == ORG + 26, "bank=11: trampoline");
			const byte want[]{ 0x85, 0xe5, 0xad, 0x00, 0x01, 0x48, 0xa2, 0x0b, 0x20, 0x1a, 0xcc, 0xa5, 0xe5,
				0x20, 0x10, 0x80, 0x85, 0xe5, 0x68, 0xaa, 0x20, 0x1a, 0xcc, 0xa5, 0xe5, 0x60 };
			for (std::size_t i{ 0 }; i < sizeof want; ++i)
				require(at(rom, 15, static_cast<word>(ORG + i)) == want[i], "bank=11: trampoline bytes");
			refuses(rom, "more far entries", [&] { m.far_entry(cfg, rom, p, 0x8010, cursor, "Test"); });
		}
		// bank=11 from the nmi: the bank switch the vanilla nmi uses for the sound engine
		{
			auto rom{ blank_rom() };
			const auto cfg{ config(rom) };
			fh::HackManager m;
			auto p{ m.place_hack(cfg, rom, hack("bank=11"), "Test", ORG, 8) };
			auto code{ body(8) };
			word cursor{ m.install_placed(rom, code, p, ORG, "Test") };
			require(m.far_entry_nmi(rom, p, 0x8013, cursor, "Test") == ORG && cursor == ORG + 18, "nmi: trampoline");
			const byte want[]{ 0xad, 0x00, 0x01, 0x48, 0xa2, 0x0b, 0x20, 0x85, 0xcc, 0x20, 0x13, 0x80,
				0x68, 0xaa, 0x20, 0x85, 0xcc, 0x60 };
			for (std::size_t i{ 0 }; i < sizeof want; ++i)
				require(at(rom, 15, static_cast<word>(ORG + i)) == want[i], "nmi: trampoline bytes");
			refuses(rom, "more far entries", [&] { m.far_entry_nmi(rom, p, 0x8013, cursor, "Test"); });
		}
		// sram=true: backed in bank 9 at $a700, or bank 27 at $8000
		for (const bool expand : { false, true }) {
			auto rom{ blank_rom(expand) };
			const auto cfg{ config(rom, expand) };
			fh::HackManager m;
			m.init_sram_state(cfg);
			auto p{ m.place_hack(cfg, rom, hack("sram=true"), "Test", ORG, 8) };
			require(p.addr == 0x6806, "sram: placement");
			auto code{ body(8) };
			word cursor{ m.install_placed(rom, code, p, ORG, "Test") };
			require(at(rom, expand ? 27 : 9, static_cast<word>((expand ? 0x8000 : 0xa700) + 6)) == 0x06, "sram: backing copy");
			require(m.far_entry_nmi(rom, p, 0x6806, cursor, "Test") == 0x6806 && cursor == ORG, "sram: no nmi trampoline");
		}
		// placement refusals
		{
			auto rom{ blank_rom() }, big{ blank_rom(true) };
			const auto cfg{ config(rom) }, big_cfg{ config(big, true) };
			fh::HackManager m;
			const auto place = [&](const std::string& params, word cursor = ORG) {
				m.place_hack(cfg, rom, hack(params), "Test", cursor, 8);
			};
			refuses(rom, "sram cannot be combined", [&] { place("sram=true bank=11"); });
			refuses(rom, "sram support", [&] { place("sram=true"); });
			refuses(rom, "addr needs a switchable bank", [&] { place("addr=0x9000"); });
			refuses(rom, "bank 12 is rewritten", [&] { place("bank=12"); });
			refuses(rom, "bank 14 is rewritten", [&] { place("bank=14"); });
			refuses(big, "bank 22 is rewritten", [&] { m.place_hack(big_cfg, big, hack("bank=22"), "Test", ORG, 8); });
			refuses(rom, "only for bank 15 hacks", [&] { place("", 0xbdb5); });
			auto used{ blank_rom() };
			used[klib::Asm6502::get_file_offset(11, 0x9004)] = 0x00;
			refuses(used, "bank 11 space", [&] { m.place_hack(cfg, used, hack("bank=11 addr=0x9000"), "Test", ORG, 8); });
			fh::HackManager s;
			s.init_sram_state(cfg);
			refuses(rom, "sram code area", [&] { s.place_hack(cfg, rom, hack("bank=9 addr=0xa6f0"), "Test", ORG, 32); });
		}
		// install refusals
		{
			auto rom{ blank_rom() };
			const auto cfg{ config(rom) };
			fh::HackManager m;
			// more code than was placed
			const auto small{ m.place_hack(cfg, rom, hack("bank=11"), "Test", ORG, 8) };
			auto big{ body(16) };
			refuses(rom, "8 placed", [&] { m.install_placed(rom, big, small, ORG, "Test"); });
			// two sram placements before either is installed
			m.init_sram_state(cfg);
			const auto p{ m.place_hack(cfg, rom, hack("sram=true"), "Test", ORG, 8) };
			const auto q{ m.place_hack(cfg, rom, hack("sram=true"), "Test", ORG, 8) };
			auto a{ body(8) }, b{ body(8) };
			m.install_placed(rom, a, p, ORG, "Test");
			refuses(rom, "placed at $6806", [&] { m.install_placed(rom, b, q, ORG, "Test"); });
			// taken by another hack in between
			const auto r{ m.place_hack(cfg, rom, hack("bank=11"), "Test", ORG, 8) };
			rom[klib::Asm6502::get_file_offset(11, r.addr + 2)] = 0x00;
			auto c{ body(8) };
			refuses(rom, "bank 11 space", [&] { m.install_placed(rom, c, r, ORG, "Test"); });
			// a long run of $ff at the end looks like free space
			const auto t{ m.place_hack(cfg, rom, hack("bank=11 addr=0x9000"), "Test", ORG, 20) };
			klib::Asm6502 d;
			d.rts();
			for (std::size_t i{ 0 }; i < 17; ++i) d.db(0xff);
			refuses(rom, "read as free space", [&] { m.install_placed(rom, d, t, ORG, "Test"); });
		}
		// a block reserved at the top of sram stops sram code below it
		{
			auto rom{ blank_rom() };
			const auto cfg{ config(rom) };
			fh::HackManager m;
			m.init_sram_state(cfg);
			require(m.reserve_sram_top(16) == 0x7ff0, "reserve: the top 16 bytes");
			const std::size_t room{ 0x7ff0 - 0x6806 };
			const auto p{ m.place_hack(cfg, rom, hack("sram=true"), "Test", ORG, room) };
			require(p.addr == 0x6806, "reserve: code up to the block is placed");
			refuses(rom, "do not fit the sram code area",
				[&] { m.place_hack(cfg, rom, hack("sram=true"), "Test", ORG, room + 1); });
			refuses(rom, "SRAM overflow when reserving", [&] { m.reserve_sram_top(room + 1); });
		}
		// the limit also stops code installed straight into sram
		{
			auto rom{ blank_rom() };
			const auto cfg{ config(rom) };
			fh::HackManager m;
			m.init_sram_state(cfg);
			m.reserve_sram_top(0x8000 - 0x6806 - 1);
			const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks("KillSwitch sram=true")) };
			refuses(rom, "SRAM overflow", [&] { m.install_general_hacks(cfg, rom, 15, ORG, 0xffe0, hacks, nullptr); });
		}
		std::cout << "hack_placement_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "hack_placement_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
