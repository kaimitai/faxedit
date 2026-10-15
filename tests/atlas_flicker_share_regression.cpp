#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// runs the stock entity loop at $800f with install_AtlasDevFlickerShare's
// hooks on a small 6502, the draw at $8bd2 replaced by a 64 entry budget, and
// checks the order the slots are drawn in, pass after pass, against the rule:
// stock order after a pass that fits, the other way round after one that
// overflowed. bank 14 and the sram code area both run.
namespace {
	constexpr word ORG{ 0xbdb5 }, LOOP{ 0x800f }, DRAW{ 0x8bd2 }, SLOT{ 0x0378 }, STATE{ 0x04ff };
	constexpr byte OVERFLOW{ 0x39 };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, BUDGET{ 64 };
	// $800f-$804d as in every retail rom
	const std::vector<byte> LOOP_ORIG{
		0xa2, 0x07, 0x8e, 0x78, 0x03, 0xbd, 0xcc, 0x02, 0x30, 0x2a, 0xad, 0x26, 0x04, 0xd0, 0x22, 0xbd,
		0x4c, 0x03, 0xd0, 0x2b, 0xbd, 0x34, 0x03, 0x30, 0x06, 0x20, 0x51, 0x81, 0x4c, 0x40, 0x80, 0x20,
		0x6b, 0xa6, 0x20, 0x04, 0x88, 0x20, 0xdc, 0x8a, 0x20, 0x08, 0x8a, 0x20, 0x7c, 0x87, 0x20, 0x0a,
		0x89, 0x20, 0xd2, 0x8b, 0xae, 0x78, 0x03, 0xca, 0x10, 0xc8, 0xa9, 0x00, 0x85, 0x33, 0x60 };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	struct Cpu {
		std::array<byte, 0x10000> m{};
		byte a{}, x{}, y{}, p{}, sp{ 0xf0 };
		word pc{};
		std::vector<word> writes;
		std::array<unsigned, 8> demand{};
		unsigned left{};
		std::vector<unsigned> order;
		std::array<unsigned, 8> drawn{};

		byte get() { return m[pc++]; }
		word abs() { const byte lo{ get() }; return static_cast<word>(lo | get() << 8); }
		byte nz(byte v) { p = static_cast<byte>((p & ~0x82) | (v == 0 ? 2 : 0) | (v & 0x80)); return v; }
		void store(word at, byte v) { writes.push_back(at); m[at] = v; }
		void branch(bool taken) { const auto d{ static_cast<std::int8_t>(get()) }; if (taken) pc = static_cast<word>(pc + d); }
		void compare(byte r, byte v) { p = static_cast<byte>((p & ~1) | (r >= v)); nz(static_cast<byte>(r - v)); }
		void ret() { const byte lo{ m[0x100 | ++sp] }; pc = static_cast<word>((lo | m[0x100 | ++sp] << 8) + 1); }

		// the budgeted draw: the slot gets what is left, and every entry that
		// does not fit counts in $39 the way the allocator's inc $39 does
		void draw() {
			const unsigned k{ m[SLOT] };
			const unsigned got{ std::min(demand[k], left) };
			drawn[k] = got; left -= got; order.push_back(k);
			for (unsigned i{ got }; i < demand[k]; ++i) store(OVERFLOW, static_cast<byte>(m[OVERFLOW] + 1));
		}

		void step() {
			if (pc == DRAW) { draw(); ret(); return; }
			const byte op{ get() };
			word at{};
			switch (op) {
			case 0xa9: a = nz(get()); break;
			case 0xa5: a = nz(m[get()]); break;
			case 0xad: a = nz(m[abs()]); break;
			case 0xbd: a = nz(m[static_cast<word>(abs() + x)]); break;
			case 0xa2: x = nz(get()); break;
			case 0xae: x = nz(m[abs()]); break;
			case 0x8d: store(abs(), a); break;
			case 0x8e: store(abs(), x); break;
			case 0x85: store(get(), a); break;
			case 0x29: a = nz(a & get()); break;
			case 0x09: a = nz(a | get()); break;
			case 0x49: a = nz(a ^ get()); break;
			case 0x4a: p = static_cast<byte>((p & ~1) | (a & 1)); a = nz(static_cast<byte>(a >> 1)); break;
			case 0xe0: compare(x, get()); break;
			case 0xe8: x = nz(static_cast<byte>(x + 1)); break;
			case 0xca: x = nz(static_cast<byte>(x - 1)); break;
			case 0xea: break;
			case 0xf0: branch(p & 2); break;
			case 0xd0: branch(!(p & 2)); break;
			case 0x30: branch(p & 0x80); break;
			case 0x10: branch(!(p & 0x80)); break;
			case 0xb0: branch(p & 1); break;
			case 0x90: branch(!(p & 1)); break;
			case 0x4c: pc = abs(); break;
			case 0x20: at = abs(); store(static_cast<word>(0x100 | sp--), static_cast<byte>((pc - 1) >> 8));
				store(static_cast<word>(0x100 | sp--), static_cast<byte>(pc - 1)); pc = at; break;
			case 0x60: ret(); break;
			default: throw std::runtime_error(std::format("unexpected opcode ${:02x} at ${:04x}", op, pc - 1));
			}
		}

		// one entity pass, called the way $8000 falls into it; $39 and the
		// sprite count start at zero every frame
		void pass() {
			m[OVERFLOW] = 0; left = BUDGET; order.clear(); drawn = {};
			for (unsigned k{ 0 }; k < 8; ++k) m[0x02cc + k] = demand[k] == 0 ? 0xff : 0x33;
			m[0x0426] = 1;  // straight to the draw
			store(static_cast<word>(0x100 | sp--), 0xff); store(static_cast<word>(0x100 | sp--), 0xfe);
			pc = LOOP;
			for (int steps{ 0 }; pc != 0xffff; ++steps) {
				require(steps < 2000, "the loop does not end");
				step();
			}
			require(sp == 0xf0, "the stack is not balanced");
		}
	};

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		std::copy(bytes.begin(), bytes.end(), rom.begin() + klib::Asm6502::get_file_offset(14, cpu));
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, LOOP, LOOP_ORIG);
		return rom;
	}

	fe::Config us_config(const std::vector<byte>& rom) {
		fe::Config config;
		config.load_definitions(EOE_TEST_CONFIG_PATH, "");
		config.set_region("us");
		config.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		return config;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec, bool sram = false) {
		const auto hacks{ fh::filter_general_hacks(14, fh::parse_general_hacks(spec)) };
		fh::HackManager manager;
		if (!sram) return manager.install_general_hacks(fe::Config{}, rom, 14, ORG, 0xc000, hacks, nullptr).bank_used;
		const auto config{ us_config(rom) };
		manager.init_sram_state(config);
		return manager.install_general_hacks(config, rom, 14, ORG, 0xc000, hacks, nullptr).bank_used;
	}

	void refused(const std::vector<byte>& rom, const std::string& spec) {
		auto copy{ rom };
		bool threw{ false };
		try { install(copy, spec); } catch (const std::exception&) { threw = true; }
		require(threw, "accepted: " + spec);
		require(copy == rom, "a refusal changed the rom: " + spec);
	}

	Cpu load(const std::vector<byte>& rom, bool sram) {
		Cpu c;
		std::copy_n(rom.begin() + klib::Asm6502::get_file_offset(14, 0x8000), 0x4000, c.m.begin() + 0x8000);
		if (sram)
			std::copy_n(rom.begin() + klib::Asm6502::get_file_offset(9, 0xa700), 0x100, c.m.begin() + 0x6806);
		c.m[0xffff] = 0x00;
		return c;
	}

	// a run of passes over changing crowds: each pass must draw every slot
	// once, 7 to 0 unless the pass before it overflowed and the switch is on,
	// in which case it goes the other way from the pass before
	void run(Cpu& c, bool on, const std::string& what) {
		const std::vector<std::array<unsigned, 8>> crowds{
			{ 0, 0, 0, 0, 0, 0, 50, 50 },   // two Shadow Euras
			{ 9, 0, 9, 0, 9, 0, 9, 9 },     // fits
			{ 20, 20, 0, 0, 20, 0, 20, 0 }, // just over
			{ 0, 0, 0, 0, 0, 0, 0, 0 },
			{ 64, 0, 0, 0, 0, 0, 0, 1 },
		};
		bool overflowed{ false }, up{ false };
		for (const auto& crowd : crowds)
			for (int n{ 0 }; n < 9; ++n) {
				c.demand = crowd;
				c.writes.clear();
				c.pass();
				up = on && overflowed ? !up : false;
				std::vector<unsigned> want;
				for (unsigned i{ 0 }; i < 8; ++i) {
					const unsigned k{ up ? i : 7 - i };
					if (crowd[k] != 0) want.push_back(k);
				}
				require(c.order == want, std::format("{}: pass {} of a crowd drew in the wrong order", what, n));
				unsigned demand{ 0 };
				for (unsigned d : crowd) demand += d;
				overflowed = demand > BUDGET;
				for (word w : c.writes)
					require(w == STATE || w == SLOT || w == OVERFLOW || w == 0x33 || (w >= 0x100 && w < 0x200),
						std::format("{}: wrote ${:04x}", what, w));
				require((c.m[STATE] & 0x7e) == 0, what + ": only bits 7 and 0 of the state are used");
			}
	}
}

