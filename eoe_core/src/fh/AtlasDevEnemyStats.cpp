#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <array>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// enemy stats: scale the enemy tables at build time.
//
// the whole enemy roster is a block of per type byte tables in bank 14, each
// indexed by the entity id in $02cc,x: hit points at $b5a9, contact damage at
// $b6d7, experience per kill at $b60e and the coin drop values at $aced,
// where indices $00 to $2f are coins and $30 to $3f are bread. the stagger an
// enemy takes when hit is one operand byte, lda #$08 at $8867.
//
// hp, damage, xp and gold are percents of vanilla applied to every entry of
// their table: a zero entry stays zero, since those are the ids that are not
// monsters, everything else rounds to nearest and saturates at 255, and a
// scaled monster never drops below one. bread is left alone. stagger is the
// stun length in frames, which is also the hit flash length, since the same
// counter drives both. 100 writes nothing; no code, no ram, no general hack
// space is used.
//
// profile=name sets all five knobs at once: easy, normal, hard, nightmare or
// grind, and any knob given explicitly overrides the profile.
//
// every table and the stagger site are verified against their vanilla bytes
// before anything is written, and they are identical in the us, us rev a, eu
// and jp roms.
namespace {
	constexpr byte BANK{ 14 };
	constexpr word PERCENT_MAX{ 400 };
	constexpr byte VANILLA_STAGGER{ 8 };
	constexpr word STAGGER_SITE{ 0x8867 };
	constexpr std::array<byte, 5> STAGGER_ORIG{ 0xa9, 0x08, 0x9d, 0x4c, 0x03 };

	struct Table {
		const char* name;
		word addr;
		std::size_t length;
		std::array<byte, 8> head;   // the first bytes the table must hold
	};
	constexpr std::array<Table, 4> TABLES{ {
		{ "hp",     0xb5a9, 101, { 0x00, 0x00, 0x00, 0x00, 0x12, 0x18, 0x0e, 0x03 } },
		{ "damage", 0xb6d7, 100, { 0x00, 0x00, 0x00, 0x1f, 0x07, 0x09, 0x06, 0x03 } },
		{ "xp",     0xb60e, 100, { 0x00, 0x00, 0x00, 0x00, 0x23, 0x37, 0x19, 0x19 } },
		{ "gold",   0xaced,  48, { 0x0a, 0x0f, 0x12, 0x14, 0x16, 0x1a, 0x20, 0x35 } },
	} };

	[[noreturn]] void fail(const std::string& message) {
		throw std::runtime_error("AtlasDevEnemyStats: " + message);
	}

	struct Profile { const char* name; word hp, damage, xp, gold, stagger; };
	constexpr std::array<Profile, 5> PROFILES{ {
		{ "normal",    100, 100, 100, 100,  8 },
		{ "easy",       75,  75, 150, 150, 12 },
		{ "hard",      150, 150, 100, 100,  6 },
		{ "nightmare", 200, 200,  75,  75,  4 },
		{ "grind",     100, 100, 200, 200,  8 },
	} };

	const Profile& profile(const fh::GeneralHack& hack) {
		const std::string name{ hack.string_or("profile", "normal") };
		for (const Profile& p : PROFILES) if (name == p.name) return p;
		fail("profile must be normal, easy, hard, nightmare or grind");
	}

	word percent(const fh::GeneralHack& hack, const char* name, word fallback) {
		const word value{ hack.word_or(name, fallback) };
		if (value < 1 || value > PERCENT_MAX)
			fail(std::format("{} must be 1 to {} percent", name, PERCENT_MAX));
		return value;
	}

	byte scale(byte value, word percent) {
		if (value == 0) return 0;
		const unsigned scaled{ (static_cast<unsigned>(value) * percent + 50) / 100 };
		return static_cast<byte>(scaled < 1 ? 1 : scaled > 255 ? 255 : scaled);
	}

	void require(const std::vector<byte>& rom, word addr, const byte* orig, std::size_t size,
		const char* what) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, addr) };
		for (std::size_t i{ 0 }; i < size; ++i)
			if (off + i >= rom.size() || rom[off + i] != orig[i])
				fail(std::format("the {} at ${:04x} is not vanilla", what, addr));
	}
}

word fh::HackManager::install_AtlasDevEnemyStats(const fe::Config&, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack) const {
	const Profile& base{ profile(hack) };
	const std::array<word, 4> percents{
		percent(hack, "hp", base.hp), percent(hack, "damage", base.damage),
		percent(hack, "xp", base.xp), percent(hack, "gold", base.gold) };
	const word stagger{ hack.word_or("stagger", base.stagger) };
	if (stagger < 1 || stagger > 255)
		fail("stagger must be 1 to 255 frames");

	// every check is complete before any mutation
	for (const Table& t : TABLES)
		require(rom, t.addr, t.head.data(), t.head.size(), (std::string{ t.name } + " table").c_str());
	require(rom, STAGGER_SITE, STAGGER_ORIG.data(), STAGGER_ORIG.size(), "stagger site");

	for (std::size_t i{ 0 }; i < TABLES.size(); ++i) {
		if (percents[i] == 100) continue;
		const auto off{ klib::Asm6502::get_file_offset(BANK, TABLES[i].addr) };
		for (std::size_t j{ 0 }; j < TABLES[i].length; ++j)
			rom[off + j] = scale(rom[off + j], percents[i]);
	}
	if (stagger != VANILLA_STAGGER)
		rom[klib::Asm6502::get_file_offset(BANK, STAGGER_SITE) + 1] = static_cast<byte>(stagger);
	return cpu_addr;
}
