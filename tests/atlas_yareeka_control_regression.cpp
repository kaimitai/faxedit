#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// pins install_AtlasDevYareekaControl to known bytes for the default, the
// slowest dash, the fastest and longest dash, the shortest dash and a flag,
// and checks that it installs next to bihoruda control
namespace {
	constexpr word ORG{ 0xbdb5 };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, BIHORUDA_SIZE{ 9 };

	std::vector<byte> from_hex(const std::string& p_hex) {
		std::vector<byte> out;
		for (std::size_t i{ 0 }; i + 1 < p_hex.size(); i += 2)
			out.push_back(static_cast<byte>(std::stoi(p_hex.substr(i, 2), nullptr, 16)));
		return out;
	}

	// yareeka control's sites
	const std::string INIT_HEX{ "2085a8d00ba9009dec029de4022094a8" };
	const std::string MACHINE_HEX{ "bce402c002f03188f013209395bdec02c9409008fee402a9409dec0260a9028d7503a9008d7403"
		"201984deec02d008fee402a9409dec0260209395bdec02c9809008a9009de4029dec0260" };
	const std::string RAMP_HEX{ "bdec02feec02a00220e183a00320c183ad750329018d75034c1984" };
	const std::string MOVE_X_HEX{ "2094849008bddc0249019ddc0260" };
	// bihoruda control's sites, for the pair
	const std::string BI_INIT_HEX{ "2085a8d00da9009dec02a9409df4022094a8" };
	const std::string BI_SITE_HEX{ "bdec02" };
	const std::string BI_BODY_HEX{ "a00220e183a00320c183ad750329018d7503201984feec02bdf402a00220e183a00320d183ad770329018d7703206485fef40260" };
	const std::string HELPERS_HEX{ "8d7403a9000e74032a88d0f98d7503608d7603a9000e76032a88d0f98d7703604839f78385006839ff83f007b9f78338e50060a50060ff7f3f1f0f0703010080402010080402" };
	const std::string MOVE_Y_HEX{ "20ca852075859008bddc0249809ddc0260" };

	// yareeka with a flag and a longer dash (which needs the hooks), placed after bihoruda with a half-pixel
	// peak on a 256-frame loop
	const std::string PAIR_CODE{ "ad02012910f004a964d002a9409dec0260" };

	struct Region { byte bank; word cpu; const char* hex; };
	struct Shape { const char* spec; std::size_t size; std::vector<Region> regions; };
	// each shape: the free space it takes and every byte it writes
	const std::vector<Shape> SHAPES{
		{ "AtlasDevYareekaControl", 0, {
		} },
		{ "AtlasDevYareekaControl dash=1", 0, {
			{ 14, 0x9566, "00" },
			{ 14, 0x956b, "20" },
		} },
		{ "AtlasDevYareekaControl dash=64 dashlen=255", 0, {
			{ 14, 0x9560, "ff" },
			{ 14, 0x9566, "08" },
		} },
		{ "AtlasDevYareekaControl flag=12", 0, {
		} },
		{ "AtlasDevYareekaControl dash=64 dashlen=255 flag=12", 37, {
			{ 14, 0x955f, "4cb5bd" },
			{ 14, 0x956c, "4cc6bd" },
			{ 14, 0xbdb5, "ad02012910f004a9" },
			{ 14, 0xbdbe, "d002a9409dec0260ad02012910f007a9088d7503a9008d74034c6f95" },
		} },
		{ "AtlasDevYareekaControl dashlen=255 flag=12", 17, {
			{ 14, 0x955f, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910f004a9" },
			{ 14, 0xbdbe, "d002a9409dec0260" },
		} },
		{ "AtlasDevYareekaControl dash=20 flag=12", 15, {
			{ 14, 0x956c, "4cb5bd" },
			{ 14, 0xbdb5, "ad02012910f002a9808d74034c6f95" },
		} },
		{ "AtlasDevYareekaControl mode=vanilla", 0, {
		} },
	};

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	void put(std::vector<byte>& rom, word cpu, const std::vector<byte>& bytes) {
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
	}

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		put(rom, 0x9538, from_hex(INIT_HEX)); put(rom, 0x9548, from_hex(MACHINE_HEX)); put(rom, 0x9593, from_hex(RAMP_HEX));
		put(rom, 0x8419, from_hex(MOVE_X_HEX));
		put(rom, 0x8ecf, from_hex(BI_INIT_HEX)); put(rom, 0x8ee1, from_hex(BI_SITE_HEX)); put(rom, 0x8ee4, from_hex(BI_BODY_HEX));
		put(rom, 0x83c1, from_hex(HELPERS_HEX)); put(rom, 0x8564, from_hex(MOVE_Y_HEX));
		return rom;
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		const auto hacks{ fh::filter_general_hacks(14, fh::parse_general_hacks(spec)) };
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 14, ORG, 0xc000, hacks, nullptr);
	}

	std::string hex_at(const std::vector<byte>& rom, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(14, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) { s += d[rom[off + i] >> 4]; s += d[rom[off + i] & 15]; }
		return s;
	}

	// what must stay as it was around the two sites: the machine outside the
	// six retargeted bytes ($955f-$9561 and $9565-$9567), the setup, the ramp
	// and the mover
	void require_untouched(const std::vector<byte>& rom, const std::string& spec) {
		require(hex_at(rom, 0x9538, INIT_HEX.size() / 2) == INIT_HEX, spec + ": the setup must stay");
		require(hex_at(rom, 0x9548, 0x17) == MACHINE_HEX.substr(0, 0x2e), spec + ": the machine before the first site must stay");
		require(hex_at(rom, 0x9562, 0x0a) == MACHINE_HEX.substr(0x34, 0x14), spec + ": the machine between the sites must stay");
		require(hex_at(rom, 0x956f, 0x24) == MACHINE_HEX.substr(0x4e), spec + ": the machine after the second site must stay");
		require(hex_at(rom, 0x9593, RAMP_HEX.size() / 2) == RAMP_HEX, spec + ": the speed-up and slow-down must stay");
		require(hex_at(rom, 0x8419, MOVE_X_HEX.size() / 2) == MOVE_X_HEX, spec + ": the sideways mover must stay");
	}
}

