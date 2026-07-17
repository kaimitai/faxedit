#ifndef KLIB_ASM6502_H
#define KLIB_ASM6502_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using byte = std::uint8_t;
using sbyte = std::int8_t;
using word = std::uint16_t;

namespace klib {

	class Asm6502 {

		struct BranchRef {
			std::size_t offset;
			std::string label;
		};

		std::vector<byte> m_bytes;
		std::unordered_map<std::string, std::size_t> m_labels;
		std::vector<BranchRef> m_branch_refs;

		void emit(byte p_byte);
		void emit(sbyte p_byte);
		void emit_word(word p_word);
		void resolve_labels(void);
		void branch(byte p_opcode, const std::string& p_label);

		void apply_hack(std::vector<byte>& p_rom, byte p_bank_no,
			word p_cpu_addr, word p_cpu_min_addr = 0xc000) const;

	public:
		Asm6502(void) = default;
		const std::vector<byte>& bytes(void) const;
		std::size_t size(void) const;
		std::size_t get_file_offset(byte p_bank_no, word p_cpu_addr, word p_cpu_min_addr) const;
		std::size_t get_file_offset(byte p_bank_no, word p_cpu_addr) const;
		void label(const std::string& p_name);

		void clear(void);
		std::size_t apply_hack_and_clear(std::vector<byte>& p_rom, byte p_bank_no,
			word p_cpu_addr, word p_cpu_min_addr);
		std::size_t apply_hack_and_clear(std::vector<byte>& p_rom, byte p_bank_no,
			word p_cpu_addr);

		// jumps and calls
		void jmp(word p_addr);
		void jmp_ind(word p_addr);
		void jsr(word p_addr);
		void rts(void);

		// loads
		void lda_zp(byte p_addr);
		void lda_imm(byte p_value);
		void lda_abs(word p_addr);
		void lda_abs_x(word p_addr);
		void lda_abs_y(word p_addr);
		void lda_ind_y(byte p_addr);
		void ldx_imm(byte p_value);
		void ldx_abs_y(word p_addr);
		void ldy_imm(byte p_value);
		void ldy_zp(byte p_value);

		// stores
		void sta_zp(byte p_addr);
		void sta_abs(word p_addr);
		void sta_abs_x(word p_addr);
		void sty_zp(byte p_addr);

		// compares
		void cmp_zp(byte p_addr);
		void cmp_imm(byte p_value);
		void cmp_abs(word p_addr);
		void cmp_abs_x(word p_addr);
		void cmp_abs_y(word p_addr);
		void cpx_imm(byte p_value);
		void cpy_imm(byte p_value);
		void cpy_abs(word p_addr);

		// branches
		void beq(sbyte p_offset);
		void bne(sbyte p_offset);
		void bcc(sbyte p_offset);
		void bpl(sbyte p_offset);
		void bmi(sbyte p_offset);
		void beq(const std::string& p_label);
		void bne(const std::string& p_label);
		void bcc(const std::string& p_label);
		void bpl(const std::string& p_label);
		void bmi(const std::string& p_label);

		// logic
		void and_imm(byte p_value);
		void and_zp(byte p_addr);
		void and_abs_y(word p_addr);
		void ora_imm(byte p_value);
		void ora_abs_y(word p_addr);
		void eor_imm(byte p_value);

		// stack
		void pha(void);
		void pla(void);

		// registers
		void tax(void);
		void tay(void);
		void tya(void);
		void txa(void);

		//shifts
		void lsr_a(std::size_t count = 1);
		void asl_a(void);

		// math
		void inx(void);
		void dec_zp(byte p_addr);
		void dex(void);
		void iny(void);
		void sec(void);
		void clc(void);

		// misc
		void nop(std::size_t count = 1);
		void db(byte p_value);
		void dw(word p_word);
	};

}

#endif
