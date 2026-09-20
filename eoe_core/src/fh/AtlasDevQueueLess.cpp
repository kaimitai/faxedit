#include "HackManager.h"
#include "common/klib/Asm6502.h"
#include <algorithm>
#include <format>
#include <stdexcept>
#include <string_view>

// reuse an already loaded sprite set when another room slot has the same
// entity id. the companion uploader keeps its own path. no extra ram.
namespace {
	using Asm = klib::Asm6502;
	void require_site(const std::vector<byte>& rom, word addr, std::string_view hex) {
		const auto off{ Asm::get_file_offset(15, addr) };
		const auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
		for (std::size_t i{ 0 }; i < hex.size() / 2; ++i)
			if (hex[2 * i] != '?' && rom.at(off + i) != nibble(hex[2 * i]) * 16 + nibble(hex[2 * i + 1]))
				throw std::runtime_error(std::format(
					"AtlasDevQueueLess: incompatible sprite upload code at ${:04x}", addr + i));
	}
	Asm dedup_code() {
		Asm code;
		code.lda_abs(0x038b); code.cmp_imm(0x30); code.beq("upload");
		code.ldx_abs(0x0378);
		// the room loader visits slots seven through zero. only higher slots
		// have valid tile bases, and empty slots have already been skipped.
		code.label("scan"); code.inx(); code.cpx_imm(8); code.bcs("upload");
		code.lda_abs_x(0x02cc); code.cmp_abs(0x038b); code.bne("scan");
		code.lda_abs_x(0x032c); code.sta_zp(0x9a); code.rts();
		code.label("upload"); code.jmp(0xcdb5);
		return code;
	}
}

word fh::HackManager::install_AtlasDevQueueLess(const fe::Config&, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack) const {
	const byte dedup{ hack.byte_or("dedup", 1) };
	if (dedup > 1) throw std::runtime_error("AtlasDevQueueLess: dedup must be 0 or 1");
	if (!dedup) return cpu_addr;
	if (rom.size() != 0x40010 || rom[0] != 'N' || rom[1] != 'E'
		|| rom[2] != 'S' || rom[3] != 0x1a || rom[4] != 16 || rom[5] != 0
		|| (rom[6] & 0xfc) != 0x10 || (rom[7] & 0xf0) != 0
		|| ((rom[7] & 0x0c) != 0 && (rom[7] & 0x0c) != 8)
		|| ((rom[7] & 0x0c) == 8 && (rom[8] != 0 || rom[9] != 0)))
		throw std::runtime_error("AtlasDevQueueLess: requires unexpanded MMC1 PRG with CHR RAM and no trainer");
	require_site(rom, 0xc27c, "a000ad8b03c9??9001c8b959c28d860360");
	require_site(rom, 0xc28d,
		"ad780348206fcda2078e7803bdcc02c9fff03c8d8b03207cc220b5cdad8b03c930"
		"d019ad8b0348a59a48a9098d8b03207cc220b5cd68859a688d8b03ae7803a59a9d2c"
		"03b5c2c9f09005a9ff9dcc02ae7803ca10b4688d78034cf4cf");
	require_site(rom, 0xcd6f, "a9098599a900859860");
	// the entity id selects both the CHR source and its tile count.
	require_site(rom, 0xcd78,
		"ad000148ae8603201accad00808502ad01801869808503ad8b03c9??9002e9??0aa8"
		"b1028596c8b1021869808597ad8b03a8b91bce859b68aa201acc60");
	// graphics export can move the bank split. its selector, comparison and
	// subtraction must agree; the retail split is not an allocation rule.
	const auto split{ rom[Asm::get_file_offset(15, 0xc282)] };
	if (rom[Asm::get_file_offset(15, 0xcd93)] != split
		|| rom[Asm::get_file_offset(15, 0xcd97)] != split)
		throw std::runtime_error("AtlasDevQueueLess: inconsistent sprite bank split");
	// the skipped uploader only builds the tile base, queues CHR bytes and
	// advances the shared graphics cursor. its bank switch is restored.
	require_site(rom, 0xcdb5,
		"a5988500a59906002a06002a06002a06002a859a2078cda59bd00160ad000148ae8603"
		"201acca59985e9a59885e8a91020dccfa000b1969d0005e8c8c01090f5862068aa201acc"
		"a5961869108596a59769008597a5981869108598a59969008599c69bd0b760");
	auto code{ dedup_code() };
	if (code.size() != 32 || std::size_t{ cpu_addr } + code.size() > 0xfffa)
		throw std::runtime_error("AtlasDevQueueLess: helper reaches interrupt vectors");
	const auto off{ Asm::get_file_offset(15, cpu_addr) };
	if (!std::all_of(rom.begin() + off, rom.begin() + off + code.size(),
		[](byte value) { return value == 0xff; }))
		throw std::runtime_error("AtlasDevQueueLess: allocated helper range is occupied");
	code.apply_hack_noclear(rom, 15, cpu_addr);
	Asm hook; hook.jsr(cpu_addr); hook.apply_hack_noclear(rom, 15, 0xc2a6);
	return static_cast<word>(cpu_addr + code.size());
}
