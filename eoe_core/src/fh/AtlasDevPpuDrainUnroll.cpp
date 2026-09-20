#include "HackManager.h"
#include "common/klib/Asm6502.h"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <string_view>

// copy full tiles with straight loads and stores. short records and records
// crossing the queue boundary keep the byte loop.
namespace {
	using Asm = klib::Asm6502;
	void require_site(const std::vector<byte>& rom, word addr, std::string_view hex) {
		const auto off{ Asm::get_file_offset(15, addr) };
		const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
		for (std::size_t i{ 0 }; i < hex.size() / 2; ++i)
			if (rom.at(off + i) != nibble(hex[2 * i]) * 16 + nibble(hex[2 * i + 1]))
				throw std::runtime_error(std::format(
					"AtlasDevPpuDrainUnroll: incompatible queue code at ${:04x}", addr + i));
	}
	Asm drain_code() {
		Asm code;
		code.label("block"); code.cpy_imm(16); code.bcc("remainder");
		code.cpx_imm(0xf1); code.bcs("remainder");
		for (word k{ 0 }; k < 16; ++k) {
			code.lda_abs_x(0x0500 + k); code.sta_abs(0x2007);
		}
		code.txa(); code.clc(); code.adc_imm(16); code.tax();
		code.tya(); code.sec(); code.sbc_imm(16); code.tay();
		code.bne("block"); code.beq("done");
		code.label("remainder"); code.lda_abs_x(0x0500); code.inx();
		code.sta_abs(0x2007); code.dey(); code.bne("remainder");
		code.label("done"); code.jmp(0xcfa1);
		return code;
	}
}

word fh::HackManager::install_AtlasDevPpuDrainUnroll(const fe::Config&, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack) const {
	const byte budget{ hack.byte_or("budget", 48) };
	if (budget < 1 || budget > 64)
		throw std::runtime_error("AtlasDevPpuDrainUnroll: budget must be 1..64");
	if (rom.size() != 0x40010 || rom[0] != 'N' || rom[1] != 'E'
		|| rom[2] != 'S' || rom[3] != 0x1a || rom[4] != 16 || rom[5] != 0
		|| (rom[6] & 0xfc) != 0x10 || (rom[7] & 0xf0) != 0
		|| ((rom[7] & 0x0c) != 0 && (rom[7] & 0x0c) != 8)
		|| ((rom[7] & 0x0c) == 8 && (rom[8] != 0 || rom[9] != 0)))
		throw std::runtime_error("AtlasDevPpuDrainUnroll: requires unexpanded MMC1 PRG with CHR RAM and no trainer");
	require_site(rom, 0xcf5b, "a9d08522a9068521");
	require_site(rom, 0xcf63,
		"a61fe420f04aa50a29fba8bd00051006297fc8c8c8c88c0020bd0005297f"
		"a8e8bd00058d0620e8bd00058d0620e8981865228522");
	require_site(rom, 0xcf97, "bd0005e88d072088d0f6");
	// the continuation saves X and replaces the flags, Y and A before use.
	require_site(rom, 0xcfa1, "861fc621f00cbc000588c0f9b004a52230b0a9008d06208d062060");
	const std::size_t origin{ (std::size_t{ cpu_addr } + 255) & ~std::size_t{ 255 } };
	auto code{ drain_code() };
	const auto end{ origin + code.size() };
	if (code.size() != 131 || end > 0xfffa)
		throw std::runtime_error("AtlasDevPpuDrainUnroll: aligned helper reaches interrupt vectors");
	const auto off{ Asm::get_file_offset(15, cpu_addr) };
	// count the alignment gap as allocated space too. keeping the helper on
	// one page prevents a taken branch from adding a page-cross cycle.
	if (!std::all_of(rom.begin() + off, rom.begin() + off + end - cpu_addr,
		[](byte value) { return value == 0xff; }))
		throw std::runtime_error("AtlasDevPpuDrainUnroll: allocated helper range is occupied");
	code.apply_hack_noclear(rom, 15, static_cast<word>(origin));
	Asm hook; hook.jmp(static_cast<word>(origin));
	while (hook.size() < 10) hook.nop();
	hook.apply_hack_noclear(rom, 15, 0xcf97);
	rom[Asm::get_file_offset(15, 0xcf5c)] = static_cast<byte>(0 - budget);
	return static_cast<word>(end);
}
