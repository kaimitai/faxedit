#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevVerticalScroll: both parts at $fcce and $8000 by
// hash, the thirteen hook sites, the parameter bytes, the flag gate, the
// refusals and the shared ram; then the placements: bank and addr, bank=15
// and sram=true.
namespace {
	constexpr word ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr word SRAM_CODE{ 0x6806 };
	constexpr std::size_t OWN_BANK[]{ 190, 232, 254, 274, 331, 348, 379 };
	constexpr std::size_t GATE_SIZE{ 17 }, RESIDENT{ 391 }, BANKED{ 529 };
	// entry points in the resident part
	constexpr word PUB{ 0 }, PLY{ 171 }, OVL{ 209 }, STUB_GATE{ 225 }, STUB_TILE{ 249 }, STUB_ATTR{ 266 };
	constexpr word MIRR_V{ 292 }, TRAMP_ARM{ 324 }, TRAMP_END{ 343 };
	constexpr std::uint64_t RESIDENT_HASH{ 0x765f063a660996a3 }, BANKED_HASH{ 0x61b879e4c38e7065 };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	struct Site { byte bank; word cpu; std::vector<byte> orig; };
	const std::vector<Site> SITES{
		{ 15, 0xdb8b, { 0xa5, 0x54, 0xc9, 0x02, 0x90, 0x1e } },
		{ 15, 0xd127, { 0x86, 0x54, 0xa5, 0x0d, 0x85, 0x59 } },
		{ 15, 0xd440, { 0xa9, 0x01, 0x85, 0x73 } },
		{ 15, 0xd4a8, { 0xbd, 0xcf, 0xd4 } },
		{ 15, 0xca1f, { 0xa9, 0x00, 0x8d, 0x05, 0x20 } },
		{ 15, 0xdbec, { 0x4c, 0x45, 0xdb } },
		{ 15, 0xec06, { 0xa5, 0xa1, 0x85, 0x28 } },
		{ 15, 0xec9e, { 0x4c, 0x39, 0xf0 } },
		{ 14, 0xb9d1, { 0x4c, 0x39, 0xf0 } },
		{ 14, 0xb875, { 0x4c, 0x39, 0xf0 } },
		{ 15, 0xcbd5, { 0xa9, 0x0e, 0x8d, 0xff, 0x9f, 0x4a, 0x8d, 0xff, 0x9f, 0x4a, 0x8d, 0xff, 0x9f,
		                0x4a, 0x8d, 0xff, 0x9f, 0x4a, 0x8d, 0xff, 0x9f } },
		{ 15, 0xcc40, { 0xa9, 0x0e } },
		{ 15, 0xcc93, { 0xa9, 0x0e } },
	};

	void put(std::vector<byte>& rom, byte bank, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		for (std::size_t i{ 0 }; i < 8; ++i) rom[i] = std::array<byte, 8>{ 'N', 'E', 'S', 0x1a, 16, 0, 0x10, 0 }[i];
		for (const auto& s : SITES) put(rom, s.bank, s.cpu, s.orig);
		return rom;
	}

	// the jump suite's and the scheduler's vanilla sites
	std::vector<byte> vanilla_rom_with_movement_sites() {
		auto rom{ vanilla_rom() };
		put(rom, 15, 0xe3cd, { 0xa5, 0xa5, 0x10, 0x00 });                      // jump: fall entry
		put(rom, 15, 0xe444, { 0xa5, 0xa4, 0x4a, 0xb0, 0x1a });                // jump: init
		put(rom, 15, 0xe463, { 0xa6, 0xa6, 0xe0, 0x10 });                      // jump: arc
		put(rom, 15, 0xc9af, { 0xa9, 0x07, 0x8d, 0x14, 0x40 });                // scheduler hook 1
		put(rom, 15, 0xc9de, { 0x8d, 0x01, 0x20, 0xa5, 0x5a });                // scheduler hook 2
		return rom;
	}

