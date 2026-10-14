#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fe/ROM_Manager.h"
#include "fh/AtlasDevFaxOptions.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// the options menu: rows from the listed hacks, the refusals, the resident
// part and the menu placement
namespace {
	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	template<typename F>
	void throws(const std::string& reason, F call) {
		bool threw{ false };
		try { call(); }
		catch (const std::runtime_error& e) {
			threw = true;
			require(std::string{ e.what() }.find(reason) != std::string::npos, reason + ": " + e.what());
		}
		require(threw, "must refuse: " + reason);
	}

	void test_rows() {
		using fh::GeneralHackLib;
		require(fh::faxoptions_label(GeneralHackLib::AtlasDevKingGrieveControl) == "KING GRIEVE CONTROL", "control kept when it fits");
		require(fh::faxoptions_label(GeneralHackLib::AtlasDevExecutionHoodControl) == "EXECUTION HOOD", "control dropped");

		throws("share flag 4", [] { fh::faxoptions_rows(fh::parse_general_hacks(
			"AtlasDevLandingTuck flag=4\nAtlasDevFastBlink flag=4")); });
		throws("flag must be 0 to 247", [] { fh::faxoptions_rows(fh::parse_general_hacks("AtlasDevLandingTuck flag=248")); });
	}

	constexpr word PAUSE_SITE{ 0xe039 }, SPAWN_SITE{ 0xdb0a }, START_SITE{ 0xdb26 }, TITLE_SITE{ 0xda71 };

	constexpr word FAR_CALL{ 0xf859 };

	void put(std::vector<byte>& rom, byte bank, word cpu, std::initializer_list<byte> b) {
		auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (byte v : b) rom[off++] = v;
	}

	// the far call's stores of a, x and y from zp, and its reload before the jump
	void put_far_call(std::vector<byte>& rom, word cpu, byte zp) {
		const byte x{ static_cast<byte>(zp + 1) }, y{ static_cast<byte>(zp + 2) };
		put(rom, 15, cpu, { 0x85, zp, 0x86, x, 0x84, y, 0x68, 0x85, 0xec });
		put(rom, 15, static_cast<word>(cpu + 0x3e), { 0xa5, zp, 0xa6, x, 0xa4, y, 0x60 });
	}

	// the hook sites and the game code the hack calls, vanilla. the far call
	// keeps a, x and y in $de-$e0 on the US ROM, in $e2-$e4 on US rev A and EU
	std::vector<byte> blank_rom(byte far_call_zp = 0xde) {
		std::vector<byte> rom(0x40010, 0xff);
		const byte header[]{ 'N', 'E', 'S', 0x1a, 16, 0, 0x12, 0 };   // battery bit set
		for (std::size_t i{ 0 }; i < sizeof header; ++i) rom[i] = header[i];
		put(rom, 15, PAUSE_SITE, { 0x20, 0xa8, 0xcb });          // jsr Sprites_FlipRanges
		put(rom, 15, SPAWN_SITE, { 0xa9, 0x50, 0x8d, 0x31, 0x04 }); // lda #$50 / sta $0431
		put(rom, 15, START_SITE, { 0x20, 0xaf, 0xc6 });          // jsr $c6af
		put(rom, 15, TITLE_SITE, { 0x8d, 0x35, 0x04 });          // sta $0435
		put_far_call(rom, FAR_CALL, far_call_zp);
		put(rom, 15, 0xcba8, { 0xa2, 0x04, 0xa0, 0x84 });                         // Sprites_FlipRanges
		put(rom, 15, 0xc6af, { 0xa9, 0xff, 0x8d, 0x28, 0x04 });                   // Game_ClearTimedItems
		put(rom, 15, 0xcc1a, { 0x8e, 0x00, 0x01, 0xa9, 0x01 });                   // the bank switch
		put(rom, 15, 0xca25, { 0xa9, 0x00, 0x85, 0x10, 0xa5, 0x10, 0xf0, 0xfc, 0x60 }); // WaitForNextFrame
		put(rom, 15, 0xcff4, { 0xa5, 0x20, 0xc5, 0x1f, 0xd0, 0xfa, 0x60 });       // PPUBuffer_WaitEmpty
		put(rom, 15, 0xcfdc, { 0x48, 0x20, 0xca, 0xcf, 0x68, 0xa6, 0x20 });       // PPUQueueAppendHeader
		put(rom, 15, 0xf804, { 0xa5, 0x0d, 0x29, 0x01, 0x09, 0x08, 0x85, 0xe9 }); // PPUAddressFromPos
		put(rom, 15, 0xf845, { 0x9d, 0x00, 0x05, 0xe8, 0x60 });                   // PPUQueuePayload
		put(rom, 12, 0x8ef1, { 0xad, 0x08, 0x02, 0x85, 0xea, 0xad, 0x09, 0x02, 0x85, 0xeb }); // OpenWindowDraw
		put(rom, 12, 0x9002, { 0xa9, 0x01, 0x8d, 0x0c, 0x02 });                   // WindowClose
		return rom;
	}

