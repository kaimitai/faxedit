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

// pins install_AtlasDevSmartKeys to the bytes it is expected to emit. the
// body replaces the five key handlers of the door gate in place, so a
// default install must consume no cursor space, must leave the dispatcher,
// the unlocked entry and the three ring handlers exactly as they were, and
// must refuse a rom whose gate has already been altered.
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };

	// the instructions each install guard checks, and nothing more
	constexpr std::array<byte, 7> DISPATCH{ 0xad, 0x2b, 0x04, 0xf0, 0x0a, 0x0a, 0xa8 };
	constexpr std::array<byte, 18> TABLE{ 0x3d, 0xeb, 0x50, 0xeb, 0x60, 0xeb, 0x70, 0xeb,
		0x80, 0xeb, 0x90, 0xeb, 0xa0, 0xeb, 0xb0, 0xeb, 0xc0, 0xeb };
	constexpr std::array<word, 5> KEYS{ 0xeb51, 0xeb61, 0xeb71, 0xeb81, 0xeb91 };
	constexpr std::array<byte, 5> VANILLA_TAIL{ 0xa9, 0x84, 0x20, 0x59, 0xf8 };
	constexpr std::array<byte, 5> UNLOCK_TAIL{ 0xa9, 0x00, 0x8d, 0x2b, 0x04 };

	// the whole body, 75 bytes at $eb51
	const std::string BODY{
		"984aa8186903cdc103d0034cd1ebaec603e009b027ca3024ddad03d0f8e8ecc603"
		"b00bbdad03ca9dad03e8e8d0f0cec603a984"
		"2059f80c41824ce1ebb996eb2059f80c4182607d7c7b027e" };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		auto put = [&](word cpu, const auto& bytes) {
			const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
		};
		put(0xeb2f, DISPATCH);
		put(0xeb3f, TABLE);
		for (std::size_t i{ 0 }; i < KEYS.size(); ++i)
			put(KEYS[i], std::array<byte, 5>{ 0xad, 0xc1, 0x03, 0xc9,
				static_cast<byte>(0x04 + i) });
		put(0xebd1, VANILLA_TAIL);
		put(0xebe1, UNLOCK_TAIL);
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(15, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0,
			hacks, nullptr);
	}

	std::string hex_at(const std::vector<byte>& rom, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) {
			s += d[rom[off + i] >> 4];
			s += d[rom[off + i] & 0x0f];
		}
		return s;
	}

	void test_vanilla_mode_writes_nothing() {
		auto rom{ vanilla_rom() };
		const auto before{ rom };
		const auto used{ install(rom, "AtlasDevSmartKeys mode=vanilla") };
		require(used == 0, "vanilla mode consumed cursor space");
		require(rom == before, "vanilla mode modified the rom");
	}

	// the body lives in the space the handlers it replaces occupied
	void test_consumes_no_cursor() {
		auto rom{ vanilla_rom() };
		const auto used{ install(rom, "AtlasDevSmartKeys") };
		require(used == 0, "a default install consumed cursor space");
		require(hex_at(rom, ORG, 16) == std::string(32, 'f'),
			"something was written at the general hack cursor");
	}

	void test_body_replaces_the_key_handlers() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevSmartKeys");
		require(hex_at(rom, 0xeb51, 75) == BODY, "body at $eb51");
		// the body stops five bytes short of the first ring handler
		require(hex_at(rom, 0xeb9c, 5) == "ffffffffff", "the body overran its span");
	}

	void test_only_the_key_entries_move() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevSmartKeys");
		// entries 1 to 5 all reach the shared handler; the rts trick stores the
		// target address minus one
		require(hex_at(rom, 0xeb41, 10) == "50eb50eb50eb50eb50eb", "key entries");
		require(hex_at(rom, 0xeb3f, 2) == "3deb", "the unlocked entry moved");
		require(hex_at(rom, 0xeb4b, 6) == "a0ebb0ebc0eb", "a ring entry moved");
		require(hex_at(rom, 0xeb2f, 7) == "ad2b04f00a0aa8", "the dispatcher moved");
	}

	void test_refusals() {
		auto rom{ vanilla_rom() };
		bool threw{ false };
		try { install(rom, "AtlasDevSmartKeys mode=sometimes"); }
		catch (const std::runtime_error&) { threw = true; }
		require(threw, "accepted an unknown mode");
	}

	// a rom whose gate is not vanilla is refused, naming the hack
	void test_refuses_a_disturbed_gate() {
		for (const word cpu : { word{ 0xeb2f }, word{ 0xeb41 }, word{ 0xeb54 },
			word{ 0xeb94 }, word{ 0xebd1 }, word{ 0xebe1 } }) {
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, cpu)] ^= 0x01;
			bool threw{ false };
			try { install(rom, "AtlasDevSmartKeys"); }
			catch (const std::runtime_error& e) {
				threw = true;
				require(std::string{ e.what() }.find("AtlasDevSmartKeys") != std::string::npos,
					"the refusal does not name the hack");
			}
			require(threw, "a disturbed gate was accepted");
		}
	}
}

int main() {
	try {
		test_vanilla_mode_writes_nothing();
		test_consumes_no_cursor();
		test_body_replaces_the_key_handlers();
		test_only_the_key_entries_move();
		test_refusals();
		test_refuses_a_disturbed_gate();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_smart_keys_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_smart_keys_regression: ok\n";
	return 0;
}
