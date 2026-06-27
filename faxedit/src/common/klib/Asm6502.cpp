#include "Asm6502.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <stdexcept>

const std::vector<byte>& klib::Asm6502::bytes(void) const {
	return m_bytes;
}

std::size_t klib::Asm6502::size(void) const {
	return m_bytes.size();
}

void klib::Asm6502::label(const std::string& p_name) {
	if (m_labels.contains(p_name))
		throw std::runtime_error(std::format("Duplicate label: {}", p_name));

	m_labels[p_name] = m_bytes.size();
}

void klib::Asm6502::clear(void) {
	m_bytes.clear();
	m_labels.clear();
	m_branch_refs.clear();
}

void klib::Asm6502::emit(byte p_byte) {
	m_bytes.push_back(p_byte);
}

void klib::Asm6502::emit(sbyte p_byte) {
	emit(static_cast<byte>(p_byte));
}

void klib::Asm6502::emit_word(word p_word) {
	emit(static_cast<byte>(p_word & 0xff));
	emit(static_cast<byte>((p_word >> 8) & 0xff));
}

void klib::Asm6502::resolve_labels() {
	for (const auto& branch : m_branch_refs) {

		auto it{ m_labels.find(branch.label) };
		if (it == m_labels.end())
			throw std::runtime_error(std::format("Undefined label: {}", branch.label));

		const auto target = static_cast<std::ptrdiff_t>(it->second);
		const auto next = static_cast<std::ptrdiff_t>(branch.offset + 1);
		const auto delta = target - next;

		if (delta < -128 || delta > 127)
			throw std::runtime_error(std::format("Branch out of range: {}", branch.label));

		m_bytes[branch.offset] = static_cast<byte>(static_cast<sbyte>(delta));
	}
}

void klib::Asm6502::branch(byte p_opcode, const std::string& p_label) {
	emit(p_opcode);

	m_branch_refs.push_back({
		m_bytes.size(),
		p_label
		});

	emit(byte{ 0 }); // patched later
}

// opcode constants
constexpr byte OP_ASL_A{ 0x0a };
constexpr byte OP_BPL{ 0x10 };
constexpr byte OP_JSR{ 0x20 };
constexpr byte OP_AND_IMM{ 0x29 };
constexpr byte OP_BMI{ 0x30 };
constexpr byte OP_LSR_A{ 0x4a };
constexpr byte OP_JMP{ 0x4c };
constexpr byte OP_RTS{ 0x60 };
constexpr byte OP_JMP_IND{ 0x6c };
constexpr byte OP_STA_ZP{ 0x85 };
constexpr byte OP_STA_ABS{ 0x8d };
constexpr byte OP_TYA{ 0x98 };
constexpr byte OP_LDX_IMM{ 0xa2 };
constexpr byte OP_LDA_ZP{ 0xa5 };
constexpr byte OP_TAY{ 0xa8 };
constexpr byte OP_LDA_IMM{ 0xa9 };
constexpr byte OP_LDA_ABS{ 0xad };
constexpr byte OP_LDA_ABS_X{ 0xbd };
constexpr byte OP_DEX{ 0xca };
constexpr byte OP_CMP_ABS{ 0xcd };
constexpr byte OP_CMP_IMM{ 0xc9 };
constexpr byte OP_BNE{ 0xd0 };
constexpr byte OP_CMP_ABS_X{ 0xdd };
constexpr byte OP_NOP{ 0xea };
constexpr byte OP_BEQ{ 0xf0 };

// jumps and calls
void klib::Asm6502::jmp(word p_addr) {
	emit(OP_JMP);
	emit_word(p_addr);
}

void klib::Asm6502::jmp_ind(word p_addr) {
	emit(OP_JMP_IND);
	emit_word(p_addr);
}

void klib::Asm6502::jsr(word p_addr) {
	emit(OP_JSR);
	emit_word(p_addr);
}

void klib::Asm6502::rts(void) {
	emit(OP_RTS);
}

// loads
void klib::Asm6502::lda_zp(byte p_addr) {
	emit(OP_LDA_ZP);
	emit(p_addr);
}

