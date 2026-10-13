#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// pins install_AtlasDevSmartMattock to known bodies at $fdbe: both modes,
// mode=press, mode=push push=1, and flag=12 push=255
namespace {
	constexpr word ORG{ 0xfdbe };
	constexpr std::size_t ROM_SIZE{ 0x40010 }, EXPANDED_ROM_SIZE{ 0x80010 };
	constexpr std::array<byte, 3> PRESS_ORIG{ 0xad, 0xc1, 0x03 };
	constexpr std::array<byte, 4> PUSH_ORIG{ 0xa9, 0x00, 0x85, 0xd7 };
	constexpr std::array<byte, 3> SPEND_ORIG{ 0x20, 0xbf, 0xc4 };
	constexpr std::array<byte, 16> TO_BLOCK_ORIG{ 0xa5, 0xb6, 0x29, 0xf0, 0x85, 0x00, 0xa5, 0xb5,
		0x4a, 0x4a, 0x4a, 0x4a, 0x05, 0x00, 0xaa, 0x60 };

	const std::string BODY_BOTH{
		"adc103c909f011aec603e009b037ca3034bdad03c909d0f6a000a5a42940f001c8a59e18798dc6c9f0b01a85b5a5a1"
		"85b6206ce8a5240a0aa8b98fc6f007dd0006d0023860186020befdb00dadc103c909f009adc1034c8ec44c16c660a5"
		"162903f01aa5a42905d01420befd900fe6d7a5d7c930900ba90085d74c16c6a90085d760adc103c909d0034cbfc4ae"
		"c603e009b01eca301bbdad03c909d0f6e8ecc603b00bbdad03ca9dad03e8e8d0f0cec60360" };
	const std::string BODY_PRESS{
		"adc103c909f011aec603e009b037ca3034bdad03c909d0f6a000a5a42940f001c8a59e18798dc6c9f0b01a85b5a5a1"
		"85b6206ce8a5240a0aa8b98fc6f007dd0006d0023860186020befdb00dadc103c909f009adc1034c8ec44c16c660ad"
		"c103c909d0034cbfc4aec603e009b01eca301bbdad03c909d0f6e8ecc603b00bbdad03ca9dad03e8e8d0f0cec60360" };
	const std::string BODY_PUSH1{
		"adc103c909f00318902da000a5a42940f001c8a59e18798dc6c9f0b01a85b5a5a185b6206ce8a5240a0aa8b98fc6f0"
		"07dd0006d0023860186020befdb00dadc103c909f009adc1034c8ec44c16c660a5162903f01aa5a42905d01420befd"
		"900fe6d7a5d7c901900ba90085d74c16c6a90085d760adc103c909d0034cbfc4aec603e009b01eca301bbdad03c909"
		"d0f6e8ecc603b00bbdad03ca9dad03e8e8d0f0cec60360" };
	const std::string BODY_FLAG12{
		"adc103c909f011aec603e009b037ca3034bdad03c909d0f6a000a5a42940f001c8a59e18798dc6c9f0b01a85b5a5a1"
		"85b6206ce8a5240a0aa8b98fc6f007dd0006d00238601860ad02012910f00c20befdb00dadc103c909f009adc1034c"
		"8ec44c16c660ad02012910f020a5162903f01aa5a42905d01420befd900fe6d7a5d7c9ff900ba90085d74c16c6a900"
		"85d760adc103c909d0034cbfc4aec603e009b01eca301bbdad03c909d0f6e8ecc603b00bbdad03ca9dad03e8e8d0f0"
		"cec60360" };

	void require(bool c, const std::string& m) { if (!c) throw std::runtime_error(m); }

	std::vector<byte> vanilla_rom() {
		std::vector<byte> rom(ROM_SIZE, 0xff);
		auto put = [&](word cpu, const auto& bytes) {
			const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
			for (std::size_t i{ 0 }; i < bytes.size(); ++i) rom[off + i] = bytes[i];
		};
		put(0xc48b, PRESS_ORIG); put(0xe9b9, PUSH_ORIG); put(0xc64c, SPEND_ORIG);
		put(0xe86c, TO_BLOCK_ORIG);
		// the rts the push site's counter reset falls through to
		rom[klib::Asm6502::get_file_offset(15, 0xe9bd)] = 0x60;
		return rom;
	}

