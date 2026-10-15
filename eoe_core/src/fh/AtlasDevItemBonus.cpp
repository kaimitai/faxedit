#include "HackManager.h"
#include "fh_constants.h"
#include "fe/Config.h"
#include "common/klib/Asm6502.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

// item bonus: the equipped weapon, armor and shield each add to the hero's
// melee damage and take from an enemy's contact damage, from a table of four
// values per slot indexed by the equipment id. ids 4 and up, $ff for nothing
// equipped included, add nothing.
//
// two bank 14 sites are retargeted. the melee hit loads the enemy's hp at
// $8891 before taking the damage in $00 from it; the attack side adds its
// bonuses to $00 there, after the pendant and the glove, saturating at 255,
// and replays the load. the contact hit jumps to Player_ReduceHP at $8ad4
// with the damage staged; the defense side takes its bonuses from the whole
// byte there, after the armor and the black onyx, and a result below zero
// clears the fraction too, so the hit does no damage. knockback and mercy
// time are left as they are.
//
// each side loads once, applies its tables and stores once. y is the live
// weapon class when the melee hit is reached, and is given back by loading
// it again; after the contact hit the hud tail sets y before reading it in
// every region, so that side leaves y as it is. no ram is claimed.
namespace {
	constexpr byte BANK{ 14 };
	constexpr int MAX_FLAG{ 247 };
	constexpr byte ID_COUNT{ 4 }, ZP_DAMAGE{ 0x00 };
	constexpr word ENEMY_HP{ 0x0344 };
	constexpr word SITE_ATTACK{ 0x8891 }, SITE_DEFENSE{ 0x8ad4 };

	// the attack site with the subtraction the replayed load returns to
	constexpr std::array<byte, 6> ATTACK_ORIG{ 0xbd, 0x44, 0x03, 0x38, 0xe5, 0x00 };
	constexpr std::array<byte, 3> DEFENSE_ORIG{ 0x4c,
		static_cast<byte>(fh::ROM::Player_ReduceHP & 0xff), static_cast<byte>(fh::ROM::Player_ReduceHP >> 8) };

	struct Table {
		const char* name;
		word slot;
		bool attack;
		std::array<byte, ID_COUNT> values{};
		bool active() const { return std::any_of(values.begin(), values.end(), [](byte v) { return v != 0; }); }
	};

	template<std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("AtlasDevItemBonus: the vanilla site at ${:04x} is not intact", p_addr));
	}

	// a clear flag branches to p_off
	void flag_test(klib::Asm6502& c, int p_flag, const std::string& p_off) {
		c.lda_abs(static_cast<word>(fh::RAM::Flags + (p_flag >> 3)));
		c.and_imm(static_cast<byte>(1 << (p_flag & 7)));
		c.beq(p_off);
	}

	// one side: a holds the value, every active table of the side is applied
	// in turn, and the first carry out of range goes to @<side>_limit
	void emit_side(klib::Asm6502& c, const std::array<Table, 6>& p_tables, bool p_attack) {
		const std::string side{ p_attack ? "@attack" : "@defense" };
		std::vector<const Table*> used;
		for (const Table& t : p_tables)
			if (t.attack == p_attack && t.active()) used.push_back(&t);
		for (std::size_t i{ 0 }; i < used.size(); ++i) {
			const bool last{ i + 1 == used.size() };
			const std::string next{ last ? side + "_store" : std::format("{}_{}", side, i) };
			c.ldy_abs(used[i]->slot); c.cpy_imm(ID_COUNT); c.bcs(next);
			if (p_attack) { c.clc(); c.adc_abs_y(std::format("@{}", used[i]->name)); }
			else { c.sec(); c.sbc_abs_y(std::format("@{}", used[i]->name)); }
			// in range goes on; the last table falls into the limit otherwise
			if (last) {
				if (p_attack) c.bcc(next); else c.bcs(next);
			}
			else {
				if (p_attack) c.bcs(side + "_limit"); else c.bcc(side + "_limit");
				c.label(next);
			}
		}
	}
}