void klib::Asm6502::lda_imm(byte p_value) {
	emit(OP_LDA_IMM);
	emit(p_value);
}

void klib::Asm6502::lda_abs(word p_addr) {
	emit(OP_LDA_ABS);
	emit_word(p_addr);
}

void klib::Asm6502::lda_abs_x(word p_addr) {
	emit(OP_LDA_ABS_X);
	emit_word(p_addr);
}

void klib::Asm6502::ldx_imm(byte p_value) {
	emit(OP_LDX_IMM);
	emit(p_value);
}

// stores
void  klib::Asm6502::sta_zp(byte p_addr) {
	emit(OP_STA_ZP);
	emit(p_addr);
}

void klib::Asm6502::sta_abs(word p_addr) {
	emit(OP_STA_ABS);
	emit_word(p_addr);
}

// compares
void klib::Asm6502::cmp_imm(byte p_value) {
	emit(OP_CMP_IMM);
	emit(p_value);
}

void klib::Asm6502::cmp_abs(word p_addr) {
	emit(OP_CMP_ABS);
	emit_word(p_addr);
}

void klib::Asm6502::cmp_abs_x(word p_addr) {
	emit(OP_CMP_ABS_X);
	emit_word(p_addr);
}

// branches
void klib::Asm6502::beq(sbyte p_offset) {
	emit(OP_BEQ);
	emit(p_offset);
}

void klib::Asm6502::bne(sbyte p_offset) {
	emit(OP_BNE);
	emit(p_offset);
}

void klib::Asm6502::bpl(sbyte p_offset) {
	emit(OP_BPL);
	emit(p_offset);
}

void klib::Asm6502::bmi(sbyte p_offset) {
	emit(OP_BMI);
	emit(p_offset);
}

void klib::Asm6502::beq(const std::string& p_label) {
	branch(OP_BEQ, p_label);
}

void klib::Asm6502::bne(const std::string& p_label) {
	branch(OP_BNE, p_label);
}

void klib::Asm6502::bpl(const std::string& p_label) {
	branch(OP_BPL, p_label);
}

void klib::Asm6502::bmi(const std::string& p_label) {
	branch(OP_BMI, p_label);
}

// logical
void klib::Asm6502::and_imm(byte p_value) {
	emit(OP_AND_IMM);
	emit(p_value);
}

// registers
void klib::Asm6502::tay(void) {
	emit(OP_TAY);
}

void klib::Asm6502::tya(void) {
	emit(OP_TYA);
}

// shifts
void klib::Asm6502::lsr_a(void) {
	emit(OP_LSR_A);
}


void klib::Asm6502::asl_a(void) {
	emit(OP_ASL_A);
}

// math
void klib::Asm6502::dex(void) {
	emit(OP_DEX);
}

// misc
void klib::Asm6502::nop(void) {
	emit(OP_NOP);
}

void klib::Asm6502::add_byte(byte p_value) {
	emit(p_value);
}

void klib::Asm6502::add_word(word p_word) {
	emit_word(p_word);
}

void klib::Asm6502::apply_hack(std::vector<byte>& p_rom, byte p_bank_no,
	word p_cpu_addr, word p_cpu_min_addr) const {
	constexpr std::size_t INES_HEADER_SIZE{ 0x10 };
	constexpr std::size_t PRG_BANK_SIZE{ 0x4000 };

	assert(p_cpu_addr >= p_cpu_min_addr);

	const std::size_t file_offset{ INES_HEADER_SIZE +
	PRG_BANK_SIZE * static_cast<std::size_t>(p_bank_no) +
	static_cast<std::size_t>(p_cpu_addr - p_cpu_min_addr) };

	assert(file_offset + m_bytes.size() <= p_rom.size());

	std::copy(m_bytes.begin(), m_bytes.end(), p_rom.begin() + file_offset);
}

void klib::Asm6502::apply_hack_and_clear(std::vector<byte>& p_rom, byte p_bank_no,
	word p_cpu_addr, word p_cpu_min_addr) {
	resolve_labels();
	apply_hack(p_rom, p_bank_no, p_cpu_addr, p_cpu_min_addr);
	clear();
}
