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

// pins install_AtlasDevCombatFeel to the bytes it writes. every knob is an
// operand byte, so a default install writes nothing and consumes no cursor
// space, and each parameter touches exactly the bytes it names. the mercy
// time and the shove are coupled through one threshold, and that coupling is
// what the iframes tests are about.
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	struct Site { byte bank; word addr; std::vector<byte> orig; };
	const std::vector<Site> SITES{
		{ 15, 0xe27f, { 0xa9, 0xc0, 0x85, 0xa9, 0xa9, 0x00, 0x85, 0xaa, 0x60 } },
		{ 15, 0xe2a3, { 0xa5, 0xa9, 0xc9, 0x80, 0xa5, 0xaa, 0xe9, 0x01, 0xb0 } },
		{ 15, 0xe2c4, { 0x02, 0x04, 0x06, 0x08 } },
		{ 15, 0xe2b5, { 0xa5, 0xa9, 0x18, 0x7d, 0xc4, 0xe2, 0x85, 0xa9 } },
		{ 14, 0x89d5, { 0xa9, 0x3c, 0x85, 0xad } },
		{ 14, 0xab49, { 0xa9, 0x3c, 0x85, 0xad } },
		{ 15, 0xc849, { 0xa9, 0x3c, 0x85, 0xad } },
		{ 15, 0xe0e8, { 0xa5, 0xad, 0xf0, 0x10, 0xc6, 0xad, 0xa5, 0xad, 0xc9, 0x39, 0xf0, 0x05, 0x90, 0x06 } },
		{ 15, 0xe288, { 0xa9, 0x00, 0x85, 0xa9, 0xa9, 0x08, 0x85, 0xaa, 0x60 } },
		{ 15, 0xe150, { 0x08, 0x03, 0x08 } },
		{ 15, 0xe138, { 0xdd, 0x50, 0xe1 } },
		{ 15, 0xe199, { 0x30, 0x23 } },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }
	std::size_t off(byte bank, word cpu) { return klib::Asm6502::get_file_offset(bank, cpu); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		for (const Site& s : SITES)
			for (std::size_t i{ 0 }; i < s.orig.size(); ++i) rom[off(s.bank, s.addr) + i] = s.orig[i];
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks, nullptr);
	}

	std::vector<std::size_t> changed(const std::vector<byte>& a, const std::vector<byte>& b) {
		std::vector<std::size_t> out;
		for (std::size_t i{ 0 }; i < a.size(); ++i) if (a[i] != b[i]) out.push_back(i);
		return out;
	}

	void test_defaults_write_nothing() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		require(install(rom, "AtlasDevCombatFeel") == 0, "default install consumed cursor space");
		require(rom == before, "default install modified the rom");
	}

	void test_walk_is_four_operands() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevCombatFeel walk=320 walkmax=512");
		require(changed(before, rom).size() == 4, "walk and walkmax touched more than four bytes");
		require(rom[off(15, 0xe280)] == 0x40 && rom[off(15, 0xe284)] == 0x01, "walk base");
		require(rom[off(15, 0xe2a6)] == 0x00 && rom[off(15, 0xe2aa)] == 0x02, "walk cap");
	}

	// raising the mercy time moves the release threshold with it, so the
	// shove stays three frames; knockbackframes moves the threshold alone
	void test_iframes_keep_the_shove() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevCombatFeel iframes=120");
		require(changed(before, rom).size() == 4, "iframes touched more than four bytes");
		for (const word cpu : { word{ 0x89d6 }, word{ 0xab4a } }) require(rom[off(14, cpu)] == 120, "bank 14 setter");
		require(rom[off(15, 0xc84a)] == 120, "bank 15 setter");
		require(rom[off(15, 0xe0f1)] == 117, "threshold must be iframes minus three");
		auto rom2{ vanilla_rom() };
		install(rom2, "AtlasDevCombatFeel knockbackframes=0");
		require(changed(before, rom2) == std::vector<std::size_t>{ off(15, 0xe0f1) } && rom2[off(15, 0xe0f1)] == 60,
			"knockbackframes=0 must write only the threshold, as iframes");
		auto rom3{ vanilla_rom() };
		install(rom3, "AtlasDevCombatFeel iframes=30 knockbackframes=10");
		require(rom3[off(15, 0xe0f1)] == 20, "threshold is iframes minus knockbackframes");
	}

	void test_lists_and_single_bytes() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevCombatFeel ramp=0+0+0+0");
		require(changed(before, rom).size() == 4, "ramp");
		for (std::size_t i{ 0 }; i < 4; ++i) require(rom[off(15, 0xe2c4) + i] == 0, "ramp entry");
		auto rom2{ vanilla_rom() };
		install(rom2, "AtlasDevCombatFeel attack=6+3+6 knockback=1536 moveattack=1");
		require(rom2[off(15, 0xe150)] == 6 && rom2[off(15, 0xe151)] == 3 && rom2[off(15, 0xe152)] == 6, "attack phases");
		require(rom2[off(15, 0xe289)] == 0x00 && rom2[off(15, 0xe28d)] == 0x06, "knockback");
		require(rom2[off(15, 0xe19a)] == 0x00 && rom2[off(15, 0xe199)] == 0x30, "moveattack neutralizes the operand only");
		require(changed(before, rom2).size() == 4, "attack, knockback and moveattack touched more than four bytes");
	}

	// every profile pins the operands it writes; vanilla writes nothing; an
	// explicit knob overrides the profile's value for that knob only
	void test_profiles() {
		struct P { const char* name; word walk, walkmax; std::array<byte, 4> ramp; byte iframes, shove; word knockback; std::array<byte, 3> attack; byte moveattack; };
		const std::vector<P> profiles{
			{ "zelda2",         320, 320, { 0, 0, 0, 0 }, 60,  4, 1536, {  6, 3,  5 }, 1 },
			{ "metroid",        256, 384, { 4, 4, 4, 4 }, 90,  6, 2048, {  8, 3,  8 }, 1 },
			{ "megaman",        352, 352, { 0, 0, 0, 0 }, 60,  2, 1024, {  5, 3,  4 }, 1 },
			{ "castlevania",    192, 192, { 0, 0, 0, 0 }, 45,  8, 2048, { 10, 6, 10 }, 0 },
			{ "ninjagaiden",    384, 384, { 0, 0, 0, 0 }, 40, 10, 2048, {  4, 3,  4 }, 1 },
			{ "ghostsngoblins", 160, 160, { 0, 0, 0, 0 }, 75,  6, 1536, {  7, 4,  7 }, 1 },
			{ "kidicarus",      288, 288, { 0, 0, 0, 0 }, 60,  3, 1280, {  5, 3,  5 }, 1 },
			{ "contra",         384, 448, { 8, 8, 8, 8 }, 30,  2, 1024, {  4, 3,  4 }, 1 },
			{ "arcade",         320, 512, { 8, 8, 8, 8 }, 30,  2, 1024, {  6, 3,  6 }, 1 },
		};
		for (const P& p : profiles) {
			auto rom{ vanilla_rom() };
			require(install(rom, std::string{ "AtlasDevCombatFeel profile=" } + p.name) == 0, "profile used cursor");
			const std::string tag{ std::string{ "profile " } + p.name };
			require(rom[off(15, 0xe280)] == (p.walk & 0xff) && rom[off(15, 0xe284)] == (p.walk >> 8), tag + " walk");
			require(rom[off(15, 0xe2a6)] == (p.walkmax & 0xff) && rom[off(15, 0xe2aa)] == (p.walkmax >> 8), tag + " walkmax");
			for (std::size_t i{ 0 }; i < 4; ++i) require(rom[off(15, 0xe2c4) + i] == p.ramp[i], tag + " ramp");
			for (const word cpu : { word{ 0x89d6 }, word{ 0xab4a } }) require(rom[off(14, cpu)] == p.iframes, tag + " iframes");
			require(rom[off(15, 0xc84a)] == p.iframes, tag + " iframes");
			require(rom[off(15, 0xe0f1)] == static_cast<byte>(p.iframes - p.shove), tag + " threshold");
			require(rom[off(15, 0xe289)] == (p.knockback & 0xff) && rom[off(15, 0xe28d)] == (p.knockback >> 8), tag + " knockback");
			for (std::size_t i{ 0 }; i < 3; ++i) require(rom[off(15, 0xe150) + i] == p.attack[i], tag + " attack");
			require(rom[off(15, 0xe19a)] == (p.moveattack ? 0x00 : 0x23), tag + " moveattack");
		}
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		install(rom, "AtlasDevCombatFeel profile=vanilla");
		require(rom == before, "profile vanilla wrote bytes");
		auto over{ vanilla_rom() };
		install(over, "AtlasDevCombatFeel profile=megaman iframes=99");
		require(over[off(15, 0xc84a)] == 99 && over[off(15, 0xe0f1)] == 97, "explicit iframes did not override the profile");
		require(over[off(15, 0xe280)] == (352 & 0xff), "the profile's walk was lost under an override");
	}

	void test_refusals() {
		for (const auto& spec : { "AtlasDevCombatFeel walk=0", "AtlasDevCombatFeel profile=doom", "AtlasDevCombatFeel walk=400 walkmax=300",
			"AtlasDevCombatFeel walkmax=2049", "AtlasDevCombatFeel iframes=0", "AtlasDevCombatFeel iframes=256",
			"AtlasDevCombatFeel knockbackframes=61", "AtlasDevCombatFeel knockback=0", "AtlasDevCombatFeel attack=0+3+8",
			"AtlasDevCombatFeel attack=8+3", "AtlasDevCombatFeel ramp=1+2+3", "AtlasDevCombatFeel ramp=1+2+3+4+5",
			"AtlasDevCombatFeel ramp=1+2+3+256", "AtlasDevCombatFeel moveattack=2" }) {
			auto rom{ vanilla_rom() };
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, spec); }
			catch (const std::runtime_error& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevCombatFeel") != std::string::npos, "refusal does not name the hack");
			}
			require(threw, std::string{ "accepted " } + spec);
			require(rom == before, std::string{ "a refused install wrote bytes: " } + spec);
		}
	}

	void test_refuses_a_disturbed_site() {
		for (const Site& s : SITES) {
			auto rom{ vanilla_rom() };
			rom[off(s.bank, s.addr)] ^= 0x01;
			bool threw{ false };
			try { install(rom, "AtlasDevCombatFeel iframes=90"); }
			catch (const std::runtime_error&) { threw = true; }
			require(threw, "a disturbed site was accepted");
		}
	}
}

int main() {
	try {
		test_defaults_write_nothing();
		test_walk_is_four_operands();
		test_iframes_keep_the_shove();
		test_lists_and_single_bytes();
		test_profiles();
		test_refusals();
		test_refuses_a_disturbed_site();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_combat_feel_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_combat_feel_regression: ok\n";
	return 0;
}
