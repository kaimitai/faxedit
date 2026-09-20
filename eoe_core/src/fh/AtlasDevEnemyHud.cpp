#include "HackManager.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	constexpr word BODY{ 0x8000 }, TTL{ 0x04e3 }, SLOT{ 0x04e4 }, TYPE{ 0x04e5 };
	constexpr word SCREEN{ 0x04e6 }, AREA{ 0x04e7 }, VISIBLE{ 0x04e8 }, HP{ 0x04e9 };
	constexpr byte COUNT{ 101 }, TIMEOUT{ 180 };
	// Community names, shortened to fit the HUD.
	const std::map<byte, std::string> NAMES{
		{3,"UNKNOWN ENEMY"},{4,"RAIDEN"},{5,"NECRON AIDES"},{6,"ZOMBIE"},{7,"HORNET"},
		{8,"BIHORUDA"},{9,"LILITH"},{11,"YUINARU"},{12,"SNOWMAN"},{13,"NASH"},
		{14,"FIRE GIANT"},{15,"ISHIISU"},{16,"EXECUTION HOOD"},{17,"ROKUSUTAHN"},
		{18,"SNAKE BODY"},{21,"CHARRON"},{22,"UNKNOWN ENEMY"},{23,"GERIBUTA"},
		{24,"SUGATA"},{25,"GRIMLOCK"},{26,"GIANT BEES"},{27,"MYCONID"},{28,"NAGA"},
		{29,"SKELETON KNIGHT"},{30,"GIANT STRIDER"},{31,"SIR GAWAINE"},{32,"MASKMAN"},
		{33,"WOLFMAN"},{34,"YAREEKA"},{35,"MAGMAN"},{36,"SPEAR CARRIER"},
		{37,"UNKNOWN ENEMY"},{38,"IKEDA"},{39,"MUPPET GUY"},{40,"LAMPREY"},
		{41,"UNKNOWN ENEMY"},{42,"MONODRON"},{43,"WINGED SKELETON"},{44,"TAMAZUTSU"},
		{45,"RIPASHEIKU"},{46,"ZORADOHNA"},{47,"BORABOHRA"},{48,"PAKUKAME"},
		{49,"ZORUGERIRU"},{50,"KING GRIEVE"},{51,"SHADOW EURA"},{70,"EYEBALL"},{71,"ZOZURA"}
	};

	[[noreturn]] void fail(const std::string& text) { throw std::runtime_error("AtlasDevEnemyHud: " + text); }
	void raw(klib::Asm6502& a, std::initializer_list<byte> bytes) { for (byte b : bytes) a.db(b); }
	void abs(klib::Asm6502& a, byte op, word address) {
		raw(a, {op, static_cast<byte>(address), static_cast<byte>(address >> 8)});
	}
	std::size_t off(byte bank, word address) { return klib::Asm6502::get_file_offset(bank, address); }
	void site(const std::vector<byte>& rom, byte bank, word address, std::initializer_list<byte> expected) {
		auto at{ off(bank, address) };
		for (byte b : expected) if (at >= rom.size() || rom[at++] != b) fail("a required vanilla instruction is not intact");
	}
	void space(const std::vector<byte>& rom, byte bank, word address, std::size_t size) {
		const std::size_t end{ bank == 15 ? 0xffe0u : 0xc000u };
		if (address + size > end) fail("code exceeds the reserved bank window");
		for (std::size_t i{0}; i < size; ++i)
			if (off(bank,address)+i >= rom.size() || rom[off(bank,address)+i] != 0xff)
				fail("required code space is already occupied");
	}

	klib::Asm6502 renderer(const std::array<byte, COUNT>& maxima, bool names, bool persistent, word queue) {
		klib::Asm6502 a;
		const byte width{ static_cast<byte>(names ? 29 : 13) };
		a.lda_abs(TTL); a.bne("active"); a.jmp("inactive");
		a.label("active"); a.lda_abs(0x0438); a.bne("invalid");
		a.lda_abs(SCREEN); a.cmp_zp(0x63); a.bne("invalid");
		a.lda_abs(AREA); a.cmp_zp(0x24); a.bne("invalid");
		a.ldx_abs(SLOT); a.cpx_imm(8); a.bcs("invalid");
		a.lda_abs_x(0x02cc); a.cmp_abs(TYPE); a.bne("invalid");
		a.cmp_imm(COUNT); a.bcs("invalid"); a.tay();
		a.lda_abs_y("supported"); a.beq("invalid");
		a.lda_abs_x(0x0344); a.sta_abs(HP);
		a.lda_abs_y("maximum"); a.cmp_abs(HP); a.bcc("invalid");
		if (!persistent) { a.dec_abs(TTL); a.beq("inactive"); }
		a.jmp("guard"); a.label("invalid"); a.lda_imm(0); a.sta_abs(TTL);
		a.label("inactive"); a.lda_abs(VISIBLE); a.bne("guard"); a.rts();
		a.label("guard"); a.lda_zp(0x1f); a.cmp_zp(0x20); a.bne("return");
		a.lda_zp(0x73); for (byte zp : {0x74,0x75,0x76,0x0e,0x5a}) a.ora_zp(zp);
		a.bne("return"); a.lda_zp(0x54); a.cmp_imm(0xff); a.bne("return");
		a.lda_zp(0x1a); a.and_imm(3); a.bne("return"); a.jmp("draw");
		a.label("return"); a.rts(); a.label("draw"); a.ldx_zp(0x20);
		for (byte b : {width, byte{0x20}, byte{0x21}}) { a.lda_imm(b); a.jsr(queue); }
		a.lda_abs(TTL); a.bne("name"); a.ldy_imm(width); a.lda_imm(0);
		a.label("blank"); a.jsr(queue); a.db(0x88); a.bne("blank");
		a.sta_abs(VISIBLE); a.jmp("publish"); a.label("name"); a.lda_imm(1); a.sta_abs(VISIBLE);
		abs(a,0xac,TYPE); a.lda_imm(0x14); a.jsr(queue); a.lda_imm(10); a.jsr(queue);
		for (int col{0}; col < 10; ++col) {
			const auto suffix{ std::to_string(col) };
			a.lda_abs_y("threshold_"+suffix); a.cmp_abs(HP); a.bcs("empty_"+suffix);
			a.lda_imm(9); a.jmp("put_"+suffix); a.label("empty_"+suffix); a.lda_imm(7);
			a.label("put_"+suffix); a.jsr(queue);
		}
		a.lda_imm(11); a.jsr(queue);
		if (names) {
			a.lda_imm(0); a.jsr(queue);
			for (int col{0}; col < 15; ++col) { a.lda_abs_y("name_"+std::to_string(col)); a.jsr(queue); }
		}
		// Update the queue cursor last so NMI cannot read a partial update.
		a.label("publish"); a.stx_zp(0x20); a.rts();
		a.label("supported"); for (byte i{0}; i < COUNT; ++i) a.db(NAMES.contains(i) ? 1 : 0);
		a.label("maximum"); for (byte hp : maxima) a.db(hp);
		if (names) for (std::size_t col{0}; col < 15; ++col) {
			a.label("name_"+std::to_string(col));
			for (byte i{0}; i < COUNT; ++i) {
				const auto found{ NAMES.find(i) };
				const char c{ found != NAMES.end() && col < found->second.size() ? found->second[col] : ' ' };
				a.db(c == ' ' ? 0 : static_cast<byte>(c-'A'+0x10));
			}
		}
		for (int col{0}; col < 10; ++col) {
			a.label("threshold_"+std::to_string(col));
			for (byte hp : maxima) a.db(static_cast<byte>(hp*col/10));
		}
		return a;
	}
}

