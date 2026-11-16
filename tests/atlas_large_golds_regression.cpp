#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// runs the game's two gold routines on a small 6502, as the us rom has them
// and with install_AtlasDevLargeGolds's hooks: an amount is taken from or
// given to the gold a step at a time, and what is checked is the gold, the
// frames, the counters left behind and the stack, for amounts from nothing to
// the most the routines can hold, in bank 12 and from the sram code area
namespace {
	constexpr word ORG{ 0xb000 };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	// the stock routines: ProgressivelySubtractGold at $8321 and ProgressivelyAddGold at $8537
	const std::string SPEND_HEX{ "20b087ad0302302c0d0202f01fad020238e90a8d0202ad0302e9008d0302a90a85eca90085ed20a5f94c2183ce0302a93c8d0202ce0202d0c760" };
	const std::string ADD_HEX{ "20b087ad0302302f0d0202f022207485ad020238e90a8d0202ad0302e9008d0302a90a85eca90085ed20c1f94c3785ce0302a93c8d0202ce0202d0c460" };
	constexpr word SPEND{ 0x8321 }, ADD{ 0x8537 }, PORTRAIT{ 0x87b0 }, SOUND{ 0x8574 }, SUBTRACT_GOLD{ 0xf9a5 }, ADD_GOLD{ 0xf9c1 }, SENTINEL{ 0x0400 };
	constexpr word TEMP_LO{ 0x0202 }, TEMP_HI{ 0x0203 }, FLAGS{ 0x0101 };
	constexpr byte ZP_EC{ 0xec }, ZP_ED{ 0xed };
	constexpr unsigned START_GOLD{ 500000 };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		std::copy(bytes.begin(), bytes.end(), rom.begin() + klib::Asm6502::get_file_offset(12, cpu));
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, SPEND, from_hex(SPEND_HEX)); put(rom, ADD, from_hex(ADD_HEX));
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
		const auto hacks{ fh::filter_general_hacks(12, fh::parse_general_hacks(sram ? spec + " sram=true" : spec)) };
		fh::HackManager manager;
		const auto config{ us_config(rom) };
		if (sram) manager.init_sram_state(config);
		return manager.install_general_hacks(config, rom, 12, ORG, 0xc000, hacks, nullptr).bank_used;
	}

	void refused(const std::vector<byte>& rom, const std::string& spec) {
		auto copy{ rom };
		bool threw{ false };
		try { install(copy, spec); } catch (const std::exception&) { threw = true; }
		require(threw, "accepted: " + spec);
		require(copy == rom, "a refusal changed the rom: " + spec);
	}

	struct Cpu {
		std::array<byte, 0x10000> m{};
		byte a{}, x{}, y{}, p{}, sp{ 0xf0 };
		word pc{};
		std::map<word, std::function<void()>> hooks;
		bool stop{ false };

		byte get() { return m[pc++]; }
		word abs() { const byte lo{ get() }; return static_cast<word>(lo | get() << 8); }
		byte nz(byte v) { p = static_cast<byte>((p & ~0x82) | (v == 0 ? 2 : 0) | (v & 0x80)); return v; }
		void push(byte v) { m[0x100 | sp--] = v; }
		byte pull() { return m[0x100 | ++sp]; }
		void branch(bool taken) { const auto d{ static_cast<std::int8_t>(get()) }; if (taken) pc = static_cast<word>(pc + d); }
		void compare(byte r, byte v) { p = static_cast<byte>((p & ~1) | (r >= v)); nz(static_cast<byte>(r - v)); }
		void sbc(byte v) { const int d{ a - v - (1 - (p & 1)) }; p = static_cast<byte>((p & ~1) | (d >= 0)); a = nz(static_cast<byte>(d)); }
		void rts() { const byte lo{ pull() }; pc = static_cast<word>((lo | pull() << 8) + 1); }

		// only the instructions the routines and the hooks use; anything else fails the test
		void step() {
			const byte op{ get() };
			word at{};
			switch (op) {
			case 0xa9: a = nz(get()); break;
			case 0xad: a = nz(m[abs()]); break;
			case 0x85: m[get()] = a; break;
			case 0x8d: m[abs()] = a; break;
			case 0xea: break;
			case 0x18: p &= ~1; break;
			case 0x38: p |= 1; break;
			case 0x29: a = nz(a & get()); break;
			case 0x0d: a = nz(a | m[abs()]); break;
			case 0xc9: compare(a, get()); break;
			case 0xe9: sbc(get()); break;
			case 0xce: at = abs(); nz(--m[at]); break;
			case 0x10: branch(!(p & 0x80)); break;
			case 0x30: branch(p & 0x80); break;
			case 0xb0: branch(p & 1); break;
			case 0xd0: branch(!(p & 2)); break;
			case 0xf0: branch(p & 2); break;
			case 0x4c: pc = abs(); break;
			case 0x20: at = abs(); push(static_cast<byte>((pc - 1) >> 8)); push(static_cast<byte>(pc - 1)); pc = at; break;
			case 0x60: rts(); break;
			default: throw std::runtime_error(std::format("unexpected opcode ${:02x} at ${:04x}", op, pc - 1));
			}
		}
	};

	struct Outcome {
		long long gold_change{ 0 };    // what the gold moved by: negative for a payment
		int frames{ 0 }, sounds{ 0 };
		unsigned left{ 0 };            // the counters, as the game reads them afterwards
		bool balanced{ false };
	};

	// one run of a routine for an amount: from the call to the return
	Outcome run(const std::vector<byte>& rom, word p_routine, unsigned p_amount, int p_flag_set = -1, bool sram = false) {
		Cpu c;
		const auto bank12{ klib::Asm6502::get_file_offset(12, 0x8000) };
		std::copy_n(rom.begin() + bank12, 0x4000, c.m.begin() + 0x8000);
		if (sram) {
			const auto backing{ klib::Asm6502::get_file_offset(9, 0xa700) };
			std::copy_n(rom.begin() + backing, 0x200, c.m.begin() + 0x6806);
		}
		if (p_flag_set >= 0) c.m[FLAGS + (p_flag_set >> 3)] |= static_cast<byte>(1 << (p_flag_set & 7));
		Outcome o;
		long long gold{ START_GOLD };
		auto amount = [&] { return static_cast<long long>(c.m[ZP_EC] | c.m[ZP_ED] << 8); };
		auto hook = [&](word at, std::function<void()> fn) { c.hooks[at] = [&c, fn] { fn(); c.rts(); }; };
		hook(PORTRAIT, [&] { ++o.frames; });
		hook(SOUND, [&] { ++o.sounds; });
		hook(SUBTRACT_GOLD, [&] { gold -= amount(); });
		hook(ADD_GOLD, [&] { gold += amount(); });
		c.hooks[SENTINEL] = [&] { c.stop = true; };
		c.m[TEMP_LO] = p_amount & 0xff; c.m[TEMP_HI] = static_cast<byte>(p_amount >> 8);
		const byte before{ c.sp };
		c.push(static_cast<byte>((SENTINEL - 1) >> 8)); c.push(static_cast<byte>(SENTINEL - 1));
		c.pc = p_routine;
		for (int steps{ 0 }; !c.stop; ++steps) {
			require(steps < 6000000, "the routine does not come back");
			if (auto it{ c.hooks.find(c.pc) }; it != c.hooks.end()) { it->second(); continue; }
			c.step();
		}
		o.gold_change = gold - static_cast<long long>(START_GOLD);
		o.left = c.m[TEMP_LO] | c.m[TEMP_HI] << 8;
		o.balanced = c.sp == before;
		return o;
	}

	// the amount a routine moves, in tens as it counts, for the stock routine and below 32768
	long long stock_amount(unsigned p_amount) { return (p_amount + 9) / 10 * 10; }
	constexpr unsigned CHUNK{ 20480 };
}

