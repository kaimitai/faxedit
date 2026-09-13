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

// sugata control: tunes sugata's curse. sugata walks, and once a cycle it
// turns the screen gray and then takes 10 hp from you wherever you stand;
// this hack sets how much hp the curse takes and how many frames the screen
// stays gray before it lands. the defaults are the stock numbers, so the hack
// alone changes nothing until a value is set. its walk stays stock.
//
// two bank 14 instructions are retargeted, where the curse's wait is set and
// where its damage is set; the new code goes in bank 15, which is always
// mapped. a clear flag, or mode=vanilla, is the stock routine. every site is
// checked against its vanilla bytes first, and they are identical in the us,
// us rev a, eu and jp roms. no ram is claimed.
//
// without a flag the new numbers are written straight into those stock
// instructions and no free space is used; with a flag only the ones whose
// values differ from stock are retargeted. at the stock values nothing is
// written.
namespace {
	constexpr byte BANK14{ 14 }, BANK15{ 15 };
	constexpr word TIMER{ 0x02ec }, DMG_CELL{ 0x04bd };
	constexpr word SITE_FLASH{ 0xab2b }, SITE_DAMAGE{ 0xab58 };
	constexpr word V_FLASH{ 0xab30 }, V_DAMAGE{ 0xab5d };
	constexpr word FLAG_BASE{ 0x0101 };
	constexpr int MAX_FLAG{ 247 };

	// the bytes each hook replaces or jumps back into
	constexpr std::array<byte, 7> FLASH_ORIG{ 0xa9, 0x02, 0x9d, 0xec, 0x02, 0xa5, 0x0b };
	constexpr std::array<byte, 8> DAMAGE_ORIG{ 0xa9, 0x0a, 0x8d, 0xbd, 0x04, 0x20, 0x8e, 0xc0 };

	template <std::size_t N>
	void require_site(const std::vector<byte>& p_rom, word p_addr, const std::array<byte, N>& p_orig,
		const std::string& p_name) {
		const auto off{ klib::Asm6502::get_file_offset(BANK14, p_addr) };
		for (std::size_t i{ 0 }; i < N; ++i)
			if (off + i >= p_rom.size() || p_rom[off + i] != p_orig[i])
				throw std::runtime_error(std::format("{}: the vanilla site at ${:04x} is not intact", p_name, p_addr));
	}
}

word fh::HackManager::install_AtlasDevSugataControl(const fe::Config&, std::vector<byte>& p_rom, word cpu_addr,
	const fh::GeneralHack& p_hack) const {
	const std::string name{ "AtlasDevSugataControl" };
	// every parameter is judged in every mode, vanilla included
	std::string mode{ p_hack.string_or("mode", "") };
	std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	if (p_hack.has_param("mode") && mode != "vanilla")
		throw std::runtime_error(name + ": mode must be vanilla");
	auto get = [&](const char* p_id, int p_default) {
		return p_hack.has_param(p_id) ? static_cast<int>(p_hack.word_or(p_id, 0)) : p_default;
	};
	const int damage{ get("damage", 10) }, flash{ get("flash", 2) };
	if (damage < 0 || damage > 255)
		throw std::runtime_error(name + ": damage must be 0 to 255");
	if (flash < 1 || flash > 255)
		throw std::runtime_error(name + ": flash must be 1 to 255");
	int flag{ -1 };
	if (p_hack.has_param("flag")) {
		flag = get("flag", 0);
		if (flag > MAX_FLAG)
			throw std::runtime_error(std::format("{}: flag must be 0 to {}", name, MAX_FLAG));
	}
	if (mode == "vanilla")
		return cpu_addr;

	// ownership checks first, so a refused install leaves the rom byte identical
	require_site(p_rom, SITE_FLASH, FLASH_ORIG, name);
	require_site(p_rom, SITE_DAMAGE, DAMAGE_ORIG, name);

	// with a flag, only the values that differ from stock need a hook
	const bool flash_hook{ flag >= 0 && flash != 2 }, damage_hook{ flag >= 0 && damage != 10 };
	klib::Asm6502 code;
	// the value the store takes: the tuned one with the flag set, the stock one with it clear. the load
	// sets Z, so the branch after it is always taken, and one store serves both paths
	auto select = [&](const std::string& p_name, byte p_value, byte p_stock) {
		code.lda_abs(static_cast<word>(FLAG_BASE + (flag >> 3))); code.and_imm(static_cast<byte>(1 << (flag & 7)));
		code.beq("@v" + p_name);
		code.lda_imm(p_value);
		if (p_value != 0) code.bne("@s" + p_name); else code.beq("@s" + p_name);
		code.label("@v" + p_name); code.lda_imm(p_stock);
		code.label("@s" + p_name);
	};
	word flash_addr{ 0 }, damage_addr{ 0 };
	if (flash_hook) {
		flash_addr = static_cast<word>(cpu_addr + code.size());
		select("flash", static_cast<byte>(flash), 0x02); code.sta_abs_x(TIMER); code.jmp(V_FLASH);
	}
	if (damage_hook) {
		damage_addr = static_cast<word>(cpu_addr + code.size());
		select("damage", static_cast<byte>(damage), 0x0a); code.sta_abs(DMG_CELL); code.jmp(V_DAMAGE);
	}
	const std::size_t size{ code.size() };
	const auto off{ klib::Asm6502::get_file_offset(BANK15, cpu_addr) };
	for (std::size_t i{ 0 }; i < size; ++i)
		if (off + i >= p_rom.size() || p_rom[off + i] != 0xff)
			throw std::runtime_error(std::format("{}: bank 15 space at ${:04x} is not free", name, cpu_addr + i));
	// without a flag the new numbers go straight into the stock instructions, and no free space is used
	if (flag < 0) {
		auto at = [&](word p_addr) -> byte& { return p_rom[klib::Asm6502::get_file_offset(BANK14, p_addr)]; };
		at(0xab2c) = static_cast<byte>(flash); at(0xab59) = static_cast<byte>(damage);
	}
	if (size == 0)
		return cpu_addr;
	code.apply_hack_and_clear(p_rom, BANK15, cpu_addr);
	if (flash_hook) { code.jmp(flash_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_FLASH); }
	if (damage_hook) { code.jmp(damage_addr); code.apply_hack_and_clear(p_rom, BANK14, SITE_DAMAGE); }
	return static_cast<word>(cpu_addr + size);
}