word fh::HackManager::install_AtlasDevEnemyHud(const fe::Config& config, std::vector<byte>& rom,
	word cpu_addr, const fh::GeneralHack& hack) const {
	const bool names{ hack.bool_or("names", true) };
	const auto visibility{ hack.string_or("visibility", "timed") };
	if (visibility != "timed" && visibility != "always") fail("visibility must be timed or always");
	if (hack.has_param("mode") && hack.get_string("mode") != "vanilla") fail("mode must be vanilla");
	if (hack.string_or("mode", "") == "vanilla") return cpu_addr;
	const auto region{ config.get_region() };
	if ((region != "us" && region != "us-rev-a" && region != "eu" && region != "jp") ||
		!config.has_constant("hack_enemy_hud_enabled") || config.constant("hack_enemy_hud_enabled") != 1)
		fail("hack_enemy_hud_enabled is unavailable for this configuration region");
	const word queue{ static_cast<word>(config.constant("rom_ppu_queue_payload")) };
	const word far_call{ static_cast<word>(config.constant("rom_vanilla_far_call")) };
	if (rom.size() != 0x40010 || rom[0] != 'N' || rom[1] != 'E' || rom[2] != 'S' || rom[3] != 0x1a ||
		rom[4] != 16 || rom[5] != 0 || (rom[6] & 0xf4) != 0x10 || (rom[7] & 0xf0) != 0)
		fail("requires an unexpanded MMC1 cartridge without a trainer");
	site(rom,14,0x8897,{0x9d,0x44,0x03}); site(rom,14,0x8203,{0x9d,0x44,0x03});
	site(rom,15,0xc134,{0x9d,0xcc,0x02}); site(rom,15,0xc238,{0x9d,0x44,0x03});
	site(rom,15,0xdb6e,{0x20,0x16,0xe0});
	site(rom,15,0xc9af,{0xa9,0x07,0x8d,0x14,0x40});
	site(rom,15,queue,{0x9d,0x00,0x05,0xe8,0x60});
	if (region == "us-rev-a" || region == "eu")
		site(rom,15,far_call,{0x85,0xe2,0x86,0xe3,0x84,0xe4,0x68,0x85,0xec});
	else
		site(rom,15,far_call,{0x85,0xde,0x86,0xdf,0x84,0xe0,0x68,0x85,0xec});
	site(rom,15,0xe016,{0xa5,0x24,0xd0,0x04,0xa5,0x63,0xf0,0x0c});
	site(rom,15,0xc235,{0xb9,0xa9,0xb5});
	std::array<byte,COUNT> maxima{};
	for (std::size_t i{0}; i < COUNT; ++i) maxima[i] = rom.at(off(14,0xb5a9)+i);
	auto body{ renderer(maxima,names,visibility=="always",queue) };
	klib::Asm6502 f;
	// Check the damage carry before changing flags; preserve registers and flags.
	f.label("hit"); f.sta_abs_x(0x0344); f.db(0x08); f.pha(); f.bcc("clear");
	f.lda_abs_x(0x02cc); f.sta_abs(TYPE); abs(f,0x8e,SLOT);
	f.lda_zp(0x63); f.sta_abs(SCREEN); f.lda_zp(0x24); f.sta_abs(AREA);
	f.lda_imm(TIMEOUT); f.sta_abs(TTL); f.jmp("restore");
	f.label("clear"); f.lda_imm(0); f.sta_abs(TTL);
	f.label("restore"); f.pla(); f.db(0x28); f.rts();
	f.label("reset"); f.sta_abs_x(0x02cc); f.db(0x08); f.pha(); f.lda_imm(1); f.sta_abs(VISIBLE); f.jmp("clear");
	f.label("spawn"); f.db(0x08); f.pha(); abs(f,0xec,SLOT); f.bne("spawn_store");
	f.lda_imm(0); f.sta_abs(TTL); f.label("spawn_store"); f.pla(); f.db(0x28); f.sta_abs_x(0x0344); f.rts();
	f.label("main"); f.jsr(0xe016); f.db(0x08); f.pha(); f.txa(); f.pha(); f.tya(); f.pha();
	f.jsr(far_call); raw(f,{9,0xff,0x7f});
	f.pla(); f.tay(); f.pla(); f.tax(); f.pla(); f.db(0x28); f.rts();
	space(rom,9,BODY,body.size()); space(rom,15,cpu_addr,f.size());
	struct Hook { byte bank; word at; const char* label; };
	for (const Hook hook : {Hook{14,0x8897,"hit"},Hook{14,0x8203,"hit"},Hook{15,0xc134,"reset"},
		Hook{15,0xc238,"spawn"},Hook{15,0xdb6e,"main"}}) {
		klib::Asm6502 code; code.jsr(static_cast<word>(cpu_addr+f.label_position(hook.label)));
		code.apply_hack_and_clear(rom,hook.bank,hook.at);
	}
	body.apply_hack_and_clear(rom,9,BODY);
	return f.apply_hack_and_clear_get_next_cpu_addr(rom,15,cpu_addr);
}