	std::vector<fh::GeneralHack> hacks(const std::string& text) {
		return fh::filter_general_hacks(15, fh::parse_general_hacks(text));
	}

	std::size_t install(std::vector<byte>& rom, const std::string& spec) {
		return fh::HackManager{}.install_general_hacks(fe::Config{}, rom, 15, ORG, 0xfff0, hacks(spec), nullptr).bank_used;
	}

	fe::Config region_config(const std::vector<byte>& rom, const std::string& region) {
		fe::Config config;
		config.load_definitions(EOE_TEST_CONFIG_PATH, "");
		config.set_region(region);
		config.load_config_data(EOE_TEST_CONFIG_PATH, "", rom);
		return config;
	}

	// the editor's expansion appends banks 16 to 31 and declares 32 banks
	std::vector<byte> expanded(std::vector<byte> rom) {
		rom.resize(EXPANDED_ROM_SIZE, 0xff);
		rom[4] = 0x20;
		return rom;
	}

	std::string hex_at(const std::vector<byte>& rom, word cpu, std::size_t n) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
		for (std::size_t i{ 0 }; i < n; ++i) { s += d[rom[off + i] >> 4]; s += d[rom[off + i] & 15]; }
		return s;
	}

	std::string op_to(byte op, word target) {
		static const char* d{ "0123456789abcdef" };
		std::string s;
		for (byte b : { op, static_cast<byte>(target & 0xff), static_cast<byte>(target >> 8) }) { s += d[b >> 4]; s += d[b & 15]; }
		return s;
	}
}

