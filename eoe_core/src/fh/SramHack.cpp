#include "HackManager.h"
#include "fh_constants.h"
#include "common/klib/Asm6502.h"
#include "common/klib/Kstring.h"
#include <algorithm>
#include <array>
#include <stdexcept>

using byte = unsigned char;

namespace {
	constexpr char ID_SRAM_START_SCREEN_ATTRS[]{ "sram_start_screen_attrs" };

	// constexpr word ShowMantraInputScreen{ 0x909d }; // at least 328 bytes free on US - might not be needed

	// savefile header, used for validation along with a 16-bit additive checksum
	constexpr std::array<byte, 4> HEADER{ 'E', 'o', 'E', 0x00 };
	// SRAM zeropage scratch variables
	constexpr byte SRAM_VALID{ fh::RAM::ZP_e5 };

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

	word install_SRAM_Ranges_table(std::vector<byte>& p_rom, word cpu_addr, const fh::GeneralHack& p_hack) {
		auto ranges{ p_hack.split_word_byte("ranges", "$039d:42+$042c:2+$0437:1+$0439:1+$04c2:4+$0101:31+$0390:5") };
		if (ranges.empty() || ranges.size() > 0xff)
			throw std::runtime_error("Invalid SRAM range count");
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

	// calculates additive 16-bit checksum across the saved ranges
	word install_SRAM_CalcCheckSum(std::vector<byte>& p_rom, word cpu_addr,
		word p_range_table_addr) {
		klib::Asm6502 code;

		// table pointer
		code.lda_imm(static_cast<byte>(p_range_table_addr % 256));
		code.sta_zp(TABLE_PTR_LO);
		code.lda_imm(static_cast<byte>(p_range_table_addr / 256));
		code.sta_zp(TABLE_PTR_HI);

		// clear checksum
		code.lda_imm(0x00);
		code.sta_zp(CHECKSUM_LO);
		code.sta_zp(CHECKSUM_HI);

		// get range count
		code.ldy_imm(0x00);
		code.lda_ind_y(TABLE_PTR_LO);
		code.sta_zp(RANGES_LEFT);

		code.label("@checksum_ranges_loop");

		// SRAM pointer = RAM address + $6000
		code.iny();
		code.lda_ind_y(TABLE_PTR_LO);
		code.sta_zp(SOURCE_PTR_LO);

		code.iny();
		code.lda_ind_y(TABLE_PTR_LO);
		code.clc();
		code.adc_imm(0x60);
		code.sta_zp(SOURCE_PTR_HI);

		// byte count
		code.iny();
		code.lda_ind_y(TABLE_PTR_LO);
		code.tax();

		// preserve table position
		code.tya();
		code.pha();

		code.ldy_imm(0x00);
		code.label("@checksum_range_loop");

		code.lda_zp(CHECKSUM_LO);
		code.clc();
		code.adc_ind_y(SOURCE_PTR_LO);
		code.sta_zp(CHECKSUM_LO);

		code.lda_zp(CHECKSUM_HI);
		code.adc_imm(0x00);
		code.sta_zp(CHECKSUM_HI);

		code.iny();
		code.dex();
		code.bne("@checksum_range_loop");

		// restore table position
		code.pla();
		code.tay();

		code.dec_zp(RANGES_LEFT);
		code.bne("@checksum_ranges_loop");
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

	word install_SRAM_Save(std::vector<byte>& p_rom, word cpu_addr,
		word p_copy_ranges_addr, word p_calc_checksum_addr) {
		klib::Asm6502 code;

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

	void install_SRAM_ShowMantra(const fe::Config& p_config, std::vector<byte>& p_rom, word p_save_addr) {
		klib::Asm6502 code;
		code.jsr(p_save_addr);
		code.jmp(fh::HackManager::cfg_word(p_config, fh::c::ID_ROM_ISCRIPTS_INVOKENEXTACTION));
		code.apply_hack_and_clear(p_rom, 12, fh::HackManager::cfg_word(p_config, fh::c::ID_ISCRIPTACTIONSHOWMANTRA));
	}

	void install_SRAM_ChooseContinue(const fe::Config& p_config, std::vector<byte>& p_rom, word p_load_addr) {
		klib::Asm6502 code;

		// update the jump target of the routine called when player chooses CONTINUE
		code.dw(p_load_addr - 1);
		code.apply_hack_and_clear(p_rom, 15, fh::HackManager::cfg_word(p_config, fh::c::ID_CHOOSECONTINUE_TARGETADDR));
	}

	word install_SRAM_ValidateOnStartup(const fe::Config& p_config, std::vector<byte>& p_rom, word cpu_addr,
		word p_validate_addr) {
		klib::Asm6502 code;

		// install hook
		code.jsr(cpu_addr);
		code.apply_hack_and_clear(p_rom, 12, fh::HackManager::cfg_word(p_config, fh::c::ID_STARTSCREEN_DRAW));

		// new routine - returns 1 in SRAM_VALID if a valid savefile exists, otherwise 0
		code.jsr(p_validate_addr);
		code.beq("@valid");
		code.lda_imm(0x00);
		code.beq("@return");

		code.label("@valid");
		code.lda_imm(0x01);

		code.label("@return");
		code.sta_zp(SRAM_VALID);
		code.jmp(fh::ROM::PPU_WaitUntilFlushed);
		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	word install_SRAM_DisableContinueOnInvalidSave(const fe::Config& p_config, std::vector<byte>& p_rom,
		word cpu_addr) {
		klib::Asm6502 code;

		// hook - verify sram before going through with selection
		code.jsr(cpu_addr);
		code.nop(10);
		code.apply_hack_and_clear(p_rom, 12, fh::HackManager::cfg_word(p_config, fh::c::ID_STARTSCREEN_INPUTSELECT));

		// set default selection to CONTINUE if a valid save exists
		code.lda_zp(SRAM_VALID);
		code.apply_hack_and_clear(p_rom, 12, fh::HackManager::cfg_word(p_config, fh::c::ID_STARTSCREEN_DEFAULTSELECTION));

		// new routine - allow selection switch if sram save is valid
		code.lda_mem(SRAM_VALID);
		code.bne("@valid");
		code.lda_imm(0x0d);
		code.jsr(fh::ROM::Sound_PlayEffect);
		code.rts();

		code.label("@valid");
		code.lda_abs(fh::RAM::StartScreenSelection);
		code.eor_imm(0x01);
		code.sta_abs(fh::RAM::StartScreenSelection);
		code.lda_imm(0x0b);
		code.jsr(fh::ROM::Sound_PlayEffect);
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	word install_SRAM_WriteStartScreenAttributes(const fe::Config& p_config,
		std::vector<byte>& p_rom, word cpu_addr) {
		const auto attrs{ p_config.string(ID_SRAM_START_SCREEN_ATTRS) };

		std::map<std::string, std::string> attr_map;
		for (const auto& kv_str : klib::str::split_string(attrs, ',')) {
			const auto kv{ klib::str::split_string(kv_str, '=') };
			if (kv.size() != 2)
				throw std::runtime_error(std::format("Invalid key-value pair: {}", kv_str));
			attr_map.insert(std::make_pair(klib::str::trim(kv[0]), klib::str::trim(kv[1])));
		}

		const word ppu_start{ static_cast<word>(klib::str::parse_numeric(attr_map.at("addr"))) };
		const std::vector<byte> attrs_valid{ klib::str::parse_byte_list(attr_map.at("valid"),'+') };
		const std::vector<byte> attrs_invalid{ klib::str::parse_byte_list(attr_map.at("invalid"),'+') };

		klib::Asm6502 code;

		// hook
		code.jsr(cpu_addr);
		code.apply_hack_and_clear(p_rom, 12, fh::HackManager::cfg_word(p_config, fh::c::ID_STARTSCREEN_DRAW_JSR_PPU_WRITETILESFROMCHRRAM));

		// new routine
		code.jsr(fh::HackManager::cfg_word(p_config, fh::c::ID_PPU_WRITETILESFROMCHRRAM)); // overwritten at call site

		code.lda_imm(ppu_start / 256);
		code.sta_abs(fh::PPU::PPU_ADDR);
		code.lda_imm(ppu_start % 256);
		code.sta_abs(fh::PPU::PPU_ADDR);

		code.lda_mem(SRAM_VALID);
		code.bne("@valid");
		for (byte b : attrs_invalid) {
			code.lda_imm(b);
			code.sta_abs(fh::PPU::PPU_DATA);
		}
		code.rts();

		code.label("@valid");
		for (byte b : attrs_valid) {
			code.lda_imm(b);
			code.sta_abs(fh::PPU::PPU_DATA);
		}
		code.rts();

		return code.apply_hack_and_clear_get_next_cpu_addr(p_rom, 12, cpu_addr);
	}

	// overwrite six bytes for far call that initializes gold and xp from title
	// with the only invariant we need to preserve (perfect byte fit!)
	void install_static_NoGoldXPReload(std::vector<byte>& p_rom, word cpu_addr) {
		klib::Asm6502 code;
		code.lda_abs(fh::RAM::PlayerTitle);
		code.sta_abs(fh::RAM::PendingTitle);
		code.apply_hack_and_clear(p_rom, 15, cpu_addr);
	}
}

// installs SRAM save support
void fh::HackManager::install_SRAM(const fe::Config& p_config, std::vector<byte>& p_rom,
	const fh::GeneralHack& p_hack) const {

	const bool keep_gold_xp_on_sram_load{ p_hack.bool_or("save_gold", true) };
	const bool keep_gold_xp_on_death{ p_hack.bool_or("keep_gold", false) };
	const bool color_text{ p_hack.bool_or("color", true) };

	if (keep_gold_xp_on_sram_load)
		install_static_NoGoldXPReload(p_rom, cfg_word(p_config, c::ID_CONTINUE_INIT_XP_GOLD));
	if (keep_gold_xp_on_death)
		install_static_NoGoldXPReload(p_rom, ROM::Player_HandleDeath_InitGoldXP);

	// dynamic block
	word cpu_addr{ cfg_word(p_config, c::ID_MANTRALOAD) };

	const word table_addr{ cpu_addr };
	cpu_addr = install_SRAM_Ranges_table(p_rom, cpu_addr, p_hack);

	const word copy_range_addr{ cpu_addr };
	cpu_addr = install_SRAM_CopyRange(p_rom, cpu_addr);

	const word copy_ranges_addr{ cpu_addr };
	cpu_addr = install_SRAM_CopyRanges(p_rom, cpu_addr, copy_range_addr, table_addr);

	const word calc_checksum_addr{ cpu_addr };
	cpu_addr = install_SRAM_CalcCheckSum(p_rom, cpu_addr, table_addr);

	const word validate_addr{ cpu_addr };
	cpu_addr = install_SRAM_Validate(p_rom, cpu_addr, calc_checksum_addr);

	const word save_addr{ cpu_addr };
	cpu_addr = install_SRAM_Save(p_rom, cpu_addr, copy_ranges_addr, calc_checksum_addr);

	const word load_addr{ cpu_addr };
	cpu_addr = install_SRAM_Load(p_rom, cpu_addr, copy_ranges_addr);

	const word startup_validate_addr{ cpu_addr };
	cpu_addr = install_SRAM_ValidateOnStartup(p_config, p_rom, startup_validate_addr, validate_addr);

	const word start_screen_select_guard_addr{ cpu_addr };
	cpu_addr = install_SRAM_DisableContinueOnInvalidSave(p_config, p_rom, start_screen_select_guard_addr);

	if (color_text) {
		const word start_screen_attribute_addr{ cpu_addr };
		cpu_addr = install_SRAM_WriteStartScreenAttributes(p_config, p_rom, start_screen_attribute_addr);
	}

	install_SRAM_ShowMantra(p_config, p_rom, save_addr);
	install_SRAM_ChooseContinue(p_config, p_rom, load_addr);

	// update SRAM-bit in the ROM header
	p_rom.at(6) |= 0x02;
}
