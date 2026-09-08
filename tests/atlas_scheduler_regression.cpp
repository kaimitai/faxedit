#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/AtlasDevFrameScheduler.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	constexpr word SCHEDULER_ORG{ 0xfcce };
	constexpr std::size_t ROM_SIZE{ 0x40010 };
	constexpr std::array<byte, 5> HOOK1_ORIG{ 0xa9, 0x07, 0x8d, 0x14, 0x40 };
	constexpr std::array<byte, 5> HOOK2_ORIG{ 0x8d, 0x01, 0x20, 0xa5, 0x5a };

	void require(bool condition, const std::string& message) {
		if (!condition)
			throw std::runtime_error(message);
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		const auto hook1{ klib::Asm6502::get_file_offset(15, 0xc9af) };
		const auto hook2{ klib::Asm6502::get_file_offset(15, 0xc9de) };
		for (std::size_t i{ 0 }; i < HOOK1_ORIG.size(); ++i) {
			rom[hook1 + i] = HOOK1_ORIG[i];
			rom[hook2 + i] = HOOK2_ORIG[i];
		}
		return rom;
	}

	std::vector<fh::GeneralHack> hacks(const std::string& text) {
		return fh::filter_general_hacks(15, fh::parse_general_hacks(text));
	}

	std::size_t install_scheduler(std::vector<byte>& rom) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15,
			SCHEDULER_ORG, 0xfff0, hacks("AtlasDevFrameScheduler"), nullptr);
	}

	void install_daynight(std::vector<byte>& rom, const std::string& params = {}) {
		const auto spec{ params.empty()
			? std::string("AtlasDevDayNightCycle")
			: std::string("AtlasDevDayNightCycle ") + params };
		fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15,
			SCHEDULER_ORG + fh::afs::CORE_SIZE, 0xfff0, hacks(spec), nullptr);
	}

	std::size_t scheduler_file_offset(const std::vector<byte>& rom) {
		const auto base{ fh::afs::find_base(rom) };
		require(base == SCHEDULER_ORG, "scheduler discovery or base changed");
		return klib::Asm6502::get_file_offset(15, base);
	}

	word read_word(const std::vector<byte>& rom, std::size_t offset) {
		return static_cast<word>(rom[offset] | (rom[offset + 1] << 8));
	}

	void test_scheduler_abi_and_strip_gate() {
		auto rom{ vanilla_rom() };
		require(install_scheduler(rom) == fh::afs::CORE_SIZE,
			"scheduler allocation size changed");
		const auto off{ scheduler_file_offset(rom) };
		require(read_word(rom, off + fh::afs::OFF_POST)
			== SCHEDULER_ORG + fh::afs::OFF_STUB,
			"default POST operand does not target the published stub");
		require(rom[off + fh::afs::OFF_STUB] == 0x60,
			"published scheduler stub is not RTS");

		// LDA $73 / ORA $74-$76 / BEQ clear; busy falls through to the
		// RAM_HEAVY store and JMP dma, while quiet clears RAM_HEAVY. The
		// unreachable NOP before clear preserves every ABI offset without
		// adding cycles to the quiet or queue-busy paths (both remain 7
		// cycles from their respective pre-fix entry points).
		const std::array<byte, 20> strip_gate{
			0xa5, 0x73, 0x05, 0x74, 0x05, 0x75, 0x05, 0x76, 0xf0, 0x07,
			0x8d, 0xdd, 0x04, 0x4c, 0, 0, 0xea, 0x8d, 0xdd, 0x04
		};
		bool found{ false };
		for (std::size_t i{ 0 }; i + strip_gate.size() <= fh::afs::CORE_SIZE; ++i) {
			bool match{ true };
			for (std::size_t j{ 0 }; j < strip_gate.size(); ++j) {
				if ((j == 14 || j == 15))
					continue;
				if (rom[off + i + j] != strip_gate[j]) {
					match = false;
					break;
				}
			}
			if (match) {
				found = true;
				break;
			}
		}
		require(found, "scheduler no longer gates PRE on nametable-strip work");
	}

	void test_daynight_claims_first_free_arm_slot() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		const auto off{ scheduler_file_offset(rom) };
		rom[off + fh::afs::OFF_ARM0] = 5;
		rom[off + fh::afs::OFF_ARM0 + 2] = 7;
		install_daynight(rom, "length=8");
		require(rom[off + fh::afs::OFF_ARM0] == 5
			&& rom[off + fh::afs::OFF_ARM0 + 1] == 2
			&& rom[off + fh::afs::OFF_ARM0 + 2] == 7,
			"day/night did not preserve owners and claim the first free arm slot");
		require(read_word(rom, off + fh::afs::OFF_POST) == 0x8000
			&& rom[off + fh::afs::OFF_POSTARMED] == 1,
			"day/night did not claim and arm POST");

		// Disabled entry reads the explicit pending count, initializes a
		// daylight sweep at 8, then decrements once per body call. Enabled
		// entry refreshes the obligation to 8 even for length=8.
		const auto daynight{ klib::Asm6502::get_file_offset(9, 0x8000) };
		const std::array<byte, 29> restore_state_machine{
			0xad, 0xe6, 0x04, 0xf0, 0x12, 0xc9, 0x08, 0xd0, 0x08,
			0xa9, 0x00, 0x8d, 0xe2, 0x04, 0x8d, 0xe3, 0x04,
			0xce, 0xe6, 0x04, 0x4c, 0x00, 0x00, 0x60,
			0xa9, 0x08, 0x8d, 0xe6, 0x04
		};
		bool found{ false };
		for (std::size_t i{ 0 }; i + restore_state_machine.size() < 0x100; ++i) {
			bool match{ true };
			for (std::size_t j{ 0 }; j < restore_state_machine.size(); ++j) {
				if (j == 21 || j == 22)
					continue;
				if (rom[daynight + i + j] != restore_state_machine[j]) {
					match = false;
					break;
				}
			}
			if (match) {
				found = true;
				break;
			}
		}
		require(found, "day/night no longer carries an explicit eight-call restore obligation");
	}

	void test_daynight_reuses_existing_arm_slot() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		const auto off{ scheduler_file_offset(rom) };
		rom[off + fh::afs::OFF_ARM0] = 5;
		rom[off + fh::afs::OFF_ARM0 + 1] = 2;
		rom[off + fh::afs::OFF_ARM0 + 2] = 7;
		install_daynight(rom);
		require(rom[off + fh::afs::OFF_ARM0] == 5
			&& rom[off + fh::afs::OFF_ARM0 + 1] == 2
			&& rom[off + fh::afs::OFF_ARM0 + 2] == 7,
			"day/night did not reuse its existing arm slot");
	}

	void test_daynight_skips_boot_off_pre_claimant() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		const auto off{ scheduler_file_offset(rom) };
		rom[off + fh::afs::OFF_PRE0] = 0x00;
		rom[off + fh::afs::OFF_PRE0 + 1] = 0x90;
		rom[off + fh::afs::OFF_ARM0 + 2] = 7;
		install_daynight(rom);
		require(read_word(rom, off + fh::afs::OFF_PRE0) == 0x9000
			&& rom[off + fh::afs::OFF_ARM0] == 0
			&& rom[off + fh::afs::OFF_ARM0 + 1] == 2
			&& rom[off + fh::afs::OFF_ARM0 + 2] == 7,
			"day/night hijacked a boot-off PRE claimant");
	}

	template<typename Mutator>
	void require_atomic_refusal(const std::string& expected, Mutator mutate) {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		const auto off{ scheduler_file_offset(rom) };
		mutate(rom, off);
		const auto before{ rom };
		bool threw{ false };
		std::string message;
		try {
			install_daynight(rom);
		}
		catch (const std::runtime_error& e) {
			threw = true;
			message = e.what();
		}
		require(threw, "conflicting day/night installation did not throw");
		require(message.find(expected) != std::string::npos,
			"conflicting day/night installation reported the wrong error");
		require(rom == before, "conflicting day/night installation mutated the ROM");
	}

	void test_daynight_refuses_owned_post_atomically() {
		require_atomic_refusal("POST lane", [](auto& rom, std::size_t off) {
			rom[off + fh::afs::OFF_POST] = 0x00;
			rom[off + fh::afs::OFF_POST + 1] = 0x90;
		});
		require_atomic_refusal("POST lane", [](auto& rom, std::size_t off) {
			rom[off + fh::afs::OFF_POSTARMED] = 1;
		});
	}

	void test_daynight_refuses_full_arm_table_atomically() {
		require_atomic_refusal("no unclaimed slot", [](auto& rom, std::size_t off) {
			rom[off + fh::afs::OFF_ARM0] = 5;
			rom[off + fh::afs::OFF_ARM0 + 1] = 6;
			rom[off + fh::afs::OFF_ARM0 + 2] = 7;
		});
	}

	void test_daynight_refuses_reserved_arm_slots_atomically() {
		require_atomic_refusal("no unclaimed slot", [](auto& rom, std::size_t off) {
			constexpr std::size_t pre_sites[3]{
				fh::afs::OFF_PRE0, fh::afs::OFF_PRE1, fh::afs::OFF_PRE2
			};
			for (std::size_t i{ 0 }; i < 3; ++i) {
				rom[off + pre_sites[i]] = 0x00;
				rom[off + pre_sites[i] + 1] = static_cast<byte>(0x90 + i);
			}
		});
	}

	void test_daynight_refuses_incompatible_existing_kind_atomically() {
		require_atomic_refusal("kind 2 slot has a PRE claimant", [](auto& rom, std::size_t off) {
			rom[off + fh::afs::OFF_ARM0] = 2;
			rom[off + fh::afs::OFF_PRE0] = 0x00;
			rom[off + fh::afs::OFF_PRE0 + 1] = 0x90;
		});
	}

	// AtlasDevJumpControl: three retargeted vanilla instructions in the bank 15
	// jump code, stubs at the general hack cursor, one ram byte
	constexpr word JC_FALL_SITE{ 0xe3cd }, JC_INIT_SITE{ 0xe444 }, JC_ARC_SITE{ 0xe463 };
	constexpr word JC_TABLE{ 0xe4d6 };
	constexpr std::array<byte, 4> JC_FALL_ORIG{ 0xa5, 0xa5, 0x10, 0x00 };
	constexpr std::array<byte, 5> JC_INIT_ORIG{ 0xa5, 0xa4, 0x4a, 0xb0, 0x1a };
	constexpr std::array<byte, 4> JC_ARC_ORIG{ 0xa6, 0xa6, 0xe0, 0x10 };
	constexpr std::array<byte, 32> JC_TABLE_BYTES{
		8, 4, 4, 4, 4, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 4, 4, 4, 4, 8 };

	template<std::size_t N>
	void seed(std::vector<byte>& rom, word addr, const std::array<byte, N>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(15, addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			rom[off + i] = bytes[i];
	}

	std::vector<byte> jump_rom() {
		auto rom{ vanilla_rom() };
		seed(rom, JC_FALL_SITE, JC_FALL_ORIG);
		seed(rom, JC_INIT_SITE, JC_INIT_ORIG);
		seed(rom, JC_ARC_SITE, JC_ARC_ORIG);
		seed(rom, JC_TABLE, JC_TABLE_BYTES);
		return rom;
	}

	std::size_t install_jump_control(std::vector<byte>& rom, const std::string& params = {},
		word cpu_addr = SCHEDULER_ORG) {
		std::string spec{ "AtlasDevJumpControl" };
		if (!params.empty())
			spec += " " + params;
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15,
			cpu_addr, 0xfff0, hacks(spec), nullptr);
	}

	void test_jump_control_installs_three_hooks_and_advances_the_cursor() {
		auto rom{ jump_rom() };
		const auto before{ rom };
		const auto size{ install_jump_control(rom) };
		require(size == 152, "default jump control body is not 152 bytes");
		const auto fall{ klib::Asm6502::get_file_offset(15, JC_FALL_SITE) };
		require(rom[fall] == 0x4c && klib::Asm6502::read_word(rom, fall + 1) == SCHEDULER_ORG
			&& rom[fall + 3] == 0xea, "fall site is not jmp stub / nop");
		const auto init{ klib::Asm6502::get_file_offset(15, JC_INIT_SITE) };
		const word init_target{ klib::Asm6502::read_word(rom, init + 1) };
		require(rom[init] == 0x4c && init_target > SCHEDULER_ORG && init_target < SCHEDULER_ORG + size
			&& rom[init + 3] == 0xea && rom[init + 4] == 0xea, "init site is not jmp stub / nop / nop");
		const auto arc{ klib::Asm6502::get_file_offset(15, JC_ARC_SITE) };
		const word arc_target{ klib::Asm6502::read_word(rom, arc + 1) };
		require(rom[arc] == 0x20 && arc_target > init_target && arc_target < SCHEDULER_ORG + size
			&& rom[arc + 3] == 0xea, "arc site is not jsr stub / nop");
		// the hop stub must hand the carry of cpx #$10 back to the vanilla branches
		std::size_t rts{ klib::Asm6502::get_file_offset(15, arc_target) };
		while (rom[rts] != 0x60)
			++rts;
		require(rom[rts - 2] == 0xe0 && rom[rts - 1] == 0x10, "hop stub does not end cpx #$10 / rts");
		// nothing outside the body and the three sites changed
		const auto body{ klib::Asm6502::get_file_offset(15, SCHEDULER_ORG) };
		for (std::size_t i{ 0 }; i < rom.size(); ++i) {
			const bool in_body{ i >= body && i < body + size };
			const bool in_site{ (i >= fall && i < fall + 4) || (i >= init && i < init + 5)
				|| (i >= arc && i < arc + 4) };
			if (!in_body && !in_site)
				require(rom[i] == before[i], "jump control changed a byte outside its body and sites");
		}
		auto air{ jump_rom() };
		require(install_jump_control(air, "airjumps=1") == 214, "5/5/3/1 jump control body is not 214 bytes");
		auto off_rom{ jump_rom() };
		const auto untouched{ off_rom };
		require(install_jump_control(off_rom, "coyote=0 buffer=0 shorthop=0 airjumps=0") == 0
			&& off_rom == untouched, "all-off jump control installed something");
	}

	template<typename Mutator>
	void require_jump_control_refusal(const std::string& expected, const std::string& params, Mutator mutate) {
		auto rom{ jump_rom() };
		mutate(rom);
		const auto before{ rom };
		bool threw{ false };
		std::string message;
		try {
			install_jump_control(rom, params);
		}
		catch (const std::runtime_error& e) {
			threw = true;
			message = e.what();
		}
		require(threw, "jump control did not refuse: " + expected);
		require(message.find(expected) != std::string::npos,
			"jump control refused with the wrong error: " + message);
		require(rom == before, "jump control refusal mutated the ROM: " + expected);
	}

	void test_jump_control_refuses_atomically() {
		require_jump_control_refusal("$e3cd", {}, [](auto& rom) {
			rom[klib::Asm6502::get_file_offset(15, JC_FALL_SITE) + 1] = 0xa4; });
		require_jump_control_refusal("$e444", {}, [](auto& rom) {
			rom[klib::Asm6502::get_file_offset(15, JC_INIT_SITE) + 4] = 0x1b; });
		require_jump_control_refusal("$e463", {}, [](auto& rom) {
			rom[klib::Asm6502::get_file_offset(15, JC_ARC_SITE)] = 0xa5; });
		require_jump_control_refusal("mirror", {}, [](auto& rom) {
			rom[klib::Asm6502::get_file_offset(15, JC_TABLE) + 31] = 7; });
		require_jump_control_refusal("not free", {}, [](auto& rom) {
			rom[klib::Asm6502::get_file_offset(15, SCHEDULER_ORG) + 40] = 0x00; });
		require_jump_control_refusal("0 to 15", "coyote=16", [](auto&) {});
		require_jump_control_refusal("Unknown parameter", "cayote=5", [](auto&) {});
	}

	void test_jump_control_switchable_is_a_scheduler_client() {
		// without the scheduler: refused, untouched
		require_jump_control_refusal("requires the AtlasDevFrameScheduler", "switchable=1", [](auto&) {});
		// with it: kind 6 lands in the first free boot slot and the body grows by the gates
		auto rom{ jump_rom() };
		install_scheduler(rom);
		const auto scheduler{ scheduler_file_offset(rom) };
		const auto before{ rom };
		const auto size{ install_jump_control(rom, "switchable=1", SCHEDULER_ORG + fh::afs::CORE_SIZE) };
		require(size == 152 + 82, "switchable default body is not 234 bytes");
		require(rom[scheduler + fh::afs::OFF_ARM0] == 0x06, "switchable did not arm kind 6 in slot 0");
		require(rom[scheduler + fh::afs::OFF_ARM0 + 1] == 0x00, "switchable armed more than one slot");
		// armed=0 installs the gates but claims no slot
		auto dormant{ jump_rom() };
		install_scheduler(dormant);
		install_jump_control(dormant, "switchable=1 armed=0", SCHEDULER_ORG + fh::afs::CORE_SIZE);
		for (std::size_t i{ 0 }; i < 3; ++i)
			require(dormant[scheduler + fh::afs::OFF_ARM0 + i] == 0x00, "armed=0 claimed a slot");
		// a full arm table refuses atomically
		auto full{ jump_rom() };
		install_scheduler(full);
		for (std::size_t i{ 0 }; i < 3; ++i)
			full[scheduler + fh::afs::OFF_ARM0 + i] = static_cast<byte>(2 + i);
		const auto full_before{ full };
		bool threw{ false };
		try {
			install_jump_control(full, "switchable=1", SCHEDULER_ORG + fh::afs::CORE_SIZE);
		}
		catch (const std::runtime_error& e) {
			threw = std::string(e.what()).find("no free scheduler slot") != std::string::npos;
		}
		require(threw && full == full_before, "a full arm table was not refused atomically");
		// the plain install still claims nothing in the scheduler
		auto plain{ jump_rom() };
		install_scheduler(plain);
		install_jump_control(plain, {}, SCHEDULER_ORG + fh::afs::CORE_SIZE);
		for (std::size_t i{ 0 }; i < 3; ++i)
			require(plain[scheduler + fh::afs::OFF_ARM0 + i] == 0x00, "a plain install touched the arm table");
	}

	void write_jump_control_parity(const std::string& base, const std::string& out, const std::string& params) {
		std::ifstream in(base, std::ios::binary);
		if (!in)
			throw std::runtime_error("cannot read " + base);
		std::vector<byte> rom((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (params.find("switchable=1") != std::string::npos) {
			install_scheduler(rom);
			install_jump_control(rom, params, SCHEDULER_ORG + fh::afs::CORE_SIZE);
		}
		else
			install_jump_control(rom, params);
		std::ofstream o(out, std::ios::binary);
		o.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
	}
}

int main(int argc, char** argv) {
	try {
		if ((argc == 4 || argc == 5) && std::string(argv[1]) == "--write-jumpcontrol-parity") {
			write_jump_control_parity(argv[2], argv[3], argc == 5 ? argv[4] : "");
			return 0;
		}
		if (argc != 1)
			throw std::runtime_error("usage: atlas_scheduler_regression [--write-jumpcontrol-parity BASE OUT [PARAMS]]");
		test_scheduler_abi_and_strip_gate();
		test_daynight_claims_first_free_arm_slot();
		test_daynight_reuses_existing_arm_slot();
		test_daynight_skips_boot_off_pre_claimant();
		test_daynight_refuses_owned_post_atomically();
		test_daynight_refuses_full_arm_table_atomically();
		test_daynight_refuses_reserved_arm_slots_atomically();
		test_daynight_refuses_incompatible_existing_kind_atomically();
		test_jump_control_installs_three_hooks_and_advances_the_cursor();
		test_jump_control_refuses_atomically();
		test_jump_control_switchable_is_a_scheduler_client();
		std::cout << "atlas scheduler regressions: ok\n";
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "atlas scheduler regressions: " << e.what() << '\n';
		return 1;
	}
}
