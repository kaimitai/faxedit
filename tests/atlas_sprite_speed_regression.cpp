#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"
#include "fh/AtlasDevFrameScheduler.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
	using Asm = klib::Asm6502;
	void require(bool ok, const std::string& message) {
		if (!ok) throw std::runtime_error(message);
	}
	std::vector<byte> bytes(const std::string& hex) {
		std::vector<byte> result;
		for (std::size_t i{ 0 }; i < hex.size(); i += 2)
			result.push_back(static_cast<byte>(std::stoul(hex.substr(i, 2), nullptr, 16)));
		return result;
	}
	void put(std::vector<byte>& rom, word addr, const std::string& hex) {
		const auto code{ bytes(hex) };
		std::copy(code.begin(), code.end(), rom.begin() + Asm::get_file_offset(15, addr));
	}
	void expect(const std::vector<byte>& rom, word addr, const std::vector<byte>& code) {
		require(std::equal(code.begin(), code.end(), rom.begin() + Asm::get_file_offset(15, addr)),
			"emitted bytes differ at " + std::to_string(addr));
	}
	std::vector<byte> fixture() {
		// instruction-only fixture; no cartridge or graphics are needed.
		std::vector<byte> rom(0x40010, 0xff);
		std::fill_n(rom.begin(), 16, 0);
		const auto header{ bytes("4e45531a10001100") };
		std::copy(header.begin(), header.end(), rom.begin());
		put(rom, 0xcb47, "a900855a855bf004a9ff855aa0008433843484358438843784398425a55a301be625a8b996cb8d0007a97f8d0107a9238d0207b998cb8d0307a004a9f0990007c8c8c8c8d0f7a51c29804980851c60");
		for (const word addr : { 0xf0f8, 0xf1ac })
			put(rom, addr, "b13ac9fff064a539d05fa642a53c7d3df28500a53e6900d050a643a53d7d3df28501a53f6900d041");
		put(rom, 0xf120, "9848a5250a0a451caaa5019d0007e8b13a1865339d0007e8c8b13a45298501a5422901a8a5263924f2f006a50109208501a5019d0007e8a5009d00072028f268a8");
		put(rom, 0xf1d4, "9848a5250a0a451caaa5019d0007e8b13a1865339d0007e8c8b13a45298501a5422901a8a5263926f2f006a50109208501a5019d0007e8a5009d00072028f268a8");
		put(rom, 0xf161, "c8c8e642c640108f688540e643c64130034cf1f068aa201acca9008533852660");
		put(rom, 0xf215, "c8c8c6421091e643c64110874c75f101020201a525492085252920d00ae625a525c9209002e63960");
		put(rom, 0xcc1a, "8e0001a90185128a8dffff4a8dffff4a8dffff4a8dffff4a8dffffa512c901f047");
		put(rom, 0xcc7f, "4c1dccc61260");
		put(rom, 0xc9af, "a9078d1440"); put(rom, 0xc9de, "8d0120a55a");
		return rom;
	}
	std::size_t install(std::vector<byte>& rom, const std::string& spec = "AtlasDevSpriteSpeed",
		word origin = 0xfcce, std::size_t end = 0xffe0) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, origin, end,
			fh::filter_general_hacks(15, fh::parse_general_hacks(spec)), nullptr);
	}
	void reject(std::vector<byte> rom, const std::string& spec = "AtlasDevSpriteSpeed",
		word origin = 0xfcce, std::size_t end = 0xffe0) {
		const auto before{ rom };
		bool threw{ false };
		try { install(rom, spec, origin, end); }
		catch (const std::runtime_error&) { threw = true; }
		require(threw, "incompatible install accepted: " + spec);
		require(rom == before, "rejected install changed the rom");
	}
	void test_modes_and_relocation() {
		for (const auto& mode : { "clear", "draw", "both" }) for (const word org : { 0xc100, 0xfcce, 0xfe80 }) {
			auto rom{ fixture() }; const auto before{ rom };
			const bool clear{ std::string{ mode } != "draw" }, draw{ std::string{ mode } != "clear" };
			require(install(rom, std::string{ "AtlasDevSpriteSpeed mode=" } + mode, org, 0xfff0)
				== (clear ? 204 : 0), "cursor consumption");
			if (clear) {
				auto expected{ bytes("98d005a9f08d0007a9f0") };
				for (unsigned i{ 4 }; i < 256; i += 4)
					expected.insert(expected.end(), { 0x8d, static_cast<byte>(i), 7 });
				const auto tail{ bytes("a0004c8dcb") };
				expected.insert(expected.end(), tail.begin(), tail.end());
				expect(rom, org, expected);
				expect(rom, 0xcb84, { 0x4c, static_cast<byte>(org), static_cast<byte>(org >> 8) });
			}
			if (draw) {
				expect(rom, 0xf120, bytes("a5250a0a451caaa5019d0007b13a1865339d0107c8b13a45298501a5424aa52690014a2901f006a50109208501a5019d0207a5009d03072028f2c84c63f1eaeaea"));
				expect(rom, 0xf1d4, bytes("a5250a0a451caaa5019d0007b13a1865339d0107c8b13a45298501a5424aa526b0014a2901f006a50109208501a5019d0207a5009d03072028f2c84c17f2eaeaea"));
			}
			for (std::size_t i{ 0 }; i < rom.size(); ++i) {
				const auto in = [&](word addr, std::size_t size) {
					const auto begin{ Asm::get_file_offset(15, addr) };
					return i >= begin && i < begin + size;
				};
				const bool owned{ (clear && (in(org, 204) || in(0xcb84, 3)))
					|| (draw && (in(0xf120, 65) || in(0xf1d4, 65))) };
				require(owned || rom[i] == before[i], "write outside owned ranges");
			}
		}
		auto implicit{ fixture() }, explicit_mode{ fixture() };
		install(implicit); install(explicit_mode, "AtlasDevSpriteSpeed mode=both");
		require(implicit == explicit_mode, "default mode differs from both");
		for (const std::string order : { "clear\nAtlasDevSpriteSpeed mode=draw", "draw\nAtlasDevSpriteSpeed mode=clear" }) {
			auto rom{ fixture() };
			require(install(rom, "AtlasDevSpriteSpeed mode=" + order) == 204, "split modes cursor");
			require(rom == implicit, "split modes differ from both");
		}
	}
	void test_refusals() {
		for (const byte bank : { 12, 14 }) {
			auto rom{ fixture() }; const auto before{ rom };
			bool threw{ false };
			try {
				fh::HackManager{}.install_general_hacks(fe::Config{}, rom, bank, 0x8000, 0xc000,
					fh::parse_general_hacks("AtlasDevSpriteSpeed"), nullptr);
			} catch (const std::runtime_error&) { threw = true; }
			require(threw && rom == before, "wrong bank must refuse without writes");
		}
		for (const std::string spec : { "AtlasDevSpriteSpeed mode=fast", "AtlasDevSpriteSpeed ram=1200",
			"AtlasDevSpriteSpeed\nAtlasDevSpriteSpeed", "AtlasDevSpriteSpeed mode=draw\nAtlasDevSpriteSpeed mode=draw" })
			reject(fixture(), spec);
		reject(fixture(), "AtlasDevSpriteSpeed", 0xfcce, 0xfd99);
		auto exact{ fixture() };
		require(install(exact, "AtlasDevSpriteSpeed", 0xfcce, 0xfd9a) == 204, "exact fit failed");
		reject(fixture(), "AtlasDevSpriteSpeed", 0xff40, 0x10000);
		for (const word addr : { 0xcb47, 0xcb65, 0xcb84, 0xcb8d, 0xf0fa, 0xf120, 0xf161,
			0xf175, 0xf1ac, 0xf1d4, 0xf215, 0xf224, 0xf228, 0xcc1a, 0xcc35, 0xcc7f, 0xcc82, 0xfcce }) {
			auto rom{ fixture() }; rom[Asm::get_file_offset(15, addr)] ^= 1; reject(rom);
		}
		auto too_short{ fixture() }; too_short.resize(10); reject(too_short);
		auto expanded{ fixture() }; expanded.resize(0x80010, 0xff); expanded[4] = 32; reject(expanded);
		for (const auto [offset, value] : std::vector<std::pair<unsigned, byte>>{
			{ 0, 0 }, { 4, 8 }, { 5, 1 }, { 6, 0x15 }, { 6, 0x21 }, { 7, 0x10 }, { 7, 4 } }) {
			auto rom{ fixture() }; rom[offset] = value; reject(rom);
		}
	}
	void test_header_variants_and_composition() {
		for (const byte flags : { 0x10, 0x11, 0x12, 0x13 }) for (const byte version : { 0, 8 }) {
			auto rom{ fixture() }; rom[6] = flags; rom[7] = version;
			rom[12] = 0x42; rom[13] = 0x67; rom[14] = 0x89; rom[15] = 0xab;
			require(install(rom) == 204, "mirroring, battery or padding rejected");
		}
		for (const bool scheduler_first : { false, true }) {
			auto rom{ fixture() };
			const std::string spec{ scheduler_first ? "AtlasDevFrameScheduler\nAtlasDevSpriteSpeed"
				: "AtlasDevSpriteSpeed\nAtlasDevFrameScheduler" };
			require(install(rom, spec) == 204 + fh::afs::CORE_SIZE, "scheduler composition size");
			require(fh::afs::find_base(rom) == (scheduler_first ? 0xfcce : 0xfd9a), "scheduler changed");
			const word org{ static_cast<word>(scheduler_first ? 0xfcce + fh::afs::CORE_SIZE : 0xfcce) };
			expect(rom, 0xcb84, { 0x4c, static_cast<byte>(org), static_cast<byte>(org >> 8) });
		}
	}
	void test_occupied_scheduler_roles() {
		const std::string roles{ "AtlasDevFrameScheduler\nAtlasDevDayNightCycle\nAtlasDevInfectedTint\nAtlasDevTimeOfDay" };
		auto reference{ fixture() };
		const auto used{ install(reference, roles) };
		require(used == fh::afs::CORE_SIZE, "role cursor size");
		const auto bank9{ Asm::get_file_offset(9, 0x8000) };
		for (const bool speed_first : { false, true }) {
			auto rom{ fixture() };
			require(install(rom, speed_first ? "AtlasDevSpriteSpeed\n" + roles
				: roles + "\nAtlasDevSpriteSpeed") == used + 204, "active-role composition size");
			const auto base{ fh::afs::find_base(rom) };
			require(base != 0, "active scheduler lost");
			const auto off{ Asm::get_file_offset(15, base) };
			require(rom[off + fh::afs::OFF_POSTARMED] == 1, "POST chain disarmed");
			for (unsigned i{ 0 }; i < 3; ++i)
				require(rom[off + fh::afs::OFF_ARM0 + i] != 0, "occupied slot cleared");
			require(std::equal(reference.begin() + bank9, reference.begin() + bank9 + 0x4000,
				rom.begin() + bank9), "sprite speed changed active role code");
			if (!speed_first) {
				const auto start{ Asm::get_file_offset(15, 0xfcce) };
				require(std::equal(reference.begin() + start, reference.begin() + start + used,
					rom.begin() + start), "sprite speed changed scheduler vectors or code");
			}
		}
	}
	void test_rom_composition(const std::vector<byte>& source, const fe::Config& cfg) {
		const auto apply = [&](std::vector<byte>& rom, const std::string& spec) {
			return fh::HackManager{}.install_general_hacks(cfg, rom, 15, 0xfcce, 0xffe0,
				fh::filter_general_hacks(15, fh::parse_general_hacks(spec)), nullptr);
		};
		for (const std::string other : { "KillSwitch", "BugFixes", "OintmentFix",
			"AtlasDevJumpControl", "AtlasDevFallControl", "AtlasDevLadderControl up=256 down=256",
			"AtlasDevLadderCrown", "AtlasDevLadderCrown mode=floor", "AtlasDevRunControl",
			"AtlasDevEnemyStats hp=125", "AtlasDevCombatFeel walk=256", "AtlasDevLandingTuck",
			"AtlasDevFrameScheduler\nAtlasDevDayNightCycle\nAtlasDevInfectedTint\nAtlasDevTimeOfDay" }) {
			auto reference{ source };
			const auto other_size{ apply(reference, other) };
			for (const std::string mode : { "clear", "draw", "both" }) {
				const std::string speed{ "AtlasDevSpriteSpeed mode=" + mode };
				const std::size_t extra{ mode == "draw" ? 0u : 204u };
				for (const bool first : { false, true }) {
					auto combined{ source };
					const std::string spec{ first ? speed + "\n" + other : other + "\n" + speed };
					if (other_size + extra > 0xffe0 - 0xfcce) {
						bool overflow{ false };
						try { apply(combined, spec); }
						catch (const std::runtime_error& e) {
							const std::string reason{ e.what() };
							overflow = reason.find("Hack overflow") != std::string::npos
								|| reason == "AtlasDevSpriteSpeed: allocated clear-helper range is occupied"
								|| reason == "AtlasDevSpriteSpeed: clear helper reaches interrupt vectors";
						}
						require(overflow && combined == source, "capacity refusal must roll back: " + spec);
						continue;
					}
					require(apply(combined, spec) == other_size + extra, "composition size: " + spec);
					// draw mode allocates nothing, so either order must retain every
					// companion byte. when speed is last its helper follows the companion.
					// crown is deliberately ordered first by the general installer.
					if (!first || extra == 0 || other.starts_with("AtlasDevLadderCrown")) {
						auto expected{ reference };
						fh::HackManager{}.install_general_hacks(cfg, expected, 15,
							0xfcce + other_size, 0xffe0, fh::parse_general_hacks(speed), nullptr);
						require(combined == expected, "companion bytes changed: " + spec);
					} else {
						auto expected{ source };
						fh::HackManager{}.install_general_hacks(cfg, expected, 15,
							0xfcce + extra, 0xffe0,
							fh::filter_general_hacks(15, fh::parse_general_hacks(other)), nullptr);
						fh::HackManager{}.install_general_hacks(cfg, expected, 15,
							0xfcce, 0xfcce + extra, fh::parse_general_hacks(speed), nullptr);
						require(combined == expected, "relocated companion bytes changed: " + spec);
					}
					// the sprite emitters are identical regardless of companion order.
					if (mode != "clear") {
						auto speed_only{ source }; apply(speed_only, speed);
						for (const word addr : { 0xf120, 0xf1d4 }) {
							const auto off{ Asm::get_file_offset(15, addr) };
							require(std::equal(speed_only.begin() + off, speed_only.begin() + off + 65,
								combined.begin() + off), "companion changed sprite emitter: " + spec);
						}
					}
				}
			}
			std::cout << "composition: " << other << " (" << other_size << " bytes): "
				<< (other_size + 204 > 0xffe0 - 0xfcce ? "draw fits; clear/both safely refused" : "all modes fit")
				<< ", both orders: ok\n";
		}
	}
	void export_rom(const char* input, const char* output, const char* region) {
		require(!std::filesystem::exists(output), "output already exists");
		std::ifstream in(input, std::ios::binary);
		require(bool(in), "cannot open source rom");
		std::vector<byte> rom(std::istreambuf_iterator<char>{ in }, {});
		fe::Config cfg;
		cfg.load_definitions(EOE_TEST_CONFIG_PATH, "");
		cfg.set_region(region);
		cfg.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		test_rom_composition(rom, cfg);
		require(install(rom) == 204, "export size");
		std::ofstream out(output, std::ios::binary);
		out.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
		out.close(); require(bool(out), "cannot write output rom");
	}
}

int main(int argc, char** argv) {
	try {
		require(argc == 1 || argc == 3 || argc == 4,
			"usage: atlas_sprite_speed_regression [source.nes output.nes [region]]");
		test_modes_and_relocation(); test_refusals(); test_header_variants_and_composition();
		test_occupied_scheduler_roles();
		if (argc >= 3) export_rom(argv[1], argv[2], argc == 4 ? argv[3] : "us");
		std::cout << "atlas_sprite_speed_regression: modes, bytes, relocation, ownership, rollback, headers, scheduler: ok\n";
	}
	catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