int main() {
	try {
		for (const auto& shape : SHAPES) {
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, shape.spec) };
			const std::string spec{ shape.spec };
			require(n == shape.size, spec + ": size " + std::to_string(n));
			auto want{ vanilla_rom() };
			for (const auto& r : shape.regions) {
				const auto bytes{ from_hex(r.hex) };
				const auto off{ klib::Asm6502::get_file_offset(r.bank, r.cpu) };
				for (std::size_t i{ 0 }; i < bytes.size(); ++i) want[off + i] = bytes[i];
			}
			for (std::size_t i{ 0 }; i < rom.size(); ++i)
				require(rom[i] == want[i], spec + ": file offset " + std::to_string(i) + " differs from the expected install");
		}
		{
			auto rom{ vanilla_rom() };
			const std::string spec{ "AtlasDevBihorudaControl xspeed=4 xloop=256\nAtlasDevYareekaControl dashlen=100 flag=12" };
			const auto n{ install(rom, spec) };
			require(n == BIHORUDA_SIZE + PAIR_CODE.size() / 2, "the pair: size " + std::to_string(n));
			require(hex_at(rom, 0x8eec, 2) == "b5bd", "the pair: bihoruda's scaler call " + hex_at(rom, 0x8eec, 2));
			require(hex_at(rom, 0x955f, 3) == "4cbebd", "the pair: yareeka's site " + hex_at(rom, 0x955f, 3));
			require(hex_at(rom, 0xbdbe, PAIR_CODE.size() / 2) == PAIR_CODE, "the pair: yareeka's code differs");
			require_untouched(rom, spec);
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevYareekaControl mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevYareekaControl dash=0", "AtlasDevYareekaControl dash=65",
				"AtlasDevYareekaControl dashlen=0", "AtlasDevYareekaControl dashlen=256", "AtlasDevYareekaControl flag=248",
				"AtlasDevYareekaControl mode=on", "AtlasDevYareekaControl speed=3",
				"AtlasDevYareekaControl mode=vanilla dash=0" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(14, 0x9595)] ^= 0x01;
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevYareekaControl"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "a non vanilla speed-up routine is refused and nothing is written");
		}
		{
			auto rom{ vanilla_rom() };
			put(rom, 0x955f, { 0x4c, 0x00, 0x90 });   // something else owns the site
			bool threw{ false };
			try { install(rom, "AtlasDevYareekaControl"); } catch (const std::exception&) { threw = true; }
			require(threw, "a site patched by something else is refused");
		}
		std::cout << "atlas_yareeka_control_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_yareeka_control_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
