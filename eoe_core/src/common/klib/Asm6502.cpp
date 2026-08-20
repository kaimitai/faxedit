#include "Asm6502.h"
#include <algorithm>
#include <cassert>
#include <format>
#include <stdexcept>

constexpr std::size_t INES_HEADER_SIZE{ 0x10 };
constexpr std::size_t PRG_BANK_SIZE{ 0x4000 };

const std::vector<byte>& klib::Asm6502::bytes(void) const {
	return m_bytes;
}

std::size_t klib::Asm6502::size(void) const {
	return m_bytes.size();
}

std::size_t klib::Asm6502::get_file_offset(byte p_bank_no, word p_cpu_addr, word p_cpu_min_addr) {
	assert(p_cpu_addr >= p_cpu_min_addr);

	return INES_HEADER_SIZE +
		PRG_BANK_SIZE * static_cast<std::size_t>(p_bank_no) +
		static_cast<std::size_t>(p_cpu_addr - p_cpu_min_addr);
}

std::size_t klib::Asm6502::get_file_offset(byte p_bank_no, word p_cpu_addr) {
	return get_file_offset(p_bank_no, p_cpu_addr, get_cpu_min_addr(p_bank_no));
}

klib::RomAddress klib::Asm6502::get_rom_address(std::size_t p_file_offset) {
	const std::size_t prg_offset{ p_file_offset - INES_HEADER_SIZE };

	const byte bank{ static_cast<byte>(prg_offset / PRG_BANK_SIZE) };
	const word cpu_min_addr{ get_cpu_min_addr(bank) };

	const word cpu_addr{ static_cast<word>(cpu_min_addr + prg_offset % PRG_BANK_SIZE) };

	return RomAddress{
		.Bank = bank,
		.CpuAddr = cpu_addr
	};
}

