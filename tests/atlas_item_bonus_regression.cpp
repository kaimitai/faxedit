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

// runs install_AtlasDevItemBonus's hooks on a small 6502 for every equipment
// id and damage value, so what is checked is the arithmetic the hero gets,
// in bank 14 and from the sram code area
namespace {
	constexpr word ORG{ 0xbdb5 }, ATTACK{ 0x8891 }, ATTACK_RET{ 0x8894 }, DEFENSE{ 0x8ad4 }, REDUCE_HP{ 0xc08e };
	constexpr word SLOTS{ 0x03bd }, LIVE_WEAPON{ 0x03c8 }, ENEMY_HP{ 0x0344 }, DAMAGE{ 0x04bd }, FRACTION{ 0x04bc };
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	const std::vector<byte> ATTACK_ORIG{ 0xbd, 0x44, 0x03, 0x38, 0xe5, 0x00 };
	const std::vector<byte> DEFENSE_ORIG{ 0x4c, 0x8e, 0xc0 };
	const std::array<std::string, 6> NAMES{ "weapon_attack", "armor_attack", "shield_attack",
		"weapon_defense", "armor_defense", "shield_defense" };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	struct Cpu {
		std::array<byte, 0x10000> m{};
		byte a{}, x{}, y{}, p{}, sp{ 0xf0 };
		word pc{};
		std::vector<word> writes;

		byte get() { return m[pc++]; }
		word abs() { const byte lo{ get() }; return static_cast<word>(lo | get() << 8); }
		byte nz(byte v) { p = static_cast<byte>((p & ~0x82) | (v == 0 ? 2 : 0) | (v & 0x80)); return v; }
		void store(word at, byte v) { writes.push_back(at); m[at] = v; }
		void branch(bool taken) { const auto d{ static_cast<std::int8_t>(get()) }; if (taken) pc = static_cast<word>(pc + d); }
		void adc(byte v) { const unsigned s{ a + v + (p & 1u) }; p = static_cast<byte>((p & ~1) | (s > 255)); a = nz(static_cast<byte>(s)); }
		void sbc(byte v) { const int d{ a - v - (1 - (p & 1)) }; p = static_cast<byte>((p & ~1) | (d >= 0)); a = nz(static_cast<byte>(d)); }

		// only the instructions the hooks use; anything else fails the test
		void step() {
			const byte op{ get() };
			word at{};
			switch (op) {
			case 0xa9: a = nz(get()); break;
			case 0xa5: a = nz(m[get()]); break;
			case 0xad: a = nz(m[abs()]); break;
			case 0xbd: a = nz(m[static_cast<word>(abs() + x)]); break;
			case 0xac: y = nz(m[abs()]); break;
			case 0x85: store(get(), a); break;
			case 0x8d: store(abs(), a); break;
			case 0x29: a = nz(a & get()); break;
			case 0xc0: { const byte v{ get() }; p = static_cast<byte>((p & ~1) | (y >= v)); nz(static_cast<byte>(y - v)); break; }
			case 0x18: p &= ~1; break;
			case 0x38: p |= 1; break;
			case 0x79: adc(m[static_cast<word>(abs() + y)]); break;
			case 0xf9: sbc(m[static_cast<word>(abs() + y)]); break;
			case 0xf0: branch(p & 2); break;
			case 0xd0: branch(!(p & 2)); break;
			case 0xb0: branch(p & 1); break;
			case 0x90: branch(!(p & 1)); break;
			case 0x4c: pc = abs(); break;
			case 0x20: at = abs(); store(static_cast<word>(0x100 | sp--), static_cast<byte>((pc - 1) >> 8));
				store(static_cast<word>(0x100 | sp--), static_cast<byte>(pc - 1)); pc = at; break;
			case 0x60: { const byte lo{ m[0x100 | ++sp] }; pc = static_cast<word>((lo | m[0x100 | ++sp] << 8) + 1); break; }
			default: throw std::runtime_error(std::format("unexpected opcode ${:02x} at ${:04x}", op, pc - 1));
			}
		}
		void run(word p_entry, word p_stop) {
			pc = p_entry;
			for (int steps{ 0 }; pc != p_stop; ++steps) {
				require(steps < 200, "the hook does not return");
				step();
			}
			require(sp == 0xf0, "the stack is not balanced");
		}
	};

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		std::copy(bytes.begin(), bytes.end(), rom.begin() + off);
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, ATTACK, ATTACK_ORIG); put(rom, DEFENSE, DEFENSE_ORIG);
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

	// bank 14 as installed, and with sram the code area copied from its
	// backing image in bank 9 the way the boot code does
	Cpu load(const std::vector<byte>& rom, bool sram) {
		Cpu c;
		const auto bank14{ klib::Asm6502::get_file_offset(14, 0x8000) };
		std::copy_n(rom.begin() + bank14, 0x4000, c.m.begin() + 0x8000);
		if (sram) {
			const auto backing{ klib::Asm6502::get_file_offset(9, 0xa700) };
			std::copy_n(rom.begin() + backing, 0x200, c.m.begin() + 0x6806);
		}
		return c;
	}

	struct Hit { unsigned weapon, armor, shield, damage; };

	// the melee hit: $00 gains every valid slot's attack, at most 255; the
	// replayed load returns the enemy's hp, and x and y are as they came in
	void check_attack(Cpu& c, const Hit& h, const std::array<std::array<byte, 4>, 6>& t, bool on, const std::string& what) {
		c.m[SLOTS] = static_cast<byte>(h.weapon); c.m[SLOTS + 1] = static_cast<byte>(h.armor); c.m[SLOTS + 2] = static_cast<byte>(h.shield);
		c.m[0] = static_cast<byte>(h.damage); c.m[ENEMY_HP + 5] = 0x97; c.m[LIVE_WEAPON] = 0xa3;
		c.x = 5; c.y = 0xa3; c.a = 0x11; c.p = static_cast<byte>(h.damage & 1); c.writes.clear();
		c.run(ATTACK, ATTACK_RET);
		unsigned want{ h.damage };
		if (on)
			for (unsigned s{ 0 }; s < 3; ++s) {
				const unsigned id{ s == 0 ? h.weapon : s == 1 ? h.armor : h.shield };
				if (id < 4) want = std::min(255u, want + t[s][id]);
			}
		require(c.m[0] == want, std::format("{}: damage {} with ids {} {} {} became {}, not {}", what, h.damage,
			h.weapon, h.armor, h.shield, c.m[0], want));
		require(c.a == 0x97 && c.x == 5 && c.y == 0xa3, what + ": registers after the melee hook");
		for (word w : c.writes) require(w == 0 || (w >= 0x100 && w < 0x200), what + ": melee hook wrote elsewhere");
	}

	// the contact hit: the whole byte loses every valid slot's defense; below
	// zero is no damage at all, fraction included
	void check_defense(Cpu& c, const Hit& h, const std::array<std::array<byte, 4>, 6>& t, bool on, const std::string& what) {
		c.m[SLOTS] = static_cast<byte>(h.weapon); c.m[SLOTS + 1] = static_cast<byte>(h.armor); c.m[SLOTS + 2] = static_cast<byte>(h.shield);
		c.m[DAMAGE] = static_cast<byte>(h.damage); c.m[FRACTION] = 0x81;
		c.x = 5; c.p = static_cast<byte>(h.damage & 1); c.writes.clear();
		c.run(DEFENSE, REDUCE_HP);
		unsigned total{ 0 };
		if (on)
			for (unsigned s{ 0 }; s < 3; ++s) {
				const unsigned id{ s == 0 ? h.weapon : s == 1 ? h.armor : h.shield };
				if (id < 4) total += t[3 + s][id];
			}
		const unsigned want{ h.damage < total ? 0 : (h.damage - total) * 256 + 0x81 };
		const unsigned got{ c.m[DAMAGE] * 256u + c.m[FRACTION] };
		require(got == want, std::format("{}: damage {} with ids {} {} {} became ${:04x}, not ${:04x}", what, h.damage,
			h.weapon, h.armor, h.shield, got, want));
		require(c.x == 5, what + ": x after the contact hook");
		for (word w : c.writes) require(w == DAMAGE || w == FRACTION, what + ": contact hook wrote elsewhere");
	}

	std::string spec_of(const std::array<std::array<byte, 4>, 6>& t, const std::string& extra = "") {
		std::string spec{ "AtlasDevItemBonus" };
		for (std::size_t i{ 0 }; i < 6; ++i)
			if (t[i] != std::array<byte, 4>{})
				spec += std::format(" {}={}+{}+{}+{}", NAMES[i], t[i][0], t[i][1], t[i][2], t[i][3]);
		return spec + extra;
	}
}

