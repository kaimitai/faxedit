#include "HackManager.h"
#include "fh_constants.h"
#include "common/klib/Asm6502.h"
#include <algorithm>
#include <array>
#include <stdexcept>

using byte = unsigned char;

namespace {

	// savefile header, used for validation along with a 16-bit additive checksum
	constexpr std::array<byte, 4> HEADER{ 'E', 'o', 'E', 0x00 };
	// SRAM zeropage scratch variables
	constexpr byte CHECKSUM_LO{ fh::RAM::ZP_2c };
	constexpr byte CHECKSUM_HI{ fh::RAM::ZP_2d };

	constexpr byte RANGES_LEFT{ fh::RAM::ZP_30 };
	constexpr byte SOURCE_HI_OFFSET{ fh::RAM::ZP_31 };
	constexpr byte TARGET_HI_OFFSET{ fh::RAM::ZP_32 };

	constexpr byte SOURCE_PTR_LO{ fh::RAM::ZP_e2 };
	constexpr byte SOURCE_PTR_HI{ fh::RAM::ZP_e3 };
	constexpr byte TARGET_PTR_LO{ fh::RAM::ZP_e4 };
	constexpr byte TARGET_PTR_HI{ fh::RAM::ZP_e5 };
	constexpr byte TABLE_PTR_LO{ fh::RAM::ZP_e6 };
	constexpr byte TABLE_PTR_HI{ fh::RAM::ZP_e7 };

