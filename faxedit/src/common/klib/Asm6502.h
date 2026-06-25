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
		void label(const std::string& p_name);

		void clear(void);
		void apply_hack_and_clear(std::vector<byte>& p_rom, byte p_bank_no,
			word p_cpu_addr, word p_cpu_min_addr = 0xc000);

		// jumps and calls
		void jmp(word p_addr);
		void jmp_ind(word p_addr);
		void jsr(word p_addr);
		void rts(void);

		// loads
		void lda_imm(byte p_value);
		void lda_abs(word p_addr);

		// stores
		void sta_abs(word p_addr);

		// compares
		void cmp_imm(byte p_value);
		void cmp_abs_x(word p_addr);

		// branches
		void beq(sbyte p_offset);
		void bne(sbyte p_offset);
		void beq(const std::string& p_label);
		void bne(const std::string& p_label);

		// logic
		void and_imm(byte p_value);

		// registers
		void tay(void);
		void tya(void);

		//shifts
		void lsr_a(void);
		void asl_a(void);

		// misc
		void nop(void);
		void add_byte(byte p_value);
		void add_word(word p_word);
	};

}

#endif
