#include "AtlasDevFrameScheduler.h"
#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"
#include <format>
#include <stdexcept>

// neutral frame scheduler: an nmi tick and a post deadline trampoline
// other hacks build on. three arm table slots dispatch PRE vectors before
// OAM DMA on idle frames; one POST vector runs in a bank 9 window after
// the frame's last deadline critical ppu write. see
// AtlasDevFrameScheduler.h for the abi.

namespace {
	constexpr byte HOOK1_ORIG[5]{ 0xa9, 0x07, 0x8d, 0x14, 0x40 };
	constexpr byte HOOK2_ORIG[5]{ 0x8d, 0x01, 0x20, 0xa5, 0x5a };

	void emit_core(klib::Asm6502& code) {
		using namespace fh::afs;
		// tick: one shot arm from the rom table
		code.lda_abs(RAM_INIT);
		code.bne("armed");
		code.lda_abs("arm0"); code.sta_abs(RAM_SLOT0);
		code.lda_abs("arm1"); code.sta_abs(RAM_SLOT1);
		code.lda_abs("arm2"); code.sta_abs(RAM_SLOT2);
		code.lda_imm(0x01);
		code.sta_abs(RAM_INIT);
		code.label("armed");
		code.lda_abs(RAM_SLOT0);
		code.ora_abs(RAM_SLOT1);
		code.ora_abs(RAM_SLOT2);
		code.ora_abs("postarmed");    // a POST-only role must still refresh HEAVY
		code.bne("run");
		code.jmp("dma");
		code.label("run");
		// latch "engine has ppu work this frame" before the drains consume it
		code.lda_zp(0x1f); code.eor_zp(0x20);
		code.bne("qbusy");
		code.lda_zp(0x73); code.ora_zp(0x74);
		code.ora_zp(0x75); code.ora_zp(0x76);
		code.sta_abs(RAM_HEAVY);
		code.jmp("quiet");
		code.label("qbusy");
		code.sta_abs(RAM_HEAVY);
		code.jmp("dma");
		code.label("quiet");
		code.inc_abs(RAM_CNT_LO);
		code.bne("nohi");
		code.inc_abs(RAM_CNT_HI);
		code.label("nohi");
		// one PRE vector per slot, kind byte in A
		code.lda_abs(RAM_SLOT0);
		code.beq("s1");
		code.label("pre0"); code.jsr("stub");
		code.label("s1");
		code.lda_abs(RAM_SLOT1);
		code.beq("s2");
		code.label("pre1"); code.jsr("stub");
		code.label("s2");
		code.lda_abs(RAM_SLOT2);
		code.beq("dma");
		code.label("pre2"); code.jsr("stub");
		code.label("dma");
		code.lda_imm(0x07);           // the displaced OAM DMA
		code.sta_abs(0x4014);
		code.rts();

		// trampoline: displaced deadline write, stand down on busy frames
		// (reading the clean latch consumes it, so the nmi lag path that
		// skips the tick always sees busy), then the POST vector in bank 9
		code.label("tramp");
		code.sta_abs(0x2001);
		code.lda_abs(RAM_HEAVY);
		code.bne("out");
		code.dec_abs(RAM_HEAVY);
		code.lda_abs("postarmed");
		code.beq("out");
		code.lda_abs(0x0100);
		code.pha();
		code.ldx_imm(0x09);
		code.jsr(0xcc85);
		code.label("post"); code.jsr("stub");
		code.pla();
		code.tax();
		code.jsr(0xcc85);
		code.label("out");
		code.lda_zp(0x5a);            // displaced; the caller's BMI reads N
		code.rts();

		code.label("stub");
		code.rts();
		code.label("postarmed"); code.db(0x00);
		code.label("arm0"); code.db(0x00);
		code.label("arm1"); code.db(0x00);
		code.label("arm2"); code.db(0x00);
	}
}

