#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevEnemyStats. every knob rewrites bank 14 table bytes
// or one operand in place, so a default install writes nothing and consumes
// no cursor space, a scaled table keeps its zero entries, saturates at 255
// and never drops a live entry below one, and a rom whose tables are not
// vanilla is refused.
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	struct Table { word addr; std::size_t length; std::array<byte, 8> head; };
	constexpr std::array<Table, 4> TABLES{ {
		{ 0xb5a9, 101, { 0x00, 0x00, 0x00, 0x00, 0x12, 0x18, 0x0e, 0x03 } },
		{ 0xb6d7, 100, { 0x00, 0x00, 0x00, 0x1f, 0x07, 0x09, 0x06, 0x03 } },
		{ 0xb60e, 100, { 0x00, 0x00, 0x00, 0x00, 0x23, 0x37, 0x19, 0x19 } },
		{ 0xaced,  48, { 0x0a, 0x0f, 0x12, 0x14, 0x16, 0x1a, 0x20, 0x35 } },
	} };
	constexpr std::array<byte, 5> STAGGER{ 0xa9, 0x08, 0x9d, 0x4c, 0x03 };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }
	std::size_t off14(word cpu) { return klib::Asm6502::get_file_offset(14, cpu); }

	// the four table heads, a deterministic spread of values after them, the
	// bread entries after the gold table, and the stagger site
	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		const std::array<byte, 8> spread{ 0, 1, 7, 40, 100, 200, 250, 255 };
		for (const Table& t : TABLES) {
			for (std::size_t i{ 0 }; i < t.length; ++i)
				rom[off14(t.addr) + i] = i < 8 ? t.head[i] : spread[i % 8];
		}
		for (std::size_t i{ 0 }; i < 16; ++i) rom[off14(0xaced) + 48 + i] = 0x08 + static_cast<byte>(i);
		for (std::size_t i{ 0 }; i < STAGGER.size(); ++i) rom[off14(0x8867) + i] = STAGGER[i];
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

	byte scaled(byte v, unsigned pct) {
		if (v == 0) return 0;
		const unsigned s{ (v * pct + 50) / 100 };
		return static_cast<byte>(s < 1 ? 1 : s > 255 ? 255 : s);
	}

	void test_defaults_write_nothing() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		require(install(rom, "AtlasDevEnemyStats") == 0, "default install consumed cursor space");
		require(rom == before, "default install modified the rom");
	}

	void test_each_table_scales_alone() {
		const std::array<const char*, 4> names{ "hp", "damage", "xp", "gold" };
		for (std::size_t k{ 0 }; k < 4; ++k) {
			auto rom{ vanilla_rom() };
			const auto before{ rom };
			require(install(rom, std::string{ "AtlasDevEnemyStats " } + names[k] + "=150") == 0,
				"a scaled install consumed cursor space");
			for (std::size_t t{ 0 }; t < 4; ++t)
				for (std::size_t i{ 0 }; i < TABLES[t].length; ++i) {
					const byte want{ t == k ? scaled(before[off14(TABLES[t].addr) + i], 150)
						: before[off14(TABLES[t].addr) + i] };
					require(rom[off14(TABLES[t].addr) + i] == want,
						std::string{ names[k] } + " scaled the wrong bytes");
				}
			// nothing outside the tables moves
			std::size_t changed{ 0 };
			for (std::size_t i{ 0 }; i < rom.size(); ++i) if (rom[i] != before[i]) ++changed;
			std::size_t inside{ 0 };
			for (std::size_t i{ 0 }; i < TABLES[k].length; ++i)
				if (rom[off14(TABLES[k].addr) + i] != before[off14(TABLES[k].addr) + i]) ++inside;
			require(changed == inside, std::string{ names[k] } + " wrote outside its table");
		}
	}

	void test_scaling_rules() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevEnemyStats hp=400");
		const auto hp{ off14(0xb5a9) };
		require(rom[hp + 0] == 0 && rom[hp + 3] == 0, "a zero entry did not stay zero");
		require(rom[hp + 4] == 72, "18 at 400 percent is 72");
		require(rom[hp + 14] == 255, "250 at 400 percent saturates");
		auto low{ vanilla_rom() };
		install(low, "AtlasDevEnemyStats hp=1");
		require(low[hp + 9] == 1, "a live entry never drops below one");
	}

	void test_gold_leaves_bread_alone() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevEnemyStats gold=200");
		for (std::size_t i{ 0 }; i < 16; ++i)
			require(rom[off14(0xaced) + 48 + i] == before[off14(0xaced) + 48 + i], "bread was scaled");
	}

	void test_stagger_is_one_operand() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevEnemyStats stagger=16");
		std::size_t changed{ 0 };
		for (std::size_t i{ 0 }; i < rom.size(); ++i) if (rom[i] != before[i]) ++changed;
		require(changed == 1 && rom[off14(0x8867) + 1] == 16, "stagger operand");
	}

	// a profile sets all five knobs; an explicit knob overrides it
	void test_profiles() {
		struct P { const char* name; unsigned hp, damage, xp, gold; byte stagger; };
		for (const P& p : { P{ "easy", 75, 75, 150, 150, 12 }, P{ "hard", 150, 150, 100, 100, 6 },
			P{ "nightmare", 200, 200, 75, 75, 4 }, P{ "grind", 100, 100, 200, 200, 8 } }) {
			auto rom{ vanilla_rom() };
			const auto before{ rom };
			require(install(rom, std::string{ "AtlasDevEnemyStats profile=" } + p.name) == 0, "profile used cursor");
			const std::array<unsigned, 4> pct{ p.hp, p.damage, p.xp, p.gold };
			for (std::size_t t{ 0 }; t < 4; ++t)
				for (std::size_t i{ 0 }; i < TABLES[t].length; ++i)
					require(rom[off14(TABLES[t].addr) + i] == scaled(before[off14(TABLES[t].addr) + i], pct[t]),
						std::string{ "profile " } + p.name + " table bytes");
			require(rom[off14(0x8867) + 1] == p.stagger, std::string{ "profile " } + p.name + " stagger");
		}
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevEnemyStats profile=normal");
		require(rom == before, "profile normal wrote bytes");
		auto over{ vanilla_rom() };
		install(over, "AtlasDevEnemyStats profile=hard hp=99");
		require(over[off14(0xb5a9) + 4] == scaled(before[off14(0xb5a9) + 4], 99), "explicit hp did not override the profile");
		require(over[off14(0xb6d7) + 4] == scaled(before[off14(0xb6d7) + 4], 150), "profile damage lost under an override");
	}

	void test_refusals() {
		for (const auto& spec : { "AtlasDevEnemyStats hp=0", "AtlasDevEnemyStats hp=401", "AtlasDevEnemyStats profile=brutal",
			"AtlasDevEnemyStats damage=0", "AtlasDevEnemyStats stagger=0", "AtlasDevEnemyStats stagger=256" }) {
			auto rom{ vanilla_rom() };
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, spec); }
			catch (const std::runtime_error& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevEnemyStats") != std::string::npos, "refusal does not name the hack");
			}
			require(threw, std::string{ "accepted " } + spec);
			require(rom == before, std::string{ "a refused install wrote bytes: " } + spec);
		}
	}

	void test_refuses_a_disturbed_table() {
		for (const word cpu : { word{ 0xb5ad }, word{ 0xb6da }, word{ 0xb612 }, word{ 0xaced }, word{ 0x8868 } }) {
			auto rom{ vanilla_rom() };
			rom[off14(cpu)] ^= 0x01;
			bool threw{ false };
			try { install(rom, "AtlasDevEnemyStats hp=120"); }
			catch (const std::runtime_error&) { threw = true; }
			require(threw, "a disturbed site was accepted");
		}
	}
}

int main() {
	try {
		test_defaults_write_nothing();
		test_each_table_scales_alone();
		test_scaling_rules();
		test_gold_leaves_bread_alone();
		test_stagger_is_one_operand();
		test_profiles();
		test_refusals();
		test_refuses_a_disturbed_table();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_enemy_stats_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_enemy_stats_regression: ok\n";
	return 0;
}
