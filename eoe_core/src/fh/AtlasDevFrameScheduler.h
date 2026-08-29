#ifndef FH_ATLAS_DEV_FRAME_SCHEDULER_H
#define FH_ATLAS_DEV_FRAME_SCHEDULER_H

#include <cstdint>
#include <vector>

using byte = unsigned char;
using word = uint16_t;

// public abi of the AtlasDevFrameScheduler general hack, for role hacks
// that build on it. the core is cursor allocated in bank 15; discover its
// base from the tick hook at $c9af (jsr <base> / nop / nop), then patch
// the offsets below relative to that base.
namespace fh::afs {

	// ram (fixed): three slots, 16 bit frame counter, busy latch, init marker
	constexpr word RAM_SLOT0{ 0x04d8 };
	constexpr word RAM_SLOT1{ 0x04d9 };
	constexpr word RAM_SLOT2{ 0x04da };
	constexpr word RAM_CNT_LO{ 0x04db };
	constexpr word RAM_CNT_HI{ 0x04dc };
	constexpr word RAM_HEAVY{ 0x04dd };
	constexpr word RAM_INIT{ 0x04de };

	// block size and offsets relative to the discovered base. a slot's PRE
	// vector operand must hold a fixed bank ($c000 and up) handler address;
	// the handler is called with the slot's kind byte in A, before OAM DMA,
	// on frames where the engine's ppu queue and nametable strips are idle.
	// the POST vector runs with bank 9 switched in, after the frame's last
	// deadline critical ppu write; enable it by setting OFF_POSTARMED
	// nonzero. arm a slot at boot by writing the role's kind byte at
	// OFF_ARM0 plus the slot number.
	constexpr std::size_t CORE_SIZE{ 150 };
	constexpr std::size_t OFF_TRAMP{ 0x6d };       // trampoline entry (hook 2 target)
	constexpr std::size_t OFF_PRE0{ 0x55 };        // jsr operand, slot 0
	constexpr std::size_t OFF_PRE1{ 0x5d };        // jsr operand, slot 1
	constexpr std::size_t OFF_PRE2{ 0x65 };        // jsr operand, slot 2
	constexpr std::size_t OFF_POST{ 0x87 };        // jsr operand, POST lane
	constexpr std::size_t OFF_POSTARMED{ 0x92 };   // 0 = POST disabled
	constexpr std::size_t OFF_ARM0{ 0x93 };        // three arm table bytes

	// returns the core's base cpu address, or 0 if the scheduler is not
	// installed. fails closed: both hooks must carry jsr/nop/nop with the
	// second targeting base plus OFF_TRAMP, the block must fit the fixed
	// bank, and every immutable core byte must match the reference
	// emission; only the documented patch sites (vectors, postarmed, arm
	// table) may differ. safe on truncated or arbitrary input.
	word find_base(const std::vector<byte>& p_rom);
}

#endif