word fh::afs::find_base(const std::vector<byte>& p_rom) {
	const auto h1{ klib::Asm6502::get_file_offset(15, 0xc9af) };
	const auto h2{ klib::Asm6502::get_file_offset(15, 0xc9de) };
	if (p_rom.size() < h2 + 5)
		return 0;
	if (p_rom[h1] != 0x20 || p_rom[h1 + 3] != 0xea || p_rom[h1 + 4] != 0xea)
		return 0;
	const std::size_t base{ static_cast<std::size_t>(p_rom[h1 + 1] | (p_rom[h1 + 2] << 8)) };
	if (base < 0xc000 || base + CORE_SIZE > 0xfffa)
		return 0;
	// the second hook must target the trampoline inside the same block
	if (p_rom[h2] != 0x20 || p_rom[h2 + 3] != 0xea || p_rom[h2 + 4] != 0xea)
		return 0;
	if (static_cast<std::size_t>(p_rom[h2 + 1] | (p_rom[h2 + 2] << 8)) != base + OFF_TRAMP)
		return 0;
	const auto off{ klib::Asm6502::get_file_offset(15, static_cast<word>(base)) };
	if (p_rom.size() < off + CORE_SIZE)
		return 0;
	// every immutable core byte must match the reference emission at this
	// base; only the documented role patch sites may differ
	klib::Asm6502 expected;
	emit_core(expected);
	std::vector<byte> scratch(off + CORE_SIZE, 0xff);
	expected.apply_hack_and_clear(scratch, 15, static_cast<word>(base));
	constexpr std::size_t mutable_sites[]{
		OFF_PRE0, OFF_PRE0 + 1, OFF_PRE1, OFF_PRE1 + 1, OFF_PRE2, OFF_PRE2 + 1,
		OFF_POST, OFF_POST + 1, OFF_POSTARMED, OFF_ARM0, OFF_ARM0 + 1, OFF_ARM0 + 2 };
	for (std::size_t i{ 0 }; i < CORE_SIZE; ++i) {
		bool is_mutable{ false };
		for (auto m : mutable_sites)
			if (m == i) { is_mutable = true; break; }
		if (!is_mutable && p_rom[off + i] != scratch[off + i])
			return 0;
	}
	return static_cast<word>(base);
}

word fh::HackManager::install_AtlasDevFrameScheduler(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack&) const {
	klib::Asm6502 code;
	emit_core(code);
	using namespace fh::afs;

	// the emitted block must match the published abi exactly
	if (code.size() != CORE_SIZE)
		throw std::runtime_error(std::format(
			"AtlasDevFrameScheduler: emitted {} bytes, expected {}", code.size(), CORE_SIZE));
	if (code.label_position("tramp") != OFF_TRAMP
		|| code.label_position("pre0") + 1 != OFF_PRE0
		|| code.label_position("pre1") + 1 != OFF_PRE1
		|| code.label_position("pre2") + 1 != OFF_PRE2
		|| code.label_position("post") + 1 != OFF_POST
		|| code.label_position("postarmed") != OFF_POSTARMED
		|| code.label_position("arm0") != OFF_ARM0)
		throw std::runtime_error("AtlasDevFrameScheduler: abi offsets drifted from the header");
	const auto tramp{ static_cast<word>(cpu_addr + code.label_position("tramp")) };

	const auto off{ klib::Asm6502::get_file_offset(15, cpu_addr) };
	for (std::size_t i{ 0 }; i < CORE_SIZE; ++i)
		if (p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format(
				"AtlasDevFrameScheduler: bank 15 space at ${:04x} is not free", cpu_addr + i));
	const auto h1{ klib::Asm6502::get_file_offset(15, 0xc9af) };
	const auto h2{ klib::Asm6502::get_file_offset(15, 0xc9de) };
	for (std::size_t i{ 0 }; i < 5; ++i)
		if (p_rom[h1 + i] != HOOK1_ORIG[i] || p_rom[h2 + i] != HOOK2_ORIG[i])
			throw std::runtime_error("AtlasDevFrameScheduler: nmi hook sites are not vanilla");

	code.apply_hack_and_clear(p_rom, 15, cpu_addr);
	code.jsr(cpu_addr);
	code.nop(2);
	code.apply_hack_and_clear(p_rom, 15, 0xc9af);
	code.jsr(tramp);
	code.nop(2);
	code.apply_hack_and_clear(p_rom, 15, 0xc9de);

	return static_cast<word>(cpu_addr + CORE_SIZE);
}
