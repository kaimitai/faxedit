#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/AtlasDevFrameScheduler.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
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
}

int main() {
	try {
		test_scheduler_abi_and_strip_gate();
		test_daynight_claims_first_free_arm_slot();
		test_daynight_reuses_existing_arm_slot();
		test_daynight_skips_boot_off_pre_claimant();
		test_daynight_refuses_owned_post_atomically();
		test_daynight_refuses_full_arm_table_atomically();
		test_daynight_refuses_reserved_arm_slots_atomically();
		test_daynight_refuses_incompatible_existing_kind_atomically();
		std::cout << "atlas scheduler regressions: ok\n";
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "atlas scheduler regressions: " << e.what() << '\n';
		return 1;
	}
}
