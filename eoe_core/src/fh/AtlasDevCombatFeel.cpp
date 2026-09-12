#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// combat feel: the hero's walk, mercy time, knockback and swing. every knob is
// an operand byte the vanilla rom already holds, so with default parameters
// this hack writes nothing at all and uses no code, ram or general hack space.
//
// the walk is not one speed. $e27f resets the speed in $a9 and $aa to $00c0
// on the first frame of a walk, 0.75 pixels per frame, and $e2b8 adds one of
// the four deltas at $e2c4 every frame, chosen by the title tier, until the
// compare at $e2a5 stops it at $0180, 1.5 pixels per frame. walk and walkmax
// are those two numbers in subpixels per frame, 256 to the pixel, and ramp is
// the four deltas joined with plus signs; vanilla is 2+4+6+8, and 0+0+0+0
// is a flat walk.
//
// iframes is the mercy time after a hit, the lda #$3c that three sites store
// into $ad: $89d5 and $ab49 in bank 14 and $c849 in bank 15. the counter is
// decremented at $e0ec, and the shove lasts while it is above the threshold
// compared at $e0f0, so vanilla shoves for 60 minus 57, three frames. the
// threshold is always written as iframes minus knockbackframes, so raising
// the mercy time never lengthens the shove by accident. knockback is the
// speed $e288 loads for the shove, vanilla $0800, eight pixels per frame.
//
// attack is the three swing phase lengths at $e150, vanilla 8, 3 and 8, of
// which the middle two are the frames the blade can hit. moveattack=1 makes
// the bmi at $e199, which throws away left and right while attacking, fall
// through to the next instruction, the idiom vanilla itself uses at $e3cd.
//
// profile=name sets every knob at once. each profile is a pick of numbers in
// this game's units that plays in the spirit of the game it is named after,
// the way AtlasDevFallControl's zelda2 is a curve shaped like that descent
// and not its bytes; none is a port of another engine's constants. a knob
// given explicitly overrides the profile.
//
// every site is verified against its exact vanilla bytes before anything is
// written, and all of them are identical in the us, us rev a, eu and jp roms.
namespace {
	constexpr word SPEED_MAX{ 0x0800 };      // what the engine trusts for the shove

	struct Site { byte bank; word addr; std::vector<byte> orig; };
	const Site WALK_BASE{ 15, 0xe27f, { 0xa9, 0xc0, 0x85, 0xa9, 0xa9, 0x00, 0x85, 0xaa, 0x60 } };
	const Site WALK_CAP{ 15, 0xe2a3, { 0xa5, 0xa9, 0xc9, 0x80, 0xa5, 0xaa, 0xe9, 0x01, 0xb0 } };
	const Site RAMP{ 15, 0xe2c4, { 0x02, 0x04, 0x06, 0x08 } };
	const Site RAMP_READER{ 15, 0xe2b5, { 0xa5, 0xa9, 0x18, 0x7d, 0xc4, 0xe2, 0x85, 0xa9 } };
	const Site IFRAMES_A{ 14, 0x89d5, { 0xa9, 0x3c, 0x85, 0xad } };
	const Site IFRAMES_B{ 14, 0xab49, { 0xa9, 0x3c, 0x85, 0xad } };
	const Site IFRAMES_C{ 15, 0xc849, { 0xa9, 0x3c, 0x85, 0xad } };
	const Site RELEASE{ 15, 0xe0e8, { 0xa5, 0xad, 0xf0, 0x10, 0xc6, 0xad, 0xa5, 0xad, 0xc9, 0x39, 0xf0, 0x05, 0x90, 0x06 } };
	const Site KNOCKBACK{ 15, 0xe288, { 0xa9, 0x00, 0x85, 0xa9, 0xa9, 0x08, 0x85, 0xaa, 0x60 } };
	const Site ATTACK{ 15, 0xe150, { 0x08, 0x03, 0x08 } };
	const Site ATTACK_READER{ 15, 0xe138, { 0xdd, 0x50, 0xe1 } };
	const Site MOVEATTACK{ 15, 0xe199, { 0x30, 0x23 } };

	[[noreturn]] void fail(const std::string& message) {
		throw std::runtime_error("AtlasDevCombatFeel: " + message);
	}

	struct Profile {
		const char* name;
		word walk, walkmax;
		std::array<byte, 4> ramp;
		byte iframes, shove;
		word knockback;
		std::array<byte, 3> attack;
		byte moveattack;
	};
	constexpr std::array<Profile, 10> PROFILES{ {
		{ "vanilla",        192, 384, { 2, 4, 6, 8 }, 60,  3, 2048, {  8, 3,  8 }, 0 },
		{ "zelda2",         320, 320, { 0, 0, 0, 0 }, 60,  4, 1536, {  6, 3,  5 }, 1 },
		{ "metroid",        256, 384, { 4, 4, 4, 4 }, 90,  6, 2048, {  8, 3,  8 }, 1 },
		{ "megaman",        352, 352, { 0, 0, 0, 0 }, 60,  2, 1024, {  5, 3,  4 }, 1 },
		{ "castlevania",    192, 192, { 0, 0, 0, 0 }, 45,  8, 2048, { 10, 6, 10 }, 0 },
		{ "ninjagaiden",    384, 384, { 0, 0, 0, 0 }, 40, 10, 2048, {  4, 3,  4 }, 1 },
		{ "ghostsngoblins", 160, 160, { 0, 0, 0, 0 }, 75,  6, 1536, {  7, 4,  7 }, 1 },
		{ "kidicarus",      288, 288, { 0, 0, 0, 0 }, 60,  3, 1280, {  5, 3,  5 }, 1 },
		{ "contra",         384, 448, { 8, 8, 8, 8 }, 30,  2, 1024, {  4, 3,  4 }, 1 },
		{ "arcade",         320, 512, { 8, 8, 8, 8 }, 30,  2, 1024, {  6, 3,  6 }, 1 },
	} };