word fh::HackManager::install_AtlasDevItemBonus(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) {
	const std::string name{ "AtlasDevItemBonus" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");

	std::array<Table, 6> tables{ {
		{ "weapon_attack", fh::RAM::SelectedWeapon, true }, { "armor_attack", fh::RAM::EquippedArmor, true },
		{ "shield_attack", fh::RAM::EquippedShield, true }, { "weapon_defense", fh::RAM::SelectedWeapon, false },
		{ "armor_defense", fh::RAM::EquippedArmor, false }, { "shield_defense", fh::RAM::EquippedShield, false } } };
	for (Table& t : tables) {
		if (!p_hack.has_param(t.name)) continue;
		const auto values{ p_hack.split_bytes(t.name) };
		if (values.size() != ID_COUNT)
			throw std::runtime_error(std::format("{}: {} is four values 0 to 255 joined with plus signs", name, t.name));
		std::copy(values.begin(), values.end(), t.values.begin());
	}
	const bool attack{ std::any_of(tables.begin(), tables.end(), [](const Table& t) { return t.attack && t.active(); }) };
	const bool defense{ std::any_of(tables.begin(), tables.end(), [](const Table& t) { return !t.attack && t.active(); }) };

	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		const word f{ p_hack.word_or("flag", 0) };
		if (f > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
		if (!attack && !defense)
			throw std::runtime_error(name + ": flag gates the bonuses; give at least one table a value");
		flag = f;
	}
	if (mode == "vanilla" || (!attack && !defense))
		return cpu_addr;
	const bool sram{ p_hack.bool_or("sram", false) };
	if (sram && !sram_state)
		throw std::runtime_error(name + ": sram=true requires a rom with sram support, or the SRAM hack");

	klib::Asm6502 code;
	if (attack) {
		// in place of lda $0344,x, which it replays before returning
		code.label("@attack");
		if (flag >= 0) flag_test(code, flag, "@attack_load");
		code.lda_zp(ZP_DAMAGE);
		emit_side(code, tables, true);
		code.label("@attack_limit"); code.lda_imm(0xff);
		code.label("@attack_store"); code.sta_zp(ZP_DAMAGE);
		code.ldy_abs(fh::RAM::LiveWeapon);
		code.label("@attack_load"); code.lda_abs_x(ENEMY_HP); code.rts();
	}
	if (defense) {
		// in place of jmp Player_ReduceHP, which it goes on to
		code.label("@defense");
		if (flag >= 0) flag_test(code, flag, "@defense_hit");
		code.lda_abs(fh::RAM::DamageAmount);
		emit_side(code, tables, false);
		code.label("@defense_limit"); code.lda_imm(0); code.sta_abs(fh::RAM::DamageAmountFraction);
		code.label("@defense_store"); code.sta_abs(fh::RAM::DamageAmount);
		code.label("@defense_hit"); code.jmp(fh::ROM::Player_ReduceHP);
	}
	for (const Table& t : tables) {
		if (!t.active()) continue;
		code.label(std::format("@{}", t.name));
		for (byte v : t.values) code.db(v);
	}

	// ownership checks first, so a refused install leaves the rom byte identical
	if (attack) require_site(p_rom, SITE_ATTACK, ATTACK_ORIG);
	if (defense) require_site(p_rom, SITE_DEFENSE, DEFENSE_ORIG);
	const std::size_t size{ code.size() };
	if (!sram) {
		const auto off{ klib::Asm6502::get_file_offset(BANK, cpu_addr) };
		for (std::size_t i{ 0 }; i < size; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
				throw std::runtime_error(std::format("{}: bank 14 space at ${:04x} is not free", name, cpu_addr + i));
	}

	// with sram the code runs from the shared sram code area, and bank 14
	// keeps only the two hooks; label addresses are read before the install
	// clears them
	const word body{ sram ? sram_hack_addr() : cpu_addr };
	const word attack_entry{ static_cast<word>(body + (attack ? code.label_position("@attack") : 0)) };
	const word defense_entry{ static_cast<word>(body + (defense ? code.label_position("@defense") : 0)) };
	if (sram) install_sram_hack(p_rom, code);
	else code.apply_hack_and_clear(p_rom, BANK, cpu_addr);

	if (attack) { code.jsr(attack_entry); code.apply_hack_and_clear(p_rom, BANK, SITE_ATTACK); }
	if (defense) { code.jmp(defense_entry); code.apply_hack_and_clear(p_rom, BANK, SITE_DEFENSE); }
	return sram ? cpu_addr : static_cast<word>(cpu_addr + size);
}
