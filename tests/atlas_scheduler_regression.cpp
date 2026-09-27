#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/AtlasDevFrameScheduler.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <algorithm>
#include <array>
#include <cmath>
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

	enum class PostRole { tint, time_of_day };
	constexpr std::size_t BANK9_SIZE{ 0x4000 };

	const char* role_hack_name(PostRole role) {
		return role == PostRole::tint ? "AtlasDevInfectedTint" : "AtlasDevTimeOfDay";
	}

	byte role_kind(PostRole role) {
		return role == PostRole::tint ? 3 : 4;
	}

	void install_role(std::vector<byte>& rom, PostRole role, const std::string& params = {}) {
		std::string spec{ role_hack_name(role) };
		if (!params.empty())
			spec += " " + params;
		fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15,
			SCHEDULER_ORG + fh::afs::CORE_SIZE, 0xfff0, hacks(spec), nullptr);
	}

	std::size_t install_status_ward(std::vector<byte>& rom,
		const std::string& params = {}, word cpu_addr = SCHEDULER_ORG + fh::afs::CORE_SIZE) {
		std::string spec{ "AtlasDevStatusWard" };
		if (!params.empty())
			spec += " " + params;
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15,
			cpu_addr, 0xfff0, hacks(spec), nullptr);
	}

	std::size_t scheduler_file_offset(const std::vector<byte>& rom) {
		const auto base{ fh::afs::find_base(rom) };
		require(base == SCHEDULER_ORG, "scheduler discovery or base changed");
		return klib::Asm6502::get_file_offset(15, base);
	}

	word read_word(const std::vector<byte>& rom, std::size_t offset) {
		return static_cast<word>(rom[offset] | (rom[offset + 1] << 8));
	}

	// ROM-free interpreter for the emitted PRE field routine only. No NES
	// devices, game code, ROM assets, or frame loop participate in these tests.
	struct FieldCpu {
		std::array<byte, 0x10000> memory{};
		word pc{};
		byte a{}, x{};
		bool carry{}, zero{}, negative{};

		void flags(byte value) { zero = value == 0; negative = (value & 0x80) != 0; }
		word operand() {
			const word lo{ memory[pc++] };
			return static_cast<word>(lo | (memory[pc++] << 8));
		}
		void add(byte value) {
			const unsigned sum{ static_cast<unsigned>(a) + value + carry };
			carry = sum > 255;
			a = static_cast<byte>(sum);
			flags(a);
		}
		void branch(bool taken) {
			const auto delta{ static_cast<std::int8_t>(memory[pc++]) };
			if (taken) pc = static_cast<word>(pc + delta);
		}
		void run(word entry) {
			pc = entry;
			a = 5; x = 0;
			carry = zero = negative = false;
			for (unsigned steps{ 0 }; steps < 1000; ++steps) {
				switch (memory[pc++]) {
				case 0xa9: a = memory[pc++]; flags(a); break;
				case 0xa5: a = memory[memory[pc++]]; flags(a); break;
				case 0xad: a = memory[operand()]; flags(a); break;
				case 0xbd: a = memory[static_cast<word>(operand() + x)]; flags(a); break;
				case 0x8d: memory[operand()] = a; break;
				case 0xc9: {
					const byte value{ memory[pc++] };
					carry = a >= value; flags(static_cast<byte>(a - value)); break;
				}
				case 0x18: carry = false; break;
				case 0x65: add(memory[memory[pc++]]); break;
				case 0x69: add(memory[pc++]); break;
				case 0x7d: add(memory[static_cast<word>(operand() + x)]); break;
				case 0x4a: carry = (a & 1) != 0; a >>= 1; flags(a); break;
				case 0x0a: carry = (a & 0x80) != 0; a <<= 1; flags(a); break;
				case 0xaa: x = a; flags(x); break;
				case 0x29: a &= memory[pc++]; flags(a); break;
				case 0x09: a |= memory[pc++]; flags(a); break;
				case 0x49: a ^= memory[pc++]; flags(a); break;
				case 0x30: branch(negative); break;
				case 0x90: branch(!carry); break;
				case 0xb0: branch(carry); break;
				case 0xd0: branch(!zero); break;
				case 0xf0: branch(zero); break;
				case 0x4c: pc = operand(); break;
				case 0x60: return;
				default: throw std::runtime_error("unexpected opcode in field fixture");
				}
			}
			throw std::runtime_error("field fixture did not return");
		}
	};

	FieldCpu field_cpu(const std::vector<byte>& rom) {
		FieldCpu cpu;
		for (const byte bank : { byte{9}, byte{15} }) {
			const word cpu_base{ bank == 15 ? word{0xc000} : word{0x8000} };
			const auto file{ klib::Asm6502::get_file_offset(bank, cpu_base) };
			for (std::size_t i{ 0 }; i < 0x4000; ++i)
				cpu.memory[cpu_base + i] = rom[file + i];
		}
		return cpu;
	}

	word field_entry(const std::vector<byte>& rom) {
		return read_word(rom, scheduler_file_offset(rom) + fh::afs::OFF_PRE0);
	}

	std::pair<std::size_t, std::size_t> changed_bank9_span(
		const std::vector<byte>& before, const std::vector<byte>& after) {
		const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
		std::size_t first{ BANK9_SIZE };
		std::size_t last{ 0 };
		for (std::size_t i{ 0 }; i < BANK9_SIZE; ++i)
			if (before[bank9 + i] != after[bank9 + i]) {
				if (first == BANK9_SIZE)
					first = i;
				last = i;
			}
		require(first != BANK9_SIZE, "role install changed no bank-9 bytes");
		return { first, last };
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

	void test_scheduler_skips_gate_only_kinds() {
		auto rom{ vanilla_rom() };
		require(install_scheduler(rom) == 156, "the gate-only revision is not 156 bytes");
		const auto off{ scheduler_file_offset(rom) };
		// every slot test is LDA slot / BEQ +5 / BMI +3 / JSR vector, so an
		// empty slot and a gate-only kind both skip the call
		for (word slot : { fh::afs::RAM_SLOT0, fh::afs::RAM_SLOT1, fh::afs::RAM_SLOT2 }) {
			const std::array<byte, 8> dispatch{ 0xad, static_cast<byte>(slot & 0xff), static_cast<byte>(slot >> 8),
				0xf0, 0x05, 0x30, 0x03, 0x20 };
			bool found{ false };
			for (std::size_t i{ 0 }; i + dispatch.size() <= fh::afs::CORE_SIZE && !found; ++i) {
				bool match{ true };
				for (std::size_t j{ 0 }; j < dispatch.size(); ++j)
					if (rom[off + i + j] != dispatch[j]) { match = false; break; }
				found = match;
			}
			require(found, "a slot test lacks the gate-only BMI");
		}
		require(read_word(rom, off + fh::afs::OFF_PRE0) == SCHEDULER_ORG + fh::afs::OFF_STUB
			&& read_word(rom, off + fh::afs::OFF_PRE2) == SCHEDULER_ORG + fh::afs::OFF_STUB,
			"PRE operands do not sit at the published offsets");
		require(fh::afs::GATE_ONLY_KIND == 0x80, "gate-only kinds start at $80");
		// an earlier revision's hook is refused by name, not as a foreign byte
		auto old{ vanilla_rom() };
		const auto h1{ klib::Asm6502::get_file_offset(15, 0xc9af) };
		old[h1] = 0x20; old[h1 + 1] = 0xd4; old[h1 + 2] = 0xfc; old[h1 + 3] = 0xea; old[h1 + 4] = 0xea;
		bool threw{ false };
		try {
			install_scheduler(old);
		}
		catch (const std::runtime_error& e) {
			threw = std::string(e.what()).find("earlier revision") != std::string::npos;
		}
		require(threw, "an installed hook was not reported as an earlier revision");
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
		require(rom[scheduler + fh::afs::OFF_ARM0] == 0x86, "switchable did not arm kind $86 in slot 0");
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

	void test_status_ward_install_survives_transactional_copy() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		const auto before{ rom };
		install_status_ward(rom);
		const auto scheduler{ scheduler_file_offset(rom) };
		const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
		require(rom != before, "StatusWard install was discarded by the general-hack transaction");
		require(read_word(rom, scheduler + fh::afs::OFF_POST) == 0x8000
			&& rom[scheduler + fh::afs::OFF_POSTARMED] == 1,
			"StatusWard did not publish its POST body");
		require(rom[scheduler + fh::afs::OFF_ARM0] == 5,
			"StatusWard did not claim the first free arm slot");
		require(rom[bank9] != 0xff, "StatusWard wrote no bank-9 body");
	}

	void test_status_ward_is_a_real_dormant_fourth_role() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		install_daynight(rom);
		install_role(rom, PostRole::tint);
		install_role(rom, PostRole::time_of_day);
		const auto scheduler{ scheduler_file_offset(rom) };
		const word previous{ read_word(rom, scheduler + fh::afs::OFF_POST) };
		const auto before{ rom };
		install_status_ward(rom);
		const word ward{ read_word(rom, scheduler + fh::afs::OFF_POST) };
		const auto [first, last]{ changed_bank9_span(before, rom) };
		const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
		require(ward == static_cast<word>(0x8000 + first)
			&& rom[bank9 + last - 2] == 0x4c
			&& read_word(rom, bank9 + last - 1) == previous,
			"StatusWard did not tail-chain after the three preceding roles");
		require(rom[scheduler + fh::afs::OFF_ARM0] == 2
			&& rom[scheduler + fh::afs::OFF_ARM0 + 1] == 3
			&& rom[scheduler + fh::afs::OFF_ARM0 + 2] == 4,
			"dormant fourth-role install displaced an active scheduler kind");
	}

	void test_status_ward_field_claims_pre_lanes_and_edgepark_is_opt_in() {
		auto omitted{ vanilla_rom() };
		auto explicit_off{ vanilla_rom() };
		auto edge_on{ vanilla_rom() };
		install_scheduler(omitted);
		install_scheduler(explicit_off);
		install_scheduler(edge_on);
		const std::string common{
			"radius=48 push=1 exempt=34 trigger=wingboots pull=false arc=front "
			"fieldpulse=24 aura=true fieldfx=true fieldtile=64 fieldattr=0 "
			"fieldspeed=4 fieldcount=4 fielddir=ccw" };
		const auto omitted_size{ install_status_ward(omitted, common) };
		const auto off_size{ install_status_ward(explicit_off, common + " edgepark=false") };
		const auto on_size{ install_status_ward(edge_on, common + " edgepark=true") };
		require(omitted == explicit_off && omitted_size == off_size,
			"omitting edgepark changed the prepared default emission");
		require(edge_on != omitted && on_size > off_size,
			"edgepark did not add its guarded field-graphic path (changed="
			+ std::to_string(edge_on != omitted) + ", off="
			+ std::to_string(off_size) + ", on=" + std::to_string(on_size) + ")");
		const auto scheduler{ scheduler_file_offset(edge_on) };
		const word entry{ static_cast<word>(SCHEDULER_ORG + fh::afs::CORE_SIZE + 16) };
		for (const auto pre : { fh::afs::OFF_PRE0, fh::afs::OFF_PRE1, fh::afs::OFF_PRE2 })
			require(read_word(edge_on, scheduler + pre) == entry,
				"StatusWard did not claim every PRE lane with the field entry");
	}

	void test_status_ward_refuses_a_pre_claim_atomically() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		const auto scheduler{ scheduler_file_offset(rom) };
		rom[scheduler + fh::afs::OFF_PRE1] = 0x00;
		rom[scheduler + fh::afs::OFF_PRE1 + 1] = 0x90;
		const auto before{ rom };
		bool threw{ false };
		std::string message;
		try {
			install_status_ward(rom, "fieldfx=true edgepark=true");
		}
		catch (const std::runtime_error& e) {
			threw = true;
			message = e.what();
		}
		require(threw && message.find("PRE lane") != std::string::npos,
			"StatusWard accepted an occupied PRE lane or reported the wrong error");
		require(rom == before, "StatusWard PRE-lane refusal mutated the caller ROM");
	}

	void test_status_ward_edgepark_clips_both_axes() {
		constexpr word FX_ORG{ SCHEDULER_ORG + fh::afs::CORE_SIZE };
		constexpr double PI{ 3.14159265358979323846 };
		std::size_t cases{ 0 };
		for (unsigned radius{ 4 }; radius <= 120; ++radius) {
			auto rom{ vanilla_rom() };
			install_scheduler(rom);
			install_status_ward(rom, "trigger=always fieldfx=true edgepark=true "
				"fieldcount=1 fieldspeed=5 radius=" + std::to_string(radius));
			FieldCpu cpu;
			const auto fixed{ klib::Asm6502::get_file_offset(15, 0xc000) };
			for (std::size_t i{ 0 }; i < 0x4000; ++i)
				cpu.memory[0xc000 + i] = rom[fixed + i];
			for (unsigned angle{ 0 }; angle < 8; ++angle) {
				const int dx{ static_cast<int>(std::lround(radius * std::cos(angle * PI / 4))) + 4 };
				const int dy{ static_cast<int>(std::lround(radius * std::sin(angle * PI / 4))) + 43 };
				cpu.memory[fh::afs::RAM_CNT_LO] = static_cast<byte>(angle * 2);
				const auto check = [&](unsigned player_x, unsigned player_y) {
					cpu.memory[0x9e] = static_cast<byte>(player_x);
					cpu.memory[0xa1] = static_cast<byte>(player_y);
					cpu.run(FX_ORG + 16);
					const int x{ static_cast<int>(player_x) + dx };
					const int y{ static_cast<int>(player_y) + dy };
					const bool visible{ x >= 0 && x < 256 && y >= 0 && y < 240 };
					require(cpu.memory[0x07f0] == (visible ? y : 0xf8),
						"edgepark Y mismatch: radius=" + std::to_string(radius)
						+ " angle=" + std::to_string(angle) + " player="
						+ std::to_string(player_x) + "," + std::to_string(player_y));
					if (visible) require(cpu.memory[0x07f3] == x, "edgepark changed visible X");
					require(cpu.memory[0x07f1] == 0x40 && cpu.memory[0x07f2] == 0,
						"edgepark lost orb tile or attributes");
					++cases;
				};
				for (unsigned y{ 0 }; y < 256; ++y) check(128, y);
				for (unsigned x{ 0 }; x < 256; ++x) check(x, 80);
			}
		}
		require(cases == 479232, "edgepark boundary matrix shrank");
	}

	// FNV-1a over bank 9 and the field range at a fixed origin. The core is
	// deliberately excluded: upstream scheduler evolution must not invalidate
	// independent StatusWard emission evidence (76f4660 and 399063e oracles).
	std::uint64_t ward_digest(const std::vector<byte>& rom) {
		std::uint64_t digest{ 14695981039346656037ULL };
		for (const auto range : { std::pair<std::size_t, std::size_t>{0x24010, 0x28010},
			std::pair<std::size_t, std::size_t>{0x3fdce, 0x40010} })
			for (auto i{ range.first }; i < range.second; ++i) {
				digest ^= rom[i]; digest *= 1099511628211ULL;
			}
		return digest;
	}

	void test_status_ward_unclipped_emission_stays_legacy() {
		// FNV-1a of StatusWard-owned ranges emitted by 76f4660. These
		// pin the opt-out path independently of the new coordinate oracle.
		const std::array<std::pair<std::string, std::uint64_t>, 3> cases{{
			{"fieldfx=true", 6967315546989672379ULL},
			{"trigger=always fieldfx=true radius=120 fieldspeed=5 fieldcount=1 fielddir=ccw",
				6603235945068683168ULL},
			{"trigger=any fieldfx=true radius=4 fieldspeed=0 fieldcount=2 fieldpulse=24",
				1712727589137627768ULL},
		}};
		for (const auto& [params, expected] : cases) {
			auto rom{ vanilla_rom() };
			install_scheduler(rom);
			install_status_ward(rom, params + " edgepark=false", 0xfdbe);
			const auto digest{ ward_digest(rom) };
			require(digest == expected, "edgepark=false changed legacy emission: " + params);
		}
	}

	void test_status_ward_visual_defaults_preserve_both_legacy_paths() {
		const std::array<std::pair<std::string, std::uint64_t>, 3> clipped{{
			{"fieldfx=true", 6487282305893163547ULL},
			{"trigger=always fieldfx=true radius=120 fieldspeed=5 fieldcount=1 fielddir=ccw",
				3154340736207787606ULL},
			{"trigger=any fieldfx=true radius=4 fieldspeed=0 fieldcount=2 fieldpulse=24",
				1242123543599924236ULL},
		}};
		for (const auto& [params, expected] : clipped) {
			for (const bool edge : { false, true }) {
				auto omitted{ vanilla_rom() }, explicit_defaults{ omitted };
				install_scheduler(omitted); install_scheduler(explicit_defaults);
				const auto spec{ params + (edge ? " edgepark=true" : " edgepark=false") };
				install_status_ward(omitted, spec, 0xfdbe);
				install_status_ward(explicit_defaults, spec
					+ " fieldpattern=orbit fieldblink=0 fieldframes=1 fieldanimspeed=3", 0xfdbe);
				require(omitted == explicit_defaults, "explicit visual defaults changed emission");
				if (edge) {
					const auto digest{ ward_digest(omitted) };
					require(digest == expected, "new visual defaults changed legacy clipped emission");
				}
			}
		}
		auto plain{ vanilla_rom() }, visual_options{ plain };
		install_scheduler(plain); install_scheduler(visual_options);
		install_status_ward(plain);
		install_status_ward(visual_options,
			"fieldpattern=contract fieldblink=24 fieldframes=4 fieldanimspeed=5");
		require(plain == visual_options, "visual options changed a fieldfx=false installation");
	}

	void test_status_ward_visual_pattern_coordinates() {
		constexpr double PI{ 3.14159265358979323846 };
		std::size_t samples{ 0 };
		for (const std::string pattern : { "orbit", "expand", "contract" })
		for (const unsigned radius : { 4U, 13U, 41U, 85U, 120U })
		for (const unsigned count : { 1U, 2U, 4U })
		for (unsigned speed{ 0 }; speed <= 5; ++speed)
		for (const bool ccw : { false, true })
		for (const bool edge : { false, true }) {
			auto rom{ vanilla_rom() }; install_scheduler(rom);
			install_status_ward(rom, "trigger=always fieldfx=true fieldpattern=" + pattern
				+ " radius=" + std::to_string(radius) + " fieldcount=" + std::to_string(count)
				+ " fieldspeed=" + std::to_string(speed) + (ccw ? " fielddir=ccw" : " fielddir=cw")
				+ (edge ? " edgepark=true" : " edgepark=false"));
			auto cpu{ field_cpu(rom) };
			const word entry{ field_entry(rom) };
			for (unsigned counter{ 0 }; counter < 256; ++counter) {
				cpu.memory[fh::afs::RAM_CNT_LO] = static_cast<byte>(counter);
				for (const auto [px, py] : { std::pair{0, 0}, std::pair{128, 80}, std::pair{255, 255} }) {
					cpu.memory[0x9e] = static_cast<byte>(px); cpu.memory[0xa1] = static_cast<byte>(py);
					cpu.run(entry);
					for (unsigned orb{ 0 }; orb < count; ++orb) {
						unsigned angle{ orb * 8 / count };
						unsigned stage_radius{ radius };
						if (pattern == "orbit") angle = (angle + (speed ? counter >> (6 - speed) : 0)) & 7;
						else {
							unsigned stage{ speed ? ((counter >> (6 - speed)) & 3) : 3 };
							if (speed && pattern == "contract") stage = 3 - stage;
							stage_radius = static_cast<unsigned>(std::lround(radius * (stage + 1) / 4.0));
						}
						if (ccw) angle = 7 - angle;
						const int x{ px + static_cast<int>(std::lround(stage_radius * std::cos(angle * PI / 4))) + 4 };
						const int y{ py + static_cast<int>(std::lround(stage_radius * std::sin(angle * PI / 4))) + 43 };
						const bool parked{ edge && (x < 0 || x >= 256 || y < 0 || y >= 240) };
						const word oam{ static_cast<word>(0x07f0 + 4 * orb) };
						require(cpu.memory[oam] == (parked ? 0xf8 : static_cast<byte>(y)),
							"visual pattern Y mismatch: " + pattern + " radius=" + std::to_string(radius));
						if (!parked) require(cpu.memory[oam + 3] == static_cast<byte>(x), "visual pattern X mismatch");
						++samples;
					}
				}
			}
		}
		require(samples == 1935360, "visual pattern coordinate matrix shrank");
	}

	void test_status_ward_visual_blink_and_animated_tiles() {
		for (const unsigned frames : { 1U, 2U, 4U })
		for (unsigned animspeed{ 0 }; animspeed <= 5; ++animspeed)
		for (const unsigned tile : { 0U, 64U, 256U - frames })
		for (const unsigned blink : { 0U, 1U, 24U, 255U })
		for (const unsigned pulse : { 0U, 24U }) {
			auto rom{ vanilla_rom() }; install_scheduler(rom);
			const std::string spec{ "trigger=always fieldfx=true fieldcount=1 fieldspeed=0 fieldframes="
				+ std::to_string(frames) + " fieldanimspeed=" + std::to_string(animspeed)
				+ " fieldtile=" + std::to_string(tile) + " fieldpulse=" + std::to_string(pulse) };
			install_status_ward(rom, spec + " fieldblink=" + std::to_string(blink));
			auto baseline{ vanilla_rom() }; install_scheduler(baseline); install_status_ward(baseline, spec);
			const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
			for (std::size_t i{ 0 }; i < BANK9_SIZE; ++i)
				require(rom[bank9 + i] == baseline[bank9 + i], "visual blink changed POST force body");
			auto cpu{ field_cpu(rom) };
			const word entry{ field_entry(rom) };
			cpu.memory[0x9e] = 100; cpu.memory[0xa1] = 80;
			for (unsigned counter{ 0 }; counter < 256; ++counter) {
				cpu.memory[fh::afs::RAM_CNT_LO] = static_cast<byte>(counter);
				cpu.memory[0x07f0] = 100; cpu.memory[0x07f1] = static_cast<byte>(tile);
				cpu.run(entry);
				const bool on{ (!blink || (counter & blink)) && (!pulse || (counter & pulse)) };
				const unsigned frame{ animspeed ? ((counter >> (6 - animspeed)) & (frames - 1)) : 0 };
				require(cpu.memory[0x07f0] == (on ? 123 : 0xf8), "visual blink/pulse gate mismatch");
				if (on) require(cpu.memory[0x07f1] == tile + frame, "animated tile frame mismatch");
			}
		}
		// Both PRE timer cleanup and POST disarm cleanup must recognize every
		// frame, including a tile interval ending at 256, and preserve others.
		for (const unsigned frames : { 1U, 2U, 4U })
		for (const unsigned tile : { 0U, 64U, 256U - frames }) {
			auto rom{ vanilla_rom() }; install_scheduler(rom);
			install_status_ward(rom, "fieldfx=true fieldframes=" + std::to_string(frames)
				+ " fieldtile=" + std::to_string(tile));
			auto cpu{ field_cpu(rom) };
			const word post{ read_word(rom, scheduler_file_offset(rom) + fh::afs::OFF_POST) };
			for (const word entry : { field_entry(rom), post })
			for (unsigned candidate{ 0 }; candidate < 256; ++candidate) {
				for (word orb{ 0 }; orb < 4; ++orb) {
					cpu.memory[0x07f0 + 4 * orb] = 100;
					cpu.memory[0x07f1 + 4 * orb] = static_cast<byte>(candidate);
				}
				cpu.run(entry);
				const bool owned{ candidate >= tile && candidate < tile + frames };
				for (word orb{ 0 }; orb < 4; ++orb)
					require(cpu.memory[0x07f0 + 4 * orb] == (owned ? 0xf8 : 100), "animated parking ownership mismatch");
			}
		}
	}

	void test_status_ward_visual_parameters_and_capacity() {
		for (const std::string params : { "fieldpattern=spiral", "fieldblink=256", "fieldblink=-1",
			"fieldframes=0", "fieldframes=3", "fieldframes=5", "fieldanimspeed=6", "fieldanimspeed=-1",
			"fieldtile=255 fieldframes=2", "fieldtile=253 fieldframes=4" }) {
			auto rom{ vanilla_rom() }; install_scheduler(rom); const auto before{ rom };
			bool threw{ false };
			try { install_status_ward(rom, "fieldfx=true " + params); }
			catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "invalid visual parameters were accepted or mutated ROM: " + params);
		}
		const std::string spec{ "AtlasDevStatusWard trigger=always fieldfx=true edgepark=true "
			"fieldpattern=contract fieldframes=4 fieldanimspeed=1 fieldblink=24 fieldcount=4 fieldspeed=1" };
		auto rom{ vanilla_rom() }; install_scheduler(rom);
		constexpr word org{ 0xfdbe };
		const auto before{ rom };
		const auto size{ fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15,
			org, 0xfff0, hacks(spec), nullptr) };
		auto exact{ before }, short_one{ before };
		fh::HackManager{}.install_general_hacks(fe::Config{}, exact, 15, org, org + size, hacks(spec), nullptr);
		bool threw{ false };
		try { fh::HackManager{}.install_general_hacks(fe::Config{}, short_one, 15, org, org + size - 1, hacks(spec), nullptr); }
		catch (const std::exception&) { threw = true; }
		require(exact == rom && threw && short_one == before, "visual bank capacity was not exact and transactional");
	}

	void test_status_ward_plain_bodies_share_one_existing_field_kind() {
		auto rom{ vanilla_rom() };
		install_scheduler(rom);
		install_status_ward(rom, "trigger=ointment fieldfx=true");
		const auto scheduler{ scheduler_file_offset(rom) };
		std::array<word, 3> pre_before{};
		const std::array<std::size_t, 3> pre_sites{
			fh::afs::OFF_PRE0, fh::afs::OFF_PRE1, fh::afs::OFF_PRE2
		};
		for (std::size_t i{ 0 }; i < pre_sites.size(); ++i)
			pre_before[i] = read_word(rom, scheduler + pre_sites[i]);
		const word previous{ read_word(rom, scheduler + fh::afs::OFF_POST) };
		const auto before{ rom };
		install_status_ward(rom, "trigger=wingboots");
		const auto [first, last]{ changed_bank9_span(before, rom) };
		const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
		require(read_word(rom, scheduler + fh::afs::OFF_POST)
			== static_cast<word>(0x8000 + first)
			&& rom[bank9 + last - 2] == 0x4c
			&& read_word(rom, bank9 + last - 1) == previous,
			"second StatusWard trigger did not join the POST chain");
		for (std::size_t i{ 0 }; i < pre_sites.size(); ++i)
			require(read_word(rom, scheduler + pre_sites[i]) == pre_before[i],
				"plain StatusWard trigger displaced the existing field owner");
		const auto ward_count{ static_cast<std::size_t>(
			(rom[scheduler + fh::afs::OFF_ARM0] == 5)
			+ (rom[scheduler + fh::afs::OFF_ARM0 + 1] == 5)
			+ (rom[scheduler + fh::afs::OFF_ARM0 + 2] == 5)) };
		require(ward_count == 1,
			"multiple StatusWard triggers duplicated kind 5 in the arm table");
	}

	template<typename Mutator>
	void require_ward_refusal(const std::string& expected, Mutator mutate,
		const std::string& params = {}) {
		auto rom{ vanilla_rom() }; install_scheduler(rom);
		mutate(rom, scheduler_file_offset(rom));
		const auto before{ rom };
		std::string error;
		try { install_status_ward(rom, params); }
		catch (const std::runtime_error& e) { error = e.what(); }
		require(!error.empty() && error.find(expected) != std::string::npos,
			"StatusWard expected refusal " + expected + ", got: " + error);
		require(rom == before, "StatusWard refusal mutated caller ROM");
	}

	void test_status_ward_malformed_ownership_and_bank_capacity() {
		for (const byte marker : { byte{0}, byte{1}, byte{2} })
			require_ward_refusal("POST", [marker](auto& rom, auto off) {
				rom[off + fh::afs::OFF_POST] = 0;
				rom[off + fh::afs::OFF_POST + 1] = 0x90;
				rom[off + fh::afs::OFF_POSTARMED] = marker;
			});
		require_ward_refusal("multiple slots", [](auto& rom, auto off) {
			rom[off + fh::afs::OFF_ARM0] = 5;
			rom[off + fh::afs::OFF_ARM0 + 1] = 5;
		});
		require_ward_refusal("PRE claimant", [](auto& rom, auto off) {
			rom[off + fh::afs::OFF_ARM0] = 5;
			rom[off + fh::afs::OFF_PRE0] = 0;
			rom[off + fh::afs::OFF_PRE0 + 1] = 0x90;
		});
		auto measured{ vanilla_rom() }; install_scheduler(measured);
		const auto before{ measured }; install_status_ward(measured);
		const auto [first, last]{ changed_bank9_span(before, measured) };
		const auto size{ last - first + 1 };
		const auto bank9{ klib::Asm6502::get_file_offset(9, 0x8000) };
		auto exact{ before };
		std::fill_n(exact.begin() + bank9, BANK9_SIZE - size, byte{0});
		install_status_ward(exact);
		require(read_word(exact, scheduler_file_offset(exact) + fh::afs::OFF_POST)
			== 0xc000 - size, "StatusWard missed exact-fit bank-9 window");
		require_ward_refusal("no free bank 9 window", [=](auto& rom, auto) {
			std::fill_n(rom.begin() + bank9, BANK9_SIZE - size + 1, byte{0});
		});
		require_ward_refusal("bank 15 space", [](auto& rom, auto) {
			install_status_ward(rom, "fieldfx=true");
		}, "fieldfx=true");
	}

	void test_status_ward_force_configuration_boundaries() {
		for (const auto trigger : { "ointment", "glove", "wingboots", "hourglass", "any", "always" })
		for (const auto arc : { "both", "front", "back" })
		for (const int push : { 1, 4 })
		for (const bool pull : { false, true }) {
			auto rom{ vanilla_rom() }; install_scheduler(rom);
			const std::string params{ std::string("trigger=") + trigger + " arc=" + arc
				+ " push=" + std::to_string(push) + " pull=" + (pull ? "true" : "false")
				+ " radius=120 fieldpulse=255" };
			try { install_status_ward(rom, params); }
			catch (const std::exception& e) {
				throw std::runtime_error(params + ": " + e.what());
			}
		}
	}

	void write_status_ward_parity_pair(const std::string& base_path,
		const std::string& native_path, const std::string& custom_params = {}) {
		const auto write_rom = [](const std::string& path,
			const std::vector<byte>& rom) {
			std::ofstream out(path, std::ios::binary);
			if (!out)
				throw std::runtime_error("could not open parity output: " + path);
			out.write(reinterpret_cast<const char*>(rom.data()),
				static_cast<std::streamsize>(rom.size()));
			if (!out)
				throw std::runtime_error("could not write parity output: " + path);
		};
		auto base{ vanilla_rom() };
		write_rom(base_path, base);

		// The project reference core ends at $FDBE. Put the smaller upstream
		// core immediately before that boundary so both emitters place the
		// independent bank-9 role at $8000 and the field graphic at $FDBE.
		constexpr word parity_scheduler{ 0xfdbe - fh::afs::CORE_SIZE };
		auto native{ base };
		fh::HackManager{}.install_general_hacks(fe::Config{}, native, 15,
			parity_scheduler, 0xfff0, hacks("AtlasDevFrameScheduler"), nullptr);
		const std::string params{ custom_params.empty() ?
			"AtlasDevStatusWard radius=48 push=1 exempt=34 trigger=wingboots "
			"pull=false arc=front fieldpulse=24 aura=true fieldfx=true "
			"fieldtile=64 fieldattr=0 fieldspeed=4 fieldcount=4 fielddir=ccw "
			"edgepark=true" : custom_params };
		fh::HackManager{}.install_general_hacks(fe::Config{}, native, 15,
			0xfdbe, 0xfff0, hacks(params), nullptr);
		write_rom(native_path, native);
	}
}