int main() {
	try {
		const auto pristine{ vanilla_rom() };
		{
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevItemBonus") == 0 && rom == pristine, "no tables installs nothing");
			require(install(rom, "AtlasDevItemBonus mode=vanilla shield_attack=0+0+0+5 flag=30") == 0 && rom == pristine,
				"mode=vanilla installs nothing");
		}

		// one table at a time: every id, $ff included, against every damage
		const std::array<byte, 4> values{ 1, 7, 128, 255 };
		for (std::size_t i{ 0 }; i < 6; ++i) {
			std::array<std::array<byte, 4>, 6> t{};
			t[i] = values;
			auto rom{ vanilla_rom() };
			install(rom, spec_of(t));
			Cpu c{ load(rom, false) };
			for (unsigned id{ 0 }; id < 256; ++id)
				for (unsigned damage{ 0 }; damage < 256; ++damage) {
					Hit h{ 0xff, 0xff, 0xff, damage };
					(i % 3 == 0 ? h.weapon : i % 3 == 1 ? h.armor : h.shield) = id;
					if (i < 3) check_attack(c, h, t, true, NAMES[i]);
					else check_defense(c, h, t, true, NAMES[i]);
				}
		}

		// all six together, in bank 14 and in sram, with and without the flag:
		// each slot holds a valid id, an invalid one or nothing
		const std::array<std::array<byte, 4>, 6> all{ { { 200, 1, 0, 3 }, { 90, 0, 60, 2 }, { 7, 255, 0, 1 },
			{ 100, 0, 3, 1 }, { 1, 100, 0, 2 }, { 0, 9, 100, 3 } } };
		const std::array<unsigned, 6> ids{ 0, 1, 2, 3, 4, 0xff };
		for (const bool sram : { false, true })
			for (const bool flagged : { false, true }) {
				auto rom{ vanilla_rom() };
				const std::string spec{ spec_of(all, std::string(flagged ? " flag=30" : "") + (sram ? " sram=true" : "")) };
				const auto used{ install(rom, spec, sram) };
				require(used == (sram ? 0u : flagged ? 143u : 129u), std::format("{}: {} bytes of bank 14", spec, used));
				Cpu c{ load(rom, sram) };
				// flag 30 is bit 6 of $0104; the other bits of that byte do not count
				for (const byte flags : { byte{ 0x40 }, byte{ 0xbf }, byte{ 0x00 } }) {
					c.m[0x0104] = flags;
					const bool on{ !flagged || (flags & 0x40) != 0 };
					for (unsigned w : ids) for (unsigned a : ids) for (unsigned s : ids)
						for (unsigned damage{ 0 }; damage < 256; damage += 3) {
							check_attack(c, Hit{ w, a, s, damage }, all, on, spec);
							check_defense(c, Hit{ w, a, s, damage }, all, on, spec);
						}
				}
			}

		// the documented example
		{
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevItemBonus shield_attack=0+0+5+0 shield_defense=0+0+3+0") == 61,
				"the example uses 61 bytes of bank 14");
		}

		// refusals leave the rom as it was
		for (const char* bad : { "AtlasDevItemBonus shield_attack=1", "AtlasDevItemBonus shield_attack=1+2+3",
			"AtlasDevItemBonus shield_attack=1+2+3+4+5", "AtlasDevItemBonus shield_attack=0+0+0+256",
			"AtlasDevItemBonus flag=30", "AtlasDevItemBonus shield_attack=0+0+0+5 flag=248",
			"AtlasDevItemBonus shield_attack=0+0+0+5 mode=on", "AtlasDevItemBonus mode=vanilla flag=248",
			"AtlasDevItemBonus shield_attack=0+0+0+5 sram=true" })
			refused(pristine, bad);
		const std::string both{ "AtlasDevItemBonus shield_attack=0+0+0+5 shield_defense=0+0+0+3" };
		for (word site : { word{ 0x8891 }, word{ 0x8892 }, word{ 0x8893 }, word{ 0x8894 }, word{ 0x8895 }, word{ 0x8896 },
			word{ 0x8ad4 }, word{ 0x8ad5 }, word{ 0x8ad6 } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, site)] ^= 1;
			refused(rom, both);
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, ORG + 60)] = 0;
			refused(rom, both);
			rom = vanilla_rom();
			install(rom, both);
			refused(rom, both);
		}
		std::cout << "atlas_item_bonus_regression: ok\n";
		return 0;
	}
	catch (const std::exception& ex) {
		std::cerr << "atlas_item_bonus_regression: " << ex.what() << '\n';
		return 1;
	}
}
