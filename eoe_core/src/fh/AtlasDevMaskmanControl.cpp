#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// maskman control: while maskman strides with his spear forward (poses 0 and
// 4), his touch box becomes his body plus spear pixels on the side he faces,
// so the spear hurts where it is drawn and his body hurts in every pose.
// vanilla gives those poses the box record at $8a75 (x+32, y+16, width $f8,
// height 0). the touch test adds 11 to the width, which overflows to 3, so
// his body does not hurt and only a thin spot to his right does, whichever
// way he faces. the drawn spear is 16 pixels long, which is the default.
//
// one bank 14 site is retargeted: the eight bytes at $8a49 in the box
// routine that point at that record, reached only for maskman in those two
// poses. the hook writes the box itself and returns for the routine, so every
// caller of the routine sees the new box. a clear flag, or mode=vanilla, is
// the vanilla box. every site is checked against its vanilla bytes first, and
// they are identical in the us, us rev a, eu and jp roms. no ram is claimed.
namespace {
	constexpr byte BANK{ 14 };
	constexpr word FACING{ 0x02dc }, BOX{ 0x03e2 }, FLAG_BASE{ 0x0101 };
	constexpr byte ZP_X{ 0xba }, ZP_Y{ 0xc2 };
	constexpr word SITE_DISPATCH{ 0x8a1f }, SITE_BRANCH{ 0x8a40 }, SITE{ 0x8a49 }, V_TAIL{ 0x8a51 };
	constexpr word SPEAR_RECORD{ 0x8a75 }, BODY_RECORD{ 0xb2f3 };
	constexpr int MAX_SPEAR{ 64 }, MAX_FLAG{ 247 };

	// the entity dispatch, the maskman branch, the site, the tail that reads
	// the record, and the two records the hook stands in for
	constexpr std::array<byte, 15> DISPATCH_ORIG{ 0xbd, 0xcc, 0x02, 0xc9, 0x1f, 0xf0, 0x08, 0xc9, 0x20, 0xf0, 0x16, 0xc9, 0x21, 0xd0, 0x23 };
	constexpr std::array<byte, 9> BRANCH_ORIG{ 0xbd, 0xe4, 0x02, 0xf0, 0x04, 0xc9, 0x04, 0xd0, 0x08 };
	constexpr std::array<byte, 8> SITE_ORIG{ 0xa9, 0x75, 0x85, 0x02, 0xa9, 0x8a, 0x85, 0x03 };
	constexpr std::array<byte, 32> TAIL_ORIG{
		0xa0, 0x00, 0xb5, 0xba, 0x18, 0x71, 0x02, 0x8d, 0xe2, 0x03, 0xb5, 0xc2, 0x18, 0xc8, 0x71, 0x02,
		0x8d, 0xe3, 0x03, 0xc8, 0xb1, 0x02, 0x8d, 0xe4, 0x03, 0xc8, 0xb1, 0x02, 0x8d, 0xe5, 0x03, 0x60 };
	constexpr std::array<byte, 4> SPEAR_RECORD_ORIG{ 0x20, 0x10, 0xf8, 0x00 };
	constexpr std::array<byte, 4> BODY_RECORD_ORIG{ 0x00, 0x00, 0x10, 0x20 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}

	// klib::Asm6502 has no lda zp,x
	void lda_zp_x(klib::Asm6502& c, byte p_zp) { c.db(0xb5); c.db(p_zp); }

	void emit_hook(klib::Asm6502& c, int p_spear, int p_flag) {
		if (p_flag >= 0) {
			c.lda_abs(static_cast<word>(FLAG_BASE + (p_flag >> 3))); c.and_imm(static_cast<byte>(1 << (p_flag & 7)));
			c.bne("@on");
			c.lda_imm(0x75); c.sta_zp(0x02); c.lda_imm(0x8a); c.sta_zp(0x03); c.jmp(V_TAIL);   // flag clear: vanilla
		}
		c.label("@on");
		c.lda_abs_x(FACING); c.lsr_a();                                       // carry is facing bit 0, set when he faces right
		lda_zp_x(c, ZP_X); c.bcs("@right");
		c.sec(); c.sbc_imm(static_cast<byte>(p_spear)); c.bcs("@right");      // facing left: the box starts spear pixels earlier
		c.lda_imm(0);                                                          // but not past the left edge
		c.label("@right"); c.sta_abs(BOX);
		lda_zp_x(c, ZP_Y); c.sta_abs(static_cast<word>(BOX + 1));
		c.lda_imm(static_cast<byte>(16 + p_spear)); c.sta_abs(static_cast<word>(BOX + 2));   // his body is 16 wide
		c.lda_imm(0x20); c.sta_abs(static_cast<word>(BOX + 3));                               // and 32 tall
		c.rts();
	}
}

word fh::HackManager::install_AtlasDevMaskmanControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevMaskmanControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default, int p_max) {
		if (!p_hack.has_param(p_id))
			return p_default;
		const int v{ static_cast<int>(p_hack.word_or(p_id, 0)) };
		if (v > p_max)
			throw std::runtime_error(std::format("{}: {} must be 0 to {}", name, p_id, p_max));
		return v;
	};
	const int spear{ get("spear", 16, MAX_SPEAR) };
	const int flag{ get("flag", -1, MAX_FLAG) };
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_DISPATCH, DISPATCH_ORIG, name);
	require_site(p_rom, SITE_BRANCH, BRANCH_ORIG, name);
	require_site(p_rom, SITE, SITE_ORIG, name);
	require_site(p_rom, V_TAIL, TAIL_ORIG, name);
	require_site(p_rom, SPEAR_RECORD, SPEAR_RECORD_ORIG, name);
	require_site(p_rom, BODY_RECORD, BODY_RECORD_ORIG, name);

	klib::Asm6502 code;
	emit_hook(code, spear, flag);
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));

	code.apply_hack_and_clear(p_rom, BANK, cpu_addr);
	code.jmp(cpu_addr); code.apply_hack_and_clear(p_rom, BANK, SITE);
	return static_cast<word>(cpu_addr + size);
}