	// default; $039d:42+$042c:2+$0437:1+$0439:1+$04c2:4
	// RAM and XP: $0390:5
	// extended flags: $0101:31
	word install_SRAM_Ranges_table(std::vector<byte>& p_rom, word cpu_addr, const fh::GeneralHack& p_hack) {
		auto ranges{ p_hack.split_word_byte("ranges") };
		if (ranges.empty())
			throw std::runtime_error("No memory ranges defined for SRAM");
		std::sort(begin(ranges), end(ranges));

		klib::Asm6502 code;

		// data table; range count followed by <address, byte count> pairs
		code.db(static_cast<byte>(ranges.size()));
		for (const auto& range : ranges) {
			code.dw(range.first);
			code.db(range.second);
		}
		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	// source addr base in SOURCE_PTR_LO/SOURCE_PTR_HI
	// target addr base in TARGET_PTR_LO/TARGET_PTR_HI
	// byte count is in X
	word install_SRAM_CopyRange(std::vector<byte>& p_rom, word cpu_addr) {
		klib::Asm6502 code;

		code.ldy_imm(0x00);
		code.label("@copy_range_loop");
		code.lda_ind_y(SOURCE_PTR_LO);
		code.sta_ind_y(TARGET_PTR_LO);
		code.iny();
		code.dex();
		code.bne("@copy_range_loop");
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	word install_SRAM_CopyRanges(std::vector<byte>& p_rom, word cpu_addr, word p_copy_range_addr,
		word p_range_table_addr) {
		klib::Asm6502 code;

		// table pointer
		code.lda_imm(static_cast<byte>(p_range_table_addr % 256));
		code.sta_zp(TABLE_PTR_LO);
		code.lda_imm(static_cast<byte>(p_range_table_addr / 256));
		code.sta_zp(TABLE_PTR_HI);

		code.ldy_imm(0x00);
		code.lda_ind_y(TABLE_PTR_LO);
		code.sta_zp(RANGES_LEFT);

		code.label("@copy_ranges_loop");

		// lo byte is identical for RAM and SRAM
		code.iny();
		code.lda_ind_y(TABLE_PTR_LO);
		code.sta_zp(SOURCE_PTR_LO);
		code.sta_zp(TARGET_PTR_LO);

		// source/target hi bytes
		code.iny();
		code.lda_ind_y(TABLE_PTR_LO);
		code.clc();
		code.adc_zp(SOURCE_HI_OFFSET);
		code.sta_zp(SOURCE_PTR_HI);

		code.lda_ind_y(TABLE_PTR_LO);
		code.clc();
		code.adc_zp(TARGET_HI_OFFSET);
		code.sta_zp(TARGET_PTR_HI);

		// length
		code.iny();
		code.lda_ind_y(TABLE_PTR_LO);
		code.tax();

		// preserve table Y
		code.tya();
		code.pha();
		code.jsr(p_copy_range_addr);
		code.pla();
		code.tay();

		code.dec_zp(RANGES_LEFT);
		code.bne("@copy_ranges_loop");
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	// clear SRAM WRAM mirror: $6000-$67ff
	word install_SRAM_Init(std::vector<byte>& p_rom, word cpu_addr) {
		klib::Asm6502 code;

		code.lda_imm(0x00);
		code.tax();
		code.label("@init_sram_loop");
		for (word addr{ 0x6000 }; addr < 0x6800; addr += 0x100)
			code.sta_abs_x(addr);
		code.inx();
		code.bne("@init_sram_loop");
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	// calculate 16-bit additive checksum of $6000-$67ff
	// result in CHECKSUM_LO:CHECKSUM_HI
	word install_SRAM_CalcCheckSum(std::vector<byte>& p_rom, word cpu_addr) {
		klib::Asm6502 code;

		code.lda_imm(0x00);
		code.sta_zp(CHECKSUM_LO);
		code.sta_zp(CHECKSUM_HI);
		code.sta_zp(SOURCE_PTR_LO);

		code.lda_imm(0x60);
		code.sta_zp(SOURCE_PTR_HI);

		code.ldy_imm(0x00);

		code.label("@checksum_sram_loop");

		code.lda_zp(CHECKSUM_LO);
		code.clc();
		code.adc_ind_y(SOURCE_PTR_LO);
		code.sta_zp(CHECKSUM_LO);

		code.lda_zp(CHECKSUM_HI);
		code.adc_imm(0x00);
		code.sta_zp(CHECKSUM_HI);

		code.iny();
		code.bne("@checksum_sram_loop");

		code.inc_zp(SOURCE_PTR_HI);
		code.lda_zp(SOURCE_PTR_HI);
		code.cmp_imm(0x68);
		code.bne("@checksum_sram_loop");

		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	word install_SRAM_Validate(std::vector<byte>& p_rom, word cpu_addr, word p_calc_checksum_addr) {
		klib::Asm6502 code;

		// check header
		code.lda_abs(0x6800);
		code.cmp_imm(HEADER[0]);
		code.bne("@invalid");

		code.lda_abs(0x6801);
		code.cmp_imm(HEADER[1]);
		code.bne("@invalid");

		code.lda_abs(0x6802);
		code.cmp_imm(HEADER[2]);
		code.bne("@invalid");

		code.lda_abs(0x6803);
		// code.cmp_imm(HEADER[3]); // can omit when version no is 0
		code.bne("@invalid");

		// calculate and compare checksum
		code.jsr(p_calc_checksum_addr);

		code.lda_zp(CHECKSUM_LO);
		code.cmp_abs(0x6804);
		code.bne("@invalid");

		code.lda_zp(CHECKSUM_HI);
		code.cmp_abs(0x6805);
		code.rts(); // Z reflects final comparison

		code.label("@invalid");
		code.lda_imm(0x01); // Z clear
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	word install_SRAM_Save(std::vector<byte>& p_rom, word cpu_addr, word p_init_addr,
		word p_copy_ranges_addr, word p_calc_checksum_addr) {
		klib::Asm6502 code;

		code.jsr(p_init_addr);
		code.jsr("@save_ranges");
		code.jsr(p_calc_checksum_addr);

		// store checksum
		code.lda_zp(CHECKSUM_LO);
		code.sta_abs(0x6804);
		code.lda_zp(CHECKSUM_HI);
		code.sta_abs(0x6805);

		// write header last
		code.lda_imm(HEADER[0]);
		code.sta_abs(0x6800);
		code.lda_imm(HEADER[1]);
		code.sta_abs(0x6801);
		code.lda_imm(HEADER[2]);
		code.sta_abs(0x6802);
		code.lda_imm(HEADER[3]);
		code.sta_abs(0x6803);
		code.rts();

		// "internal" helper
		// save: RAM -> SRAM ranges
		code.label("@save_ranges");
		code.lda_imm(0x00);
		code.sta_zp(SOURCE_HI_OFFSET);
		code.lda_imm(0x60);
		code.sta_zp(TARGET_HI_OFFSET);
		code.jmp(p_copy_ranges_addr);

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	word install_SRAM_Load(std::vector<byte>& p_rom, word cpu_addr, word p_copy_ranges_addr) {
		klib::Asm6502 code;

		// load: SRAM -> RAM
		code.label("@load_ranges");
		code.lda_imm(0x60);
		code.sta_zp(SOURCE_HI_OFFSET);
		code.lda_imm(0x00);
		code.sta_zp(TARGET_HI_OFFSET);
		code.jmp(p_copy_ranges_addr);

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}
}

// installs SRAM save support
void fh::HackManager::install_SRAM(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fh::GeneralHack& p_hack) const {
	word cpu_addr{ 0x909d };

	const word table_addr{ cpu_addr };
	cpu_addr = install_SRAM_Ranges_table(p_rom, cpu_addr, p_hack);

	const word copy_range_addr{ cpu_addr };
	cpu_addr = install_SRAM_CopyRange(p_rom, cpu_addr);

	const word copy_ranges_addr{ cpu_addr };
	cpu_addr = install_SRAM_CopyRanges(
		p_rom, cpu_addr, copy_range_addr, table_addr);

	const word init_addr{ cpu_addr };
	cpu_addr = install_SRAM_Init(p_rom, cpu_addr);

	const word calc_checksum_addr{ cpu_addr };
	cpu_addr = install_SRAM_CalcCheckSum(p_rom, cpu_addr);

	// only the following three functions will be called from the outside
	const word validate_addr{ cpu_addr };
	cpu_addr = install_SRAM_Validate(p_rom, cpu_addr, calc_checksum_addr);

	const word save_addr{ cpu_addr };
	cpu_addr = install_SRAM_Save(p_rom, cpu_addr, init_addr, copy_ranges_addr, calc_checksum_addr);

	const word load_addr{ cpu_addr };
	cpu_addr = install_SRAM_Load(p_rom, cpu_addr, copy_ranges_addr);
}