int main() {
	try {
		const auto stock{ vanilla_rom() };
		const std::vector<unsigned> edges{ 0, 10, 160, 5600, 12500, 32760, 32770, 32780, 40000, 49150, 49160, 65270 };
		// the control: the stock routines, and the bug in them
		for (unsigned amount : edges) {
			const auto pay{ run(stock, SPEND, amount) }, get{ run(stock, ADD, amount) };
			if (amount < 32768) {
				require(pay.gold_change == -stock_amount(amount) && pay.frames == static_cast<int>(stock_amount(amount) / 10 + 60) && pay.balanced,
					std::format("stock pays {}: {} in {} frames", amount, pay.gold_change, pay.frames));
				require(get.gold_change == stock_amount(amount) && get.sounds == static_cast<int>(stock_amount(amount) / 10), std::format("stock gives {}", amount));
			}
			else {
				require(pay.gold_change == 0 && get.gold_change == 0, std::format("stock takes and gives nothing for {}", amount));
			}
		}
		// the fix
		{
			auto rom{ stock };
			require(install(rom, "AtlasDevLargeGolds") == 77, "both routines take 77 bytes");
			std::vector<unsigned> amounts{ edges };
			for (unsigned a : { 32768u, 32767u, 20480u, 40960u, 40970u, 61440u, 61450u, 65279u, 65280u, 65281u, 65535u, 1u, 9u, 11u }) amounts.push_back(a);
			unsigned seed{ 20261016 };
			for (int i{ 0 }; i < 150; ++i) { seed = seed * 1103515245u + 12345u; amounts.push_back(((seed >> 16) % 6528) * 10); }
			for (unsigned amount : amounts) {
				const auto pay{ run(rom, SPEND, amount) }, get{ run(rom, ADD, amount) };
				const auto was_pay{ run(stock, SPEND, amount) }, was_get{ run(stock, ADD, amount) };
				const std::string what{ std::format("amount {}", amount) };
				require(pay.balanced && get.balanced, what + ": the stack");
				if (amount < 32768 || amount >= 65280) {
					// what the stock routines do, to the frame; the few amounts at the top are still not taken
					require(pay.gold_change == was_pay.gold_change && pay.frames == was_pay.frames && pay.left == was_pay.left, what + ": the payment is not the stock one");
					require(get.gold_change == was_get.gold_change && get.frames == was_get.frames && get.left == was_get.left && get.sounds == was_get.sounds, what + ": the reward is not the stock one");
				}
				else {
					// 20480 off at once as many times as it takes, and the stock routine, run for real, on what is left
					unsigned chunks{ 0 }, rest{ amount };
					while ((rest >> 8) >= 0x80) { rest -= CHUNK; ++chunks; }
					const auto rest_pay{ run(stock, SPEND, rest) }, rest_get{ run(stock, ADD, rest) };
					require(pay.gold_change == -static_cast<long long>(chunks) * CHUNK + rest_pay.gold_change && pay.frames == rest_pay.frames && pay.left == rest_pay.left,
						std::format("{}: paid {} in {} frames", what, -pay.gold_change, pay.frames));
					require(get.gold_change == static_cast<long long>(chunks) * CHUNK + rest_get.gold_change && get.frames == rest_get.frames && get.left == rest_get.left &&
						get.sounds == static_cast<int>(chunks) + rest_get.sounds, std::format("{}: given {}", what, get.gold_change));
					if (amount % 10 == 0) require(pay.gold_change == -static_cast<long long>(amount), what + ": a multiple of ten is taken exactly");
				}
			}
		}
		// one of the two, and the flag
		{
			auto rom{ stock };
			require(install(rom, "AtlasDevLargeGolds add=false") == 37, "spend only: 37 bytes");
			require(run(rom, SPEND, 40000).gold_change == -40000 && run(rom, ADD, 40000).gold_change == 0, "spend only fixes the payment alone");
			auto both_off{ stock };
			install(both_off, "AtlasDevLargeGolds spend=false");
			require(run(both_off, ADD, 40000).gold_change == 40000 && run(both_off, SPEND, 40000).gold_change == 0, "add only fixes the reward alone");
			auto flagged{ stock };
			{ const auto n{ install(flagged, "AtlasDevLargeGolds flag=40") }; require(n == 91, std::format("both with a flag: {} bytes", n)); }
			require(run(flagged, SPEND, 40000).gold_change == 0 && run(flagged, ADD, 40000).gold_change == 0, "flag clear: the stock routines");
			require(run(flagged, SPEND, 65270, 40).gold_change == -65270 && run(flagged, ADD, 65270, 40).gold_change == 65270, "flag set: the fix, two steps of 20480");
			require(run(flagged, SPEND, 160).frames == 76 && run(flagged, SPEND, 160, 40).frames == 76, "a small payment takes the same frames either way");
		}
		// from the sram code area
		{
			auto rom{ stock };
			install(rom, "AtlasDevLargeGolds flag=40", true);
			require(run(rom, SPEND, 40000, 40, true).gold_change == -40000 && run(rom, ADD, 40000, 40, true).gold_change == 40000, "sram: the fix");
			require(run(rom, SPEND, 40000, -1, true).gold_change == 0, "sram: flag clear is the stock routine");
			const auto o{ klib::Asm6502::get_file_offset(12, SPEND) };
			require(rom[o] == 0x4c && (rom[o + 2] << 8 | rom[o + 1]) >= 0x6806 && (rom[o + 2] << 8 | rom[o + 1]) < 0x8000, "sram: the hook goes to the sram code area");
		}
		// refusals leave the rom as it was
		{
			for (const char* bad : { "spend=false add=false", "flag=248", "mode=on", "mode=vanilla spend=false add=false", "mode=vanilla flag=248", "speed=3", "sram=true" })
				refused(stock, std::string("AtlasDevLargeGolds ") + bad);
			auto rom{ stock };
			require(install(rom, "AtlasDevLargeGolds mode=vanilla") == 0 && rom == stock, "vanilla installs nothing");
			// every instruction the hack reads must be the stock one
			const std::vector<std::pair<word, std::vector<unsigned>>> reads{ { SPEND, { 0, 3, 6, 32, 36, 38 } }, { ADD, { 0, 3, 6, 13, 35, 39, 41 } } };
			for (const auto& [routine, offsets] : reads)
				for (unsigned off : offsets) {
					auto bad{ stock };
					bad[klib::Asm6502::get_file_offset(12, static_cast<word>(routine + off))] ^= 0x01;
					try { refused(bad, "AtlasDevLargeGolds"); }
					catch (const std::exception& e) { throw std::runtime_error(std::format("{} (routine ${:04x} byte {})", e.what(), routine, off)); }
				}
			auto occupied{ stock };
			occupied[klib::Asm6502::get_file_offset(12, ORG + 5)] = 0x00;
			refused(occupied, "AtlasDevLargeGolds");
			auto twice{ stock };
			install(twice, "AtlasDevLargeGolds");
			refused(twice, "AtlasDevLargeGolds");
			auto only_add{ stock };
			install(only_add, "AtlasDevLargeGolds add=false");
			refused(only_add, "AtlasDevLargeGolds add=false");
			// the payment routine already hooked by another hack, as a replacement countdown would: the fix for it is refused, and the other half still works
			auto hooked{ stock };
			put(hooked, 0xb800, from_hex("20b087" "4c2483"));
			put(hooked, SPEND, from_hex("4c00b8"));
			refused(hooked, "AtlasDevLargeGolds");
			install(hooked, "AtlasDevLargeGolds spend=false");
			require(run(hooked, ADD, 40000).gold_change == 40000, "the reward fix works beside a hooked payment routine");
		}
		std::cout << "atlas_large_golds_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_large_golds_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