	const Profile& profile(const fh::GeneralHack& hack) {
		const std::string name{ hack.string_or("profile", "vanilla") };
		for (const Profile& p : PROFILES) if (name == p.name) return p;
		fail("profile must be vanilla, zelda2, metroid, megaman, castlevania, ninjagaiden, "
			"ghostsngoblins, kidicarus, contra or arcade");
	}

	void require(const std::vector<byte>& rom, const Site& s) {
		const auto off{ klib::Asm6502::get_file_offset(s.bank, s.addr) };
		for (std::size_t i{ 0 }; i < s.orig.size(); ++i)
			if (off + i >= rom.size() || rom[off + i] != s.orig[i])
				fail(std::format("the vanilla site at ${:04x} is not intact", s.addr));
	}

	void put(std::vector<byte>& rom, const Site& s, std::size_t at, byte value) {
		rom[klib::Asm6502::get_file_offset(s.bank, s.addr) + at] = value;
	}

	// a plus separated list of bytes, exactly N long; commas separate hacks
	template<std::size_t N>
	std::array<byte, N> byte_list(const fh::GeneralHack& hack, const char* name, std::array<byte, N> fallback) {
		if (!hack.has_param(name)) return fallback;
		std::array<byte, N> out{};
		std::string_view rest{ hack.get_string(name) };
		for (std::size_t i{ 0 }; i < N; ++i) {
			const auto comma{ rest.find('+') };
			const std::string_view tok{ rest.substr(0, comma) };
			unsigned v{};
			const auto r{ std::from_chars(tok.data(), tok.data() + tok.size(), v) };
			if (r.ec != std::errc{} || r.ptr != tok.data() + tok.size() || v > 255 || (i + 1 < N) != (comma != std::string_view::npos))
				fail(std::format("{} is {} values 0 to 255 joined with plus signs", name, N));
			out[i] = static_cast<byte>(v);
			rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
		}
		return out;
	}
}

word fh::HackManager::install_AtlasDevCombatFeel(const fe::Config&, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack) const {
	const Profile& base{ profile(hack) };
	const word walk{ hack.word_or("walk", base.walk) }, walkmax{ hack.word_or("walkmax", base.walkmax) };
	const word knockback{ hack.word_or("knockback", base.knockback) };
	const word iframes{ hack.word_or("iframes", base.iframes) };
	const word shove{ hack.word_or("knockbackframes", base.shove) };
	const byte moveattack{ hack.byte_or("moveattack", base.moveattack) };
	const auto ramp{ byte_list<4>(hack, "ramp", base.ramp) };
	const auto attack{ byte_list<3>(hack, "attack", base.attack) };

	if (walk < 1 || walk > walkmax || walkmax > SPEED_MAX)
		fail(std::format("walk must be 1 to walkmax and walkmax at most {}, subpixels per frame", SPEED_MAX));
	if (iframes < 1 || iframes > 255) fail("iframes must be 1 to 255 frames");
	if (shove > iframes) fail("knockbackframes must be 0 to iframes");
	if (knockback < 1 || knockback > SPEED_MAX)
		fail(std::format("knockback must be 1 to {} subpixels per frame", SPEED_MAX));
	for (byte p : attack) if (p == 0) fail("attack phases must be 1 to 255 frames");
	if (moveattack > 1) fail("moveattack must be 0 or 1");

	// every check is complete before any mutation
	for (const Site* s : { &WALK_BASE, &WALK_CAP, &RAMP, &RAMP_READER, &IFRAMES_A, &IFRAMES_B,
		&IFRAMES_C, &RELEASE, &KNOCKBACK, &ATTACK, &ATTACK_READER, &MOVEATTACK })
		require(rom, *s);

	put(rom, WALK_BASE, 1, static_cast<byte>(walk & 0xff));
	put(rom, WALK_BASE, 5, static_cast<byte>(walk >> 8));
	put(rom, WALK_CAP, 3, static_cast<byte>(walkmax & 0xff));
	put(rom, WALK_CAP, 7, static_cast<byte>(walkmax >> 8));
	for (std::size_t i{ 0 }; i < 4; ++i) put(rom, RAMP, i, ramp[i]);
	for (const Site* s : { &IFRAMES_A, &IFRAMES_B, &IFRAMES_C }) put(rom, *s, 1, static_cast<byte>(iframes));
	put(rom, RELEASE, 9, static_cast<byte>(iframes - shove));
	put(rom, KNOCKBACK, 1, static_cast<byte>(knockback & 0xff));
	put(rom, KNOCKBACK, 5, static_cast<byte>(knockback >> 8));
	for (std::size_t i{ 0 }; i < 3; ++i) put(rom, ATTACK, i, attack[i]);
	if (moveattack) put(rom, MOVEATTACK, 1, 0x00);
	return cpu_addr;
}