int main() {
	try {
		const auto pristine{ vanilla_rom() };
		{
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevFlickerShare mode=vanilla flag=30") == 0 && rom == pristine,
				"mode=vanilla installs nothing");
		}
		{
			// the stock loop starves slot 6 behind slot 7 on every pass
			Cpu c{ load(pristine, false) };
			c.demand = { 0, 0, 0, 0, 0, 0, 50, 50 };
			for (int n{ 0 }; n < 4; ++n) {
				c.pass();
				require(c.drawn[7] == 50 && c.drawn[6] == 14, "the stock loop is not the one measured");
			}
		}
		for (const bool sram : { false, true }) {
			const std::string where{ sram ? "sram" : "bank 14" };
			{
				auto rom{ vanilla_rom() };
				require(install(rom, std::string("AtlasDevFlickerShare") + (sram ? " sram=true" : ""), sram) == (sram ? 0u : 64u),
					where + ": 64 bytes of bank 14");
				Cpu c{ load(rom, sram) };
				run(c, true, where);
				// the two Euras: each is whole on alternate passes
				c.demand = { 0, 0, 0, 0, 0, 0, 50, 50 };
				c.m[STATE] = 0;
				c.pass();
				for (int n{ 0 }; n < 6; ++n) {
					c.pass();
					require(c.drawn[n % 2 == 0 ? 6 : 7] == 50, where + ": the Euras do not take turns");
				}
			}
			{
				// flag 30 is bit 6 of $0104; the other bits of that byte do not count
				auto rom{ vanilla_rom() };
				require(install(rom, std::string("AtlasDevFlickerShare flag=30") + (sram ? " sram=true" : ""), sram) == (sram ? 0u : 71u),
					where + ": 71 bytes of bank 14 with a flag");
				Cpu c{ load(rom, sram) };
				c.m[0x0104] = 0xbf;
				run(c, false, where + ", flag clear");
				c.m[0x0104] = 0x40;
				c.m[STATE] = 0;
				run(c, true, where + ", flag set");
			}
		}

		// refusals leave the rom as it was
		for (const char* bad : { "AtlasDevFlickerShare flag=248", "AtlasDevFlickerShare mode=on",
			"AtlasDevFlickerShare sram=true", "AtlasDevFlickerShare mode=vanilla flag=248" })
			refused(pristine, bad);
		for (word site : { word{ 0x800f }, word{ 0x8011 }, word{ 0x8013 }, word{ 0x8014 }, word{ 0x8016 },
			word{ 0x8043 }, word{ 0x8046 }, word{ 0x8048 }, word{ 0x8049 }, word{ 0x804d } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, site)] ^= 1;
			refused(rom, "AtlasDevFlickerShare");
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, ORG + 63)] = 0;
			refused(rom, "AtlasDevFlickerShare");
			rom = vanilla_rom();
			install(rom, "AtlasDevFlickerShare");
			refused(rom, "AtlasDevFlickerShare");
		}
		std::cout << "atlas_flicker_share_regression: ok\n";
		return 0;
	}
	catch (const std::exception& ex) {
		std::cerr << "atlas_flicker_share_regression: " << ex.what() << '\n';
		return 1;
	}
}