int main(int argc, char** argv) {
	try {
		if ((argc == 4 || argc == 5) && std::string(argv[1]) == "--write-jumpcontrol-parity") {
			write_jump_control_parity(argv[2], argv[3], argc == 5 ? argv[4] : "");
			return 0;
		}
		if ((argc == 4 || argc == 5) && std::string(argv[1]) == "--write-statusward-parity") {
			write_status_ward_parity_pair(argv[2], argv[3], argc == 5 ? argv[4] : "");
			return 0;
		}
		if (argc != 1)
			throw std::runtime_error("usage: atlas_scheduler_regression [--write-jumpcontrol-parity BASE OUT [PARAMS]] [--write-statusward-parity BASE NATIVE [HACK_SPEC]]");
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
		test_scheduler_skips_gate_only_kinds();
		test_status_ward_malformed_ownership_and_bank_capacity();
		test_status_ward_force_configuration_boundaries();
		test_status_ward_install_survives_transactional_copy();
		test_status_ward_is_a_real_dormant_fourth_role();
		test_status_ward_field_claims_pre_lanes_and_edgepark_is_opt_in();
		test_status_ward_refuses_a_pre_claim_atomically();
		test_status_ward_edgepark_clips_both_axes();
		test_status_ward_unclipped_emission_stays_legacy();
		test_status_ward_visual_defaults_preserve_both_legacy_paths();
		test_status_ward_visual_pattern_coordinates();
		test_status_ward_visual_blink_and_animated_tiles();
		test_status_ward_visual_parameters_and_capacity();
		test_status_ward_plain_bodies_share_one_existing_field_kind();
		std::cout << "atlas scheduler regressions: ok\n";
		return 0;
	}
	catch (const std::exception& e) {
		std::cerr << "atlas scheduler regressions: " << e.what() << '\n';
		return 1;
	}
}