	fe::Config config(const std::vector<byte>& rom, const std::string& region) {
		fe::Config result;
		result.load_definitions(EOE_TEST_CONFIG_PATH, "");
		result.set_region(region);
		result.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		return result;
	}

	void install(std::vector<byte>& rom, const std::string& spec, bool sram, const std::string& region = "us") {
		const auto cfg{ config(rom, region) };
		fh::HackManager m;
		if (sram) m.init_sram_state(cfg);
		const auto all{ fh::parse_general_hacks(spec) };
		m.set_project_hacks(all);
		m.install_general_hacks(cfg, rom, 15, 0xfcce, 0xffe0, fh::filter_general_hacks(15, all), nullptr);
	}

	std::size_t offset(byte bank, word cpu) { return klib::Asm6502::get_file_offset(bank, cpu); }
	byte at(const std::vector<byte>& rom, word cpu) {
		return rom[offset(15, cpu)];
	}
	word word_at(const std::vector<byte>& rom, word cpu) {
		return static_cast<word>(at(rom, cpu) | (at(rom, static_cast<word>(cpu + 1)) << 8));
	}
	bool bytes_at(const std::vector<byte>& rom, word cpu, std::initializer_list<byte> b) {
		for (byte v : b)
			if (at(rom, cpu++) != v) return false;
		return true;
	}

	// fnv-1a of p_size rom bytes from p_off
	std::uint64_t digest(const std::vector<byte>& rom, std::size_t p_off, std::size_t p_size) {
		std::uint64_t result{ 14695981039346656037ULL };
		for (std::size_t i{ 0 }; i < p_size; ++i) {
			result ^= rom[p_off + i];
			result *= 1099511628211ULL;
		}
		return result;
	}

	// the pause hook's jmp to the menu: jsr Sprites_FlipRanges / lda / and / beq / jmp
	word menu_entry(const std::vector<byte>& rom) {
		const word pause{ word_at(rom, PAUSE_SITE + 1) };
		return word_at(rom, static_cast<word>(pause + 10));
	}

	void test_refusals() {
		const std::string tuck{ "AtlasDevLandingTuck flag=4 mode=vanilla\n" };
		for (const auto& [spec, reason] : std::initializer_list<std::pair<std::string, std::string>>{
				{ tuck + "AtlasDevFaxOptions\nKillSwitch", "KillSwitch" },
				{ "AtlasDevFastBlink mode=vanilla\nAtlasDevFaxOptions", "no listed AtlasDev hack has a flag" } }) {
			auto rom{ blank_rom() };
			throws(reason, [&] { install(rom, spec, false); });
		}
		// FaxOptions has no flag of its own
		throws("Unknown parameter 'flag'", [] { fh::parse_general_hacks("AtlasDevFaxOptions flag=9"); });
		// the sites each build hooks: the pause screen and the title screen without
		// a save, the spawn and the start with one
		for (const auto& [sites, save] : std::initializer_list<std::pair<std::vector<word>, bool>>{
				{ { PAUSE_SITE, TITLE_SITE }, false }, { { SPAWN_SITE, START_SITE }, true } })
			for (const word site : sites) {
				auto dirty{ blank_rom() };
				dirty[offset(15, site)] = 0x4c;
				throws("is not intact", [&] { install(dirty, tuck + "AtlasDevFaxOptions", save); });
			}
	}

	void test_without_a_save() {
		auto rom{ blank_rom() };
		install(rom, "AtlasDevLandingTuck flag=4 mode=vanilla\nAtlasDevFaxOptions", false);
		require(bytes_at(rom, word_at(rom, TITLE_SITE + 1), { 0x8d, 0x35, 0x04 }), "title: the stock store first");
	}

	void test_with_a_save() {
		// ten rows, so the row bits fill a second byte, and one from a bank 14 hack
		auto rom{ blank_rom() };
		install(rom, "AtlasDevLandingTuck flag=4 mode=vanilla\nAtlasDevHornetControl flag=13 mode=vanilla\n"
			"AtlasDevFastBlink flag=247 mode=vanilla\nAtlasDevScreenTransition flag=20 mode=vanilla\n"
			"AtlasDevRipasheikuControl flag=31 mode=vanilla\nAtlasDevNagaControl flag=40 mode=vanilla\n"
			"AtlasDevPakukameControl flag=55 mode=vanilla\nAtlasDevSugataControl flag=66 mode=vanilla\n"
			"AtlasDevGiantBeesControl flag=77 mode=vanilla\nAtlasDevZorugeriruControl flag=100 mode=vanilla\n"
			"AtlasDevFaxOptions", true);
		require(at(rom, SPAWN_SITE + 3) == 0xea && at(rom, SPAWN_SITE + 4) == 0xea, "spawn hook padded with nops");
		// the resident part, from the start hook to the menu's trampoline, pinned
		// for this configuration
		const word begin{ word_at(rom, START_SITE + 1) };
		const std::size_t size{ static_cast<std::size_t>(menu_entry(rom) - begin) };
		require(size == 224 && digest(rom, offset(15, begin), size) == 8112320362195629462ULL,
			"the resident part changed");
	}