word klib::Asm6502::get_cpu_min_addr(byte p_bank_no) {
	return p_bank_no == 15 || p_bank_no == 31 ? 0xc000 : 0x8000;
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
	m_jump_refs.clear();
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

void klib::Asm6502::resolve_labels(word p_base_cpu_addr) {
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

	// Label-targeted JMPs carry a full 16-bit operand, so unlike branches they
	// have no range limit; the absolute address is the hack's base plus the
	// label's offset, which apply_hack supplies when the code is placed.
	for (const auto& jump : m_jump_refs) {

		auto it{ m_labels.find(jump.label) };
		if (it == m_labels.end())
			throw std::runtime_error(std::format("Undefined label: {}", jump.label));

		const auto target = static_cast<word>(p_base_cpu_addr + it->second);
		m_bytes[jump.offset] = static_cast<byte>(target & 0xff);
		m_bytes[jump.offset + 1] = static_cast<byte>((target >> 8) & 0xff);
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
constexpr byte OP_ORA_IMM{ 0x09 };
constexpr byte OP_ASL_A{ 0x0a };
constexpr byte OP_BPL{ 0x10 };
constexpr byte OP_CLC{ 0x18 };
constexpr byte OP_ORA_ABS_Y{ 0x19 };
constexpr byte OP_JSR{ 0x20 };
constexpr byte OP_AND_ZP{ 0x25 };
constexpr byte OP_AND_IMM{ 0x29 };
constexpr byte OP_BMI{ 0x30 };
constexpr byte OP_SEC{ 0x38 };
constexpr byte OP_AND_ABS_Y{ 0x39 };
constexpr byte OP_PHA{ 0x48 };
constexpr byte OP_EOR_IMM{ 0x49 };
constexpr byte OP_LSR_A{ 0x4a };
constexpr byte OP_JMP{ 0x4c };
constexpr byte OP_RTS{ 0x60 };
constexpr byte OP_ADC_ZP{ 0x65 };
constexpr byte OP_PLA{ 0x68 };
constexpr byte OP_ADC_IMM{ 0x69 };
constexpr byte OP_JMP_IND{ 0x6c };
constexpr byte OP_ADC_ABS_X{ 0x7d };
constexpr byte OP_STY_ZP{ 0x84 };
constexpr byte OP_STA_ZP{ 0x85 };
constexpr byte OP_STA_ABS{ 0x8d };
constexpr byte OP_TXA{ 0x8a };
constexpr byte OP_BCC{ 0x90 };
constexpr byte OP_STA_IND_Y{ 0x91 };
constexpr byte OP_TYA{ 0x98 };
constexpr byte OP_STA_ABS_X{ 0x9d };
constexpr byte OP_LDY_IMM{ 0xa0 };
constexpr byte OP_LDX_IMM{ 0xa2 };
constexpr byte OP_LDY_ZP{ 0xa4 };
constexpr byte OP_LDA_ZP{ 0xa5 };
constexpr byte OP_TAY{ 0xa8 };
constexpr byte OP_LDA_IMM{ 0xa9 };
constexpr byte OP_TAX{ 0xaa };
constexpr byte OP_LDA_ABS{ 0xad };
constexpr byte OP_BCS{ 0xb0 };
constexpr byte OP_LDA_IND_Y{ 0xb1 };
constexpr byte OP_LDA_ABS_Y{ 0xb9 };
constexpr byte OP_TSX{ 0xba };
constexpr byte OP_LDA_ABS_X{ 0xbd };
constexpr byte OP_LDX_ABS_Y{ 0xbe };
constexpr byte OP_CPY_IMM{ 0xc0 };
constexpr byte OP_CMP_ZP{ 0xc5 };
constexpr byte OP_DEC_ZP{ 0xc6 };
constexpr byte OP_DEX{ 0xca };
constexpr byte OP_CPY_ABS{ 0xcc };
constexpr byte OP_CMP_ABS{ 0xcd };
constexpr byte OP_INY{ 0xc8 };
constexpr byte OP_CMP_IMM{ 0xc9 };
constexpr byte OP_BNE{ 0xd0 };
constexpr byte OP_CMP_ABS_Y{ 0xd9 };
constexpr byte OP_CMP_ABS_X{ 0xdd };
constexpr byte OP_DEC_ABS_X{ 0xde };
constexpr byte OP_CPX_IMM{ 0xe0 };
constexpr byte OP_INX{ 0xe8 };
constexpr byte OP_SBC_IMM{ 0xe9 };
constexpr byte OP_NOP{ 0xea };
constexpr byte OP_BEQ{ 0xf0 };
constexpr byte OP_SBC_ABS_X{ 0xfd };

// jumps and calls
void klib::Asm6502::jmp(word p_addr) {
	emit(OP_JMP);
	emit_word(p_addr);
}

void klib::Asm6502::jmp(const std::string& p_label) {
	emit(OP_JMP);

	m_jump_refs.push_back({
		m_bytes.size(),
		p_label
		});

	emit_word(word{ 0 }); // patched later
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

void klib::Asm6502::lda_abs_y(word p_addr) {
	emit(OP_LDA_ABS_Y);
	emit_word(p_addr);
}

void klib::Asm6502::lda_ind_y(byte p_addr) {
	emit(OP_LDA_IND_Y);
	emit(p_addr);
}

void klib::Asm6502::ldx_imm(byte p_value) {
	emit(OP_LDX_IMM);
	emit(p_value);
}

void klib::Asm6502::ldx_abs_y(word p_addr) {
	emit(OP_LDX_ABS_Y);
	emit_word(p_addr);
}

void klib::Asm6502::ldy_imm(byte p_value) {
	emit(OP_LDY_IMM);
	emit(p_value);
}

void klib::Asm6502::ldy_zp(byte p_addr) {
	emit(OP_LDY_ZP);
	emit(p_addr);
}

// virtual helper
void klib::Asm6502::lda_mem(word p_addr) {
	if (p_addr <= 0xff)
		lda_zp(static_cast<byte>(p_addr));
	else
		lda_abs(p_addr);
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

void klib::Asm6502::sta_abs_x(word p_addr) {
	emit(OP_STA_ABS_X);
	emit_word(p_addr);
}

void klib::Asm6502::sta_ind_y(byte p_addr) {
	emit(OP_STA_IND_Y);
	emit(p_addr);
}

void klib::Asm6502::sty_zp(byte p_addr) {
	emit(OP_STY_ZP);
	emit(p_addr);
}

// virtual helper
void klib::Asm6502::sta_mem(word p_addr) {
	if (p_addr <= 0xff)
		sta_zp(static_cast<byte>(p_addr));
	else
		sta_abs(p_addr);
}

// compares
void klib::Asm6502::cmp_zp(byte p_addr) {
	emit(OP_CMP_ZP);
	emit(p_addr);
}

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

void klib::Asm6502::cmp_abs_y(word p_addr) {
	emit(OP_CMP_ABS_Y);
	emit_word(p_addr);
}

void klib::Asm6502::cpx_imm(byte p_value) {
	emit(OP_CPX_IMM);
	emit(p_value);
}

void klib::Asm6502::cpy_imm(byte p_value) {
	emit(OP_CPY_IMM);
	emit(p_value);
}

void klib::Asm6502::cpy_abs(word p_addr) {
	emit(OP_CPY_ABS);
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

void klib::Asm6502::bcc(sbyte p_offset) {
	emit(OP_BCC);
	emit(p_offset);
}

void klib::Asm6502::bcs(sbyte p_offset) {
	emit(OP_BCS);
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

void klib::Asm6502::bcc(const std::string& p_label) {
	branch(OP_BCC, p_label);
}

void klib::Asm6502::bcs(const std::string& p_label) {
	branch(OP_BCS, p_label);
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

void klib::Asm6502::and_zp(byte p_addr) {
	emit(OP_AND_ZP);
	emit(p_addr);
}

void klib::Asm6502::and_abs_y(word p_addr) {
	emit(OP_AND_ABS_Y);
	emit_word(p_addr);
}

void klib::Asm6502::ora_imm(byte p_value) {
	emit(OP_ORA_IMM);
	emit(p_value);
}

void klib::Asm6502::ora_abs_y(word p_addr) {
	emit(OP_ORA_ABS_Y);
	emit_word(p_addr);
}

void klib::Asm6502::eor_imm(byte p_value) {
	emit(OP_EOR_IMM);
	emit(p_value);
}

// stack
void klib::Asm6502::pha(void) {
	emit(OP_PHA);
}

void klib::Asm6502::pla(void) {
	emit(OP_PLA);
}

// registers
void klib::Asm6502::tax(void) {
	emit(OP_TAX);
}

void klib::Asm6502::tay(void) {
	emit(OP_TAY);
}

void klib::Asm6502::tya(void) {
	emit(OP_TYA);
}

void klib::Asm6502::txa(void) {
	emit(OP_TXA);
}

void klib::Asm6502::tsx(void) {
	emit(OP_TSX);
}

// shifts
void klib::Asm6502::lsr_a(std::size_t count) {
	for (std::size_t i{ 0 }; i < count;++i)
		emit(OP_LSR_A);
}


void klib::Asm6502::asl_a(void) {
	emit(OP_ASL_A);
}

// math
void klib::Asm6502::inx(void) {
	emit(OP_INX);
}

void klib::Asm6502::dec_zp(byte p_addr) {
	emit(OP_DEC_ZP);
	emit(p_addr);
}

void klib::Asm6502::dec_abs_x(word p_addr) {
	emit(OP_DEC_ABS_X);
	emit_word(p_addr);
}

void klib::Asm6502::adc_imm(byte p_value) {
	emit(OP_ADC_IMM);
	emit(p_value);
}

void klib::Asm6502::adc_zp(byte p_addr) {
	emit(OP_ADC_ZP);
	emit(p_addr);
}

void klib::Asm6502::adc_abs_x(word p_addr) {
	emit(OP_ADC_ABS_X);
	emit_word(p_addr);
}

void klib::Asm6502::sbc_imm(byte p_value) {
	emit(OP_SBC_IMM);
	emit(p_value);
}

void klib::Asm6502::sbc_abs_x(word p_addr) {
	emit(OP_SBC_ABS_X);
	emit_word(p_addr);
}

void klib::Asm6502::dex(void) {
	emit(OP_DEX);
}

void klib::Asm6502::iny(void) {
	emit(OP_INY);
}

void klib::Asm6502::sec(void) {
	emit(OP_SEC);
}

void klib::Asm6502::clc(void) {
	emit(OP_CLC);
}

// misc
void klib::Asm6502::nop(std::size_t count) {
	for (std::size_t i{ 0 }; i < count; ++i)
		emit(OP_NOP);
}

void klib::Asm6502::db(byte p_value) {
	emit(p_value);
}

void klib::Asm6502::dw(word p_word) {
	emit_word(p_word);
}

void klib::Asm6502::apply_hack(std::vector<byte>& p_rom, byte p_bank_no,
	word p_cpu_addr, word p_cpu_min_addr) const {
	const std::size_t file_offset = get_file_offset(p_bank_no, p_cpu_addr, p_cpu_min_addr);
	assert(file_offset + m_bytes.size() <= p_rom.size());
	std::copy(m_bytes.begin(), m_bytes.end(), p_rom.begin() + file_offset);
}

std::size_t klib::Asm6502::apply_hack_and_clear(std::vector<byte>& p_rom, byte p_bank_no,
	word p_cpu_addr, word p_cpu_min_addr) {
	resolve_labels(p_cpu_addr);
	std::size_t result{ size() };
	apply_hack(p_rom, p_bank_no, p_cpu_addr, p_cpu_min_addr);
	clear();
	return result;
}

std::size_t klib::Asm6502::apply_hack_and_clear(std::vector<byte>& p_rom, byte p_bank_no,
	word p_cpu_addr) {
	return apply_hack_and_clear(p_rom, p_bank_no, p_cpu_addr, get_cpu_min_addr(p_bank_no));
}

void klib::Asm6502::apply_byte(std::vector<byte>& p_rom, byte p_byte,
	byte p_bank_no, word p_cpu_addr, word p_cpu_min_addr) {
	p_rom.at(get_file_offset(p_bank_no, p_cpu_addr, p_cpu_min_addr)) = p_byte;
}

void klib::Asm6502::apply_byte(std::vector<byte>& p_rom, byte p_byte,
	byte p_bank_no, word p_cpu_addr) {
	apply_byte(p_rom, p_byte, p_bank_no, p_cpu_addr, get_cpu_min_addr(p_bank_no));
}

std::size_t klib::Asm6502::apply_bytes(std::vector<byte>& p_rom, const std::vector<byte>& p_bytes,
	byte p_bank_no, word p_cpu_addr, word p_cpu_min_addr) {
	std::copy(p_bytes.begin(), p_bytes.end(), p_rom.begin() + get_file_offset(p_bank_no, p_cpu_addr, p_cpu_min_addr));

	return p_bytes.size();
}

std::size_t klib::Asm6502::apply_bytes(std::vector<byte>& p_rom, const std::vector<byte>& p_bytes,
	byte p_bank_no, word p_cpu_addr) {
	return apply_bytes(p_rom, p_bytes, p_bank_no, p_cpu_addr, get_cpu_min_addr(p_bank_no));
}

void klib::Asm6502::apply_word(std::vector<byte>& p_rom, word p_word,
	byte p_bank_no, word p_cpu_addr) {
	std::size_t file_offset{ get_file_offset(p_bank_no, p_cpu_addr) };

	p_rom.at(file_offset) = static_cast<byte>(p_word % 256);
	p_rom.at(file_offset + 1) = static_cast<byte>(p_word / 256);
}

std::size_t klib::Asm6502::apply_words(std::vector<byte>& p_rom, const std::vector<word>& p_words,
	byte p_bank_no, word p_cpu_addr, word p_cpu_min_addr) {

	for (std::size_t i{ 0 }; i < p_words.size(); ++i)
		apply_word(p_rom, p_words[i], p_bank_no, static_cast<word>(p_cpu_addr + 2 * i));

	return 2 * p_words.size();
}

std::size_t klib::Asm6502::apply_words(std::vector<byte>& p_rom, const std::vector<word>& p_words,
	byte p_bank_no, word p_cpu_addr) {
	return apply_words(p_rom, p_words, p_bank_no, p_cpu_addr, get_cpu_min_addr(p_bank_no));
}

std::size_t klib::Asm6502::apply_words_as_split_table(std::vector<byte>& p_rom, const std::vector<word>& p_words,
	byte p_bank_no, word p_cpu_addr, word p_cpu_min_addr) {
	std::size_t lo_offset{ get_file_offset(p_bank_no, p_cpu_addr, p_cpu_min_addr) };
	std::size_t hi_offset{ get_file_offset(p_bank_no, p_cpu_addr + static_cast<word>(p_words.size()), p_cpu_min_addr) };

	for (std::size_t i{ 0 }; i < p_words.size(); ++i) {
		p_rom.at(hi_offset + i) = static_cast<byte>(p_words[i] / 256);
		p_rom.at(lo_offset + i) = static_cast<byte>(p_words[i] % 256);
	}

	return 2 * p_words.size();
}

std::size_t klib::Asm6502::apply_words_as_split_table(std::vector<byte>& p_rom, const std::vector<word>& p_words,
	byte p_bank_no, word p_cpu_addr) {
	return apply_words_as_split_table(p_rom, p_words, p_bank_no, p_cpu_addr, get_cpu_min_addr(p_bank_no));
}

word klib::Asm6502::read_word(const std::vector<byte>& p_rom, std::size_t p_file_offset) {
	return static_cast<word>(p_rom.at(p_file_offset) | p_rom.at(p_file_offset + 1) << 8);
}

word klib::Asm6502::read_word(const std::vector<byte>& p_rom, byte p_bank_no, word p_cpu_addr) {
	return read_word(p_rom, get_file_offset(p_bank_no, p_cpu_addr));
}