int main() {
	try {
		{
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, "AtlasDevSmartMattock") };
			require(n == BODY_BOTH.size() / 2, "both: size " + std::to_string(n));
			require(hex_at(rom, ORG, n) == BODY_BOTH, "both: body differs: " + hex_at(rom, ORG, n));
			// press site: JMP @press; spend site: JSR @spend; push site: JMP @push + NOP, RTS kept
			const word press{ static_cast<word>(ORG + BODY_BOTH.find("20befdb00d") / 2) };
			require(hex_at(rom, 0xc48b, 3) == op_to(0x4c, press), "press site: " + hex_at(rom, 0xc48b, 3));
			const word spend{ static_cast<word>(ORG + BODY_BOTH.rfind("adc103c909d0034cbfc4") / 2) };
			require(hex_at(rom, 0xc64c, 3) == op_to(0x20, spend), "spend site: " + hex_at(rom, 0xc64c, 3));
			const word push{ static_cast<word>(ORG + BODY_BOTH.find("a5162903f01a") / 2) };
			require(hex_at(rom, 0xe9b9, 4) == op_to(0x4c, push) + "ea", "push site: " + hex_at(rom, 0xe9b9, 4));
			require(hex_at(rom, 0xe9bd, 1) == "60", "push site: the rts must stay");
		}
		{
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, "AtlasDevSmartMattock mode=press") };
			require(hex_at(rom, ORG, n) == BODY_PRESS, "press: body differs: " + hex_at(rom, ORG, n));
			require(hex_at(rom, 0xe9b9, 4) == "a90085d7", "press: the push site must be untouched");
		}
		{
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, "AtlasDevSmartMattock mode=push push=1") };
			require(hex_at(rom, ORG, n) == BODY_PUSH1, "push1: body differs: " + hex_at(rom, ORG, n));
		}
		{
			auto rom{ vanilla_rom() };
			const auto n{ install(rom, "AtlasDevSmartMattock flag=12 push=255") };
			require(hex_at(rom, ORG, n) == BODY_FLAG12, "flag12: body differs: " + hex_at(rom, ORG, n));
		}
		{
			const auto pristine{ vanilla_rom() };
			auto rom{ vanilla_rom() };
			require(install(rom, "AtlasDevSmartMattock mode=vanilla") == 0, "vanilla installs nothing");
			require(rom == pristine, "vanilla leaves the rom byte identical");
			require(hex_at(rom, 0xc48b, 3) == "adc103", "vanilla leaves the press site");
		}
		{
			const auto pristine{ vanilla_rom() };
			for (const char* bad : { "AtlasDevSmartMattock mode=always", "AtlasDevSmartMattock push=0",
				"AtlasDevSmartMattock push=256", "AtlasDevSmartMattock flag=248",
				"AtlasDevSmartMattock mode=press push=10",
				"AtlasDevSmartMattock mode=vanilla push=10",
				"AtlasDevSmartMattock mode=vanilla flag=248" }) {
				auto rom{ vanilla_rom() };
				bool threw{ false };
				try { install(rom, bad); } catch (const std::exception&) { threw = true; }
				require(threw, std::string("must refuse: ") + bad);
				require(rom == pristine, std::string("refusal must not write: ") + bad);
				require(hex_at(rom, 0xc48b, 3) == "adc103", std::string("refusal must not write: ") + bad);
			}
		}
		{
			auto rom{ vanilla_rom() };
			rom[klib::Asm6502::get_file_offset(15, 0xc64c)] = 0xea;
			bool threw{ false };
			try { install(rom, "AtlasDevSmartMattock"); } catch (const std::exception&) { threw = true; }
			require(threw, "a non vanilla site is refused");
		}
		// sram=true moves the body to the sram code area. bank 15 keeps only
		// the three retargeted instructions and the shared 9-byte trampoline,
		// and the body's call to its own block check goes to the sram copy.
		{
			constexpr word SRAM_CODE{ 0x6806 };
			struct Layout { std::string region; bool expand; byte bank; word addr; };
			for (const Layout& layout : { Layout{ "us", false, 9, 0xa700 }, Layout{ "us-512", true, 27, 0x8000 } })
				for (const std::string spec : { "AtlasDevSmartMattock", "AtlasDevSmartMattock mode=press",
					"AtlasDevSmartMattock flag=12 push=255" }) {
					auto plain{ vanilla_rom() };
					const auto body{ install(plain, spec) };
					auto rom{ layout.expand ? expanded(vanilla_rom()) : vanilla_rom() };
					const auto config{ region_config(rom, layout.region) };
					fh::HackManager manager;
					manager.init_sram_state(config);
					const auto usage{ manager.install_general_hacks(config, rom, 15, ORG, 0xfff0,
						hacks(spec + " sram=true"), nullptr) };
					const std::string what{ layout.region + ": " + spec };
					require(usage.sram_used == body, "sram body size differs: " + what);
					require(usage.bank_used == 9, "sram install kept the body in bank 15: " + what);
					const auto in_sram = [&](word cpu) { return cpu >= SRAM_CODE && cpu < SRAM_CODE + body; };
					const auto operand = [&](word cpu) {
						const auto off{ klib::Asm6502::get_file_offset(15, cpu) };
						return static_cast<word>(rom[off + 1] | (rom[off + 2] << 8));
					};
					require(hex_at(rom, 0xc48b, 1) == "4c" && in_sram(operand(0xc48b)), "press hook: " + what);
					require(hex_at(rom, 0xc64c, 1) == "20" && in_sram(operand(0xc64c)), "spend hook: " + what);
					if (spec.find("press") == std::string::npos)
						require(hex_at(rom, 0xe9b9, 1) == "4c" && in_sram(operand(0xe9b9)), "push hook: " + what);
					// the bank 15 body calls its block check at ORG; the sram copy calls it in sram
					const auto backing{ klib::Asm6502::get_file_offset(layout.bank, layout.addr) };
					const std::vector<byte> image(rom.begin() + backing, rom.begin() + backing + body);
					bool calls_sram{ false }, calls_org{ false };
					for (std::size_t i{ 0 }; i + 2 < image.size(); ++i)
						if (image[i] == 0x20) {
							const word target{ static_cast<word>(image[i + 1] | (image[i + 2] << 8)) };
							calls_sram = calls_sram || in_sram(target);
							calls_org = calls_org || target == ORG;
						}
					require(calls_sram && !calls_org, "the sram copy does not call its own block check: " + what);
				}
			auto rom{ vanilla_rom() };
			const auto before{ rom };
			bool threw{ false };
			try { install(rom, "AtlasDevSmartMattock sram=true"); } catch (const std::exception&) { threw = true; }
			require(threw && rom == before, "accepted sram=true without sram support");
		}
		std::cout << "atlas_smart_mattock_regression: ok\n";
	}
	catch (const std::exception& e) { std::cerr << "atlas_smart_mattock_regression: " << e.what() << "\n"; return 1; }
	return 0;
}