	void test_menu() {
		const std::string rows{ "AtlasDevLandingTuck flag=4 mode=vanilla\nAtlasDevFastBlink flag=5 mode=vanilla\n" };

		// bank 9 by default, at its trailing free space; the whole menu image,
		// pinned for this configuration
		auto rom{ blank_rom() };
		const word free{ fe::ROM_Manager::find_trailing_free_cpu_addr(rom, 9, 0xff, 16) };
		install(rom, rows + "AtlasDevFaxOptions", false);
		constexpr std::size_t MENU_SIZE{ 914 };
		const auto off{ offset(9, free) };
		require(rom[off + MENU_SIZE - 1] != 0xff && rom[off + MENU_SIZE] == 0xff
			&& digest(rom, off, MENU_SIZE) == 6629881637798381992ULL, "the menu bytes changed");

		// bank= moves the menu, and the trampoline switches to that bank
		auto banked{ blank_rom() };
		install(banked, rows + "AtlasDevFaxOptions bank=11", false);
		require(bytes_at(banked, static_cast<word>(menu_entry(banked) + 6), { 0xa2, 0x0b }), "bank=11 in the trampoline");

		// sram=true: the resident part and the menu go to sram together
		auto in_sram{ blank_rom() };
		install(in_sram, rows + "AtlasDevFaxOptions sram=true", true);
	}

	// with the far call of US rev A and EU the menu keeps its cursor, row and
	// top in $de-$e0; a far call of neither kind, the JP ROM and a ROM with the
	// flags in SRAM are refused
	void test_regions() {
		const std::string spec{ "AtlasDevLandingTuck flag=4 mode=vanilla\nAtlasDevFaxOptions" };
		auto rom{ blank_rom(0xe2) };
		const auto off{ offset(9, fe::ROM_Manager::find_trailing_free_cpu_addr(rom, 9, 0xff, 16)) };
		install(rom, spec, false);
		const auto has = [&](std::initializer_list<byte> b) {
			const auto begin{ rom.begin() + off }, end{ begin + 0x400 };
			return std::search(begin, end, b.begin(), b.end()) != end;
		};
		// the opening's cursor and top, then render_all's top, row and slot
		require(has({ 0x20, 0x25, 0xca, 0xa9, 0x00, 0x85, 0xde, 0x85, 0xe0 })
			&& has({ 0xa5, 0xe0, 0x85, 0xdf, 0xa9, 0x0a, 0x85, 0xe7 }), "rev A and EU: the menu's state in $de-$e0");

		// AtlasDevFastBlink's jmp on PPUBuffer_WaitEmpty is accepted
		auto blink{ blank_rom() };
		blink[offset(15, 0xcff4)] = 0x4c;
		install(blink, spec, false);

		auto other{ blank_rom() };
		put(other, 15, FAR_CALL, { 0x98, 0x48, 0x8a, 0x48, 0xad, 0x00, 0x01, 0x48, 0xa2 });
		throws("$f859 differs from the US, US rev A and EU", [&] { install(other, spec, false); });

		// the JP ROM: its far call, at $f921, keeps the US three, and the code
		// the menu calls is elsewhere, first at $f804
		auto jp{ blank_rom() };
		put_far_call(jp, 0xf921, 0xde);
		put(jp, 15, FAR_CALL, { 0x98, 0x48, 0x8a, 0x48, 0xad, 0x00, 0x01, 0x48, 0xa2 });
		put(jp, 15, 0xf804, { 0x53, 0xa1, 0xff, 0xff, 0xff, 0x9d, 0xa8, 0x79 });
		throws("$f804 differs from the US, US rev A and EU", [&] { install(jp, spec, false, "jp"); });

		// the English translation keeps the flags in SRAM
		auto translated{ blank_rom() };
		throws("keeps the flags in SRAM", [&] { install(translated, spec, false, "en-transl"); });
	}
}

int main() {
	try {
		test_rows();
		test_refusals();
		test_without_a_save();
		test_with_a_save();
		test_menu();
		test_regions();
		std::cout << "atlas_faxoptions_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_faxoptions_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