	std::vector<fh::GeneralHack> hacks(const std::string& text) {
		return fh::filter_general_hacks(15, fh::parse_general_hacks(text));
	}
	fe::Config config(const std::vector<byte>& rom, const std::string& region) {
		fe::Config result;
		result.load_definitions(EOE_TEST_CONFIG_PATH, "");
		result.set_region(region);
		result.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		return result;
	}
	std::size_t install(std::vector<byte>& rom, const std::string& spec, word org = ORG) {
		return fh::HackManager{}.install_general_hacks(config(rom, "us"), rom, 15, org, 0xfff0, hacks(spec), nullptr).bank_used;
	}
	std::string hex(const std::vector<byte>& rom, byte bank, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) { s += d[rom[off + i] >> 4]; s += d[rom[off + i] & 0xf]; }
		return s;
	}
	std::string lo_hi(word v) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		s += d[(v & 0xff) >> 4]; s += d[v & 0xf]; s += d[v >> 12]; s += d[(v >> 8) & 0xf];
		return s;
	}
	std::vector<byte> bytes(const std::vector<byte>& rom, byte bank, word cpu, std::size_t n) {
		const auto off{ klib::Asm6502::get_file_offset(bank, cpu) };
		return { rom.begin() + static_cast<std::ptrdiff_t>(off), rom.begin() + static_cast<std::ptrdiff_t>(off + n) };
	}
	// fnv-1a
	std::uint64_t hash(const std::vector<byte>& v) {
		std::uint64_t h{ 0xcbf29ce484222325 };
		for (const byte b : v) { h ^= b; h *= 0x100000001b3; }
		return h;
	}

	void test_default_install() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevVerticalScroll addr=0x8000");
		require(hash(bytes(rom, 15, ORG, RESIDENT)) == RESIDENT_HASH, "resident part");
		require(hash(bytes(rom, 9, 0x8000, BANKED)) == BANKED_HASH, "banked part");
		// the armer hook is pinned with the flag gate, the weapon tail with sram=true
		const auto at = [](word off) { return static_cast<word>(ORG + off); };
		require(hex(rom, 15, 0xdb8b, 6) == "20" + lo_hi(at(STUB_GATE)) + "901fea", "gate hook");
		require(hex(rom, 15, 0xd440, 4) == "4c" + lo_hi(at(STUB_TILE)) + "ea", "tile hook");
		require(hex(rom, 15, 0xd4a8, 3) == "4c" + lo_hi(at(STUB_ATTR)), "attr hook");
		require(hex(rom, 15, 0xca1f, 5) == "20" + lo_hi(at(PUB)) + "eaea", "publish hook");
		require(hex(rom, 15, 0xdbec, 3) == "4c" + lo_hi(at(TRAMP_END)), "end hook");
		require(hex(rom, 15, 0xec06, 4) == "20" + lo_hi(at(PLY)) + "ea", "player hook");
		require(hex(rom, 15, 0xec9e, 3) == "4c" + lo_hi(at(OVL)), "swing tail hook");
		require(hex(rom, 14, 0xb9d1, 3) == "4c" + lo_hi(at(OVL)), "shield tail hook");
		require(hex(rom, 15, 0xcbd5, 21) == "20" + lo_hi(at(MIRR_V)) + "eaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaeaea", "boot burst hook");
		require(hex(rom, 15, 0xcc40, 2) == "a59c" && hex(rom, 15, 0xcc93, 2) == "a59c", "mirroring resets read $9c");
	}

	void test_bias_byte() {
		auto a{ vanilla_rom() }, b{ vanilla_rom() };
		install(a, "AtlasDevVerticalScroll");
		install(b, "AtlasDevVerticalScroll bias=30");
		std::size_t where{ 0 };
		for (std::size_t i{ 0 }; i < a.size(); ++i) if (a[i] != b[i]) where = i;
		require(b[where] == 30 && a[where] == 32, "the bias byte holds the value itself");
	}

	void test_direction_patches() {
		auto a{ vanilla_rom() }, u{ vanilla_rom() }, d{ vanilla_rom() }, f{ vanilla_rom() };
		install(a, "AtlasDevVerticalScroll");
		install(u, "AtlasDevVerticalScroll up=false");
		install(d, "AtlasDevVerticalScroll down=false");
		install(f, "AtlasDevVerticalScroll flip=false");
		auto count = [&](const std::vector<byte>& x) {
			std::size_t n{ 0 };
			for (std::size_t i{ 0 }; i < a.size(); ++i) if (a[i] != x[i]) { require(x[i] == 0xff, "a refused direction is the $ff sentinel"); ++n; }
			return n;
		};
		require(count(u) == 2 && count(d) == 2 && count(f) == 1, "direction switches change exactly their compare bytes");
	}

	void test_flag_gate() {
		auto rom{ vanilla_rom() };
		install(rom, "AtlasDevVerticalScroll flag=20");
		const word tramp{ static_cast<word>(ORG + GATE_SIZE + TRAMP_ARM) };
		require(hex(rom, 15, ORG, 17) == "ad03012910f0034c" + lo_hi(tramp) + "8654a50d855960", "gate bytes");
		require(hex(rom, 15, 0xd127, 6) == "20" + lo_hi(ORG) + "eaeaea", "armer hook targets the gate");
		auto rom247{ vanilla_rom() };
		install(rom247, "AtlasDevVerticalScroll flag=247");
		require(hex(rom247, 15, ORG, 5) == "ad1f012980", "flag 247 is byte $011f mask $80");
	}

	void test_refusals() {
		auto expect_throw = [](const std::string& spec, const std::string& needle, std::vector<byte> rom) {
			try { install(rom, spec); }
			catch (const std::runtime_error& e) {
				require(std::string{ e.what() }.find(needle) != std::string::npos, spec + ": " + e.what());
				return;
			}
			require(false, spec + " did not throw");
		};
		expect_throw("AtlasDevVerticalScroll up=false down=false", "installs nothing", vanilla_rom());
		expect_throw("AtlasDevVerticalScroll flag=248", "0 to 247", vanilla_rom());
		// a site changed past its first byte, and a single used byte, are refused too
		auto dirty14{ vanilla_rom() }; dirty14[klib::Asm6502::get_file_offset(14, 0xb877)] = 0xea;
		expect_throw("AtlasDevVerticalScroll", "not stock", dirty14);
		auto full{ vanilla_rom() }; full[klib::Asm6502::get_file_offset(15, static_cast<word>(ORG + RESIDENT - 1))] = 0x00;
		expect_throw("AtlasDevVerticalScroll", "no free space", full);
		expect_throw("AtlasDevVerticalScroll bank=31", "switchable bank", vanilla_rom());
		expect_throw("AtlasDevVerticalScroll bank=14", "bank 14 is rewritten", vanilla_rom());
		expect_throw("AtlasDevVerticalScroll addr=0xbf00", "do not fit", vanilla_rom());
		expect_throw("AtlasDevVerticalScroll mode=off", "mode must be vanilla", vanilla_rom());
		expect_throw("AtlasDevVerticalScroll mode=vanilla bias=256", "0 to 255", vanilla_rom());
		// mode=vanilla installs nothing, even on a rom it would refuse
		auto hooked{ vanilla_rom() };
		hooked[klib::Asm6502::get_file_offset(15, 0xd440)] = 0xea;
		const auto before{ hooked };
		require(install(hooked, "AtlasDevVerticalScroll mode=vanilla") == 0 && hooked == before, "mode=vanilla installs nothing");
		expect_throw("AtlasDevVerticalScroll bank=15", "bank 15 free space", vanilla_rom());
		auto hud{ vanilla_rom() }; hud[klib::Asm6502::get_file_offset(14, 0x8897)] = 0x20;
		expect_throw("AtlasDevVerticalScroll", "AtlasDevEnemyHud", hud);
	}

	void test_placements() {
		// bank and addr: every stub maps the new bank
		auto a{ vanilla_rom() }, b{ vanilla_rom() };
		install(a, "AtlasDevVerticalScroll addr=0x8000");
		install(b, "AtlasDevVerticalScroll bank=11 addr=0x9000");
		std::size_t own{ 0 };
		for (std::size_t i{ 0 }; i < RESIDENT; ++i) {
			const auto off{ klib::Asm6502::get_file_offset(15, static_cast<word>(ORG + i)) };
			if (a[off] == 9 && b[off] == 11) ++own;
		}
		require(own == std::size(OWN_BANK), "bank=11: seven stubs map bank 11");
		// AtlasDevEnemyHud after it is refused: they share $04e3-$04e9
		auto d{ vanilla_rom() };
		bool refused{ false };
		try { fh::HackManager{}.install_general_hacks(config(d, "us"), d, 15, ORG, 0xfff0,
			hacks("AtlasDevVerticalScroll\nAtlasDevEnemyHud"), nullptr); }
		catch (const std::runtime_error& e) { refused = std::string{ e.what() }.find("AtlasDevEnemyHud: shares ram") != std::string::npos; }
		require(refused, "AtlasDevEnemyHud after the scroll must be refused");

		// bank=15 and sram=true carry the same image, gate, fixed part and
		// banked part back to back. the address bytes are the ones that change
		// when bank=15 is installed $20 higher; in the sram copy every other
		// byte must be the bank=15 byte
		const std::string spec{ "AtlasDevVerticalScroll flag=20" };
		const std::size_t size{ GATE_SIZE + RESIDENT + BANKED };
		auto low{ vanilla_rom() }, high{ vanilla_rom() };
		require(install(low, spec + " bank=15", 0xf800) == size, "bank=15: the whole image in bank 15");
		install(high, spec + " bank=15", 0xf820);
		const auto lo{ klib::Asm6502::get_file_offset(15, 0xf800) }, hi{ lo + 0x20 };
		auto rom{ vanilla_rom() };
		const auto cfg{ config(rom, "us") };
		fh::HackManager manager;
		manager.init_sram_state(cfg);
		manager.install_general_hacks(cfg, rom, 15, ORG, 0xfff0, hacks(spec + " sram=true"), nullptr);
		const auto backing{ klib::Asm6502::get_file_offset(9, 0xa700) };
		for (std::size_t i{ 0 }; i < size; ++i) {
			if (high[hi + i] != low[lo + i]) { ++i; continue; }   // an address
			require(rom[backing + i] == low[lo + i], "sram=true: the sram copy differs outside an address");
		}
		// the boot burst runs before sram is filled, so its mirroring reset stays in bank 15
		require(hex(rom, 15, ORG, 32) == hex(low, 15, static_cast<word>(0xf800 + GATE_SIZE + MIRR_V), 32), "sram=true: boot reset in bank 15");
		require(hex(rom, 15, 0xcbd5, 3) == "20" + lo_hi(ORG), "sram=true: the boot burst calls bank 15");
		require(hex(rom, 14, 0xb875, 3) == "4c" + lo_hi(static_cast<word>(SRAM_CODE + GATE_SIZE + OVL)), "sram=true: overlay");
	}

	void test_post_clients_refused_both_orders() {
		auto expect_throw = [](const std::string& spec, const std::string& needle) {
			auto rom{ vanilla_rom_with_movement_sites() };
			try { install(rom, spec); }
			catch (const std::runtime_error& e) {
				require(std::string{ e.what() }.find(needle) != std::string::npos, spec + ": " + e.what());
				return;
			}
			require(false, spec + " did not throw");
		};
		expect_throw("AtlasDevFrameScheduler\nAtlasDevDayNightCycle\nAtlasDevVerticalScroll", "shares ram $04e2-$04ec");
		expect_throw("AtlasDevVerticalScroll\nAtlasDevFrameScheduler\nAtlasDevDayNightCycle", "AtlasDevDayNightCycle: shares ram");
		expect_throw("AtlasDevVerticalScroll\nAtlasDevFrameScheduler\nAtlasDevInfectedTint", "AtlasDevInfectedTint: shares ram");
		expect_throw("AtlasDevVerticalScroll\nAtlasDevFrameScheduler\nAtlasDevTimeOfDay", "AtlasDevTimeOfDay: shares ram");
	}

	void test_scheduler_core_and_jump_coexist() {
		auto a{ vanilla_rom_with_movement_sites() };
		install(a, "AtlasDevFrameScheduler\nAtlasDevJumpControl\nAtlasDevVerticalScroll");
		require(hex(a, 15, 0xca1f, 1) == "20" && hex(a, 15, 0xe3cd, 1) == "4c", "scheduler, jump and scroll install together");
	}
}

int main() {
	try {
		test_default_install();
		test_bias_byte();
		test_direction_patches();
		test_flag_gate();
		test_refusals();
		test_post_clients_refused_both_orders();
		test_scheduler_core_and_jump_coexist();
		test_placements();
	}
	catch (const std::exception& e) {
		std::cerr << "atlas_vertical_scroll_regression: " << e.what() << '\n';
		return 1;
	}
	std::cout << "atlas_vertical_scroll_regression: ok\n";
	return 0;
}
