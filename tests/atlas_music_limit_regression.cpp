#include "fe/Config.h"
#include "fh/HackManager.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<byte>;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::size_t file(word cpu) { return 0x28010 + cpu; }
word read(const Bytes& rom, std::size_t at) { return rom.at(at) | (rom.at(at + 1) << 8); }
void put(Bytes& rom, word cpu, word value) { rom.at(file(cpu)) = value & 255; rom.at(file(cpu) + 1) = value >> 8; }
struct Fixture {
    std::filesystem::path dir = std::filesystem::current_path() / ("music-limit-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Fixture() { require(std::filesystem::create_directory(dir), "fixture directory"); }
    ~Fixture() { std::error_code ec; std::filesystem::remove_all(dir, ec); }
    fe::Config config(const Bytes& rom, std::optional<unsigned> limit, bool omit_default, std::size_t table) {
        const auto path = dir / "override.xml";
        std::ofstream out(path);
        out << "<eoe_config><consts>";
        if (limit) out << "<const name=\"hack_music_max_id\" value=\"" << *limit << "\"/>";
        out << "</consts><pointers><pointer name=\"music_ptr\" value=\"" << table
            << "\" zero_addr=\"49168\"/></pointers></eoe_config>"; out.close();
        std::string base = EOE_TEST_CONFIG_PATH;
        if (omit_default) {
            std::ifstream in(base); std::string text((std::istreambuf_iterator<char>(in)), {});
            const std::string line = "<const name=\"hack_music_max_id\" value=\"0\" />";
            const auto pos = text.find(line); require(pos != std::string::npos, "default config entry");
            text.erase(pos, line.size()); base = (dir / "base.xml").string();
            std::ofstream(base) << text;
        }
        return fe::Config(base, path.string(), rom, "us");
    }
};
struct Image { Bytes rom; std::array<word, 2> entries; word next, yes, no; };
Image install(Fixture& fixture, unsigned songs, std::optional<unsigned> limit = {},
    bool omit_default = false, std::size_t table = 0x14f0b) {
    Bytes rom(0x40010, 0xff); put(rom, 0x8277, 0x8f00); put(rom, 0x8273, 0x8f10);
    // Authored song table, four channels per song and the usual padding byte.
    if (table + 8 * songs + 3 < rom.size()) {
        const word target = static_cast<word>(table - 0xc010 + 8 * songs + 1);
        for (unsigned channel = 0; channel < songs * 4; ++channel) {
            rom.at(table + channel * 2) = target & 255;
            rom.at(table + channel * 2 + 1) = target >> 8;
        }
        if (songs == 0) { rom.at(table) = 0; rom.at(table + 1) = 0; }
        rom.at(table + 8 * songs + 1) = 0;
        rom.at(table + 8 * songs + 2) = 0;
    }
    const auto cfg = fixture.config(rom, limit, omit_default, table);
    fh::HackManager{}.apply_script_library(cfg, rom, file(0xad00),
        {fh::HackLib::AtlasDevSetMusic, fh::HackLib::AtlasDevIfMusic}, 2);
    const auto lo = read(rom, file(0x8277)), hi = read(rom, file(0x8273));
    std::array<word, 2> entries{};
    for (unsigned i = 0; i < 2; ++i) entries[i] = 1 + rom.at(file(lo) + i + 2) + (rom.at(file(hi) + i + 2) << 8);
    return {std::move(rom), entries, static_cast<word>(cfg.constant("rom_iscripts_invokenextaction")),
        static_cast<word>(cfg.constant("rom_iscripts_jumptonextaddr")),
        static_cast<word>(cfg.constant("rom_iscripts_skipaddrandinvoke"))};
}
// Execute the emitted instructions after IScripts_LoadByte returns the operand.
word execute(const Image& image, unsigned index, byte operand, byte& music) {
    word pc = image.entries[index];
    require(image.rom.at(file(pc)) == 0x20, "handler loads operand with JSR");
    pc += 3; byte a = operand; bool carry = false, zero = false;
    auto get = [&]() { return image.rom.at(file(pc++)); };
    for (unsigned steps = 0; steps < 20; ++steps) {
        switch (get()) {
        case 0xc9: { auto value = get(); carry = a >= value; zero = a == value; break; }
        case 0xb0: { auto offset = static_cast<std::int8_t>(get()); if (carry) pc += offset; break; }
        case 0xf0: { auto offset = static_cast<std::int8_t>(get()); if (zero) pc += offset; break; }
        case 0xc5: require(get() == 0xfa, "comparison reads Music_Current"); zero = a == music; carry = a >= music; break;
        case 0x09: a |= get(); zero = a == 0; break;
        case 0x85: require(get() == 0xfa, "only Music_Current written"); music = a; break;
        case 0x4c: { const auto lo = get(); return lo | (get() << 8); }
        default: throw std::runtime_error("unexpected generated instruction");
        }
    }
    throw std::runtime_error("handler did not finish");
}
}
int main() {
    try {
        Fixture fixture;
        const auto baseline = install(fixture, 16);
        const auto fallback = install(fixture, 16, {}, true);
        require(baseline.rom == fallback.rom, "old configs preserve default output");
        const Bytes expected{0x20,0xa4,0x87,0xc9,0x11,0xb0,0x02,0x85,0xfa,0x4c,0x6e,0x82};
        require(std::equal(expected.begin(), expected.end(), baseline.rom.begin() + file(baseline.entries[0])), "vanilla SetMusic bytes unchanged");
        const Bytes expected_if{0x20,0xa4,0x87,0xc9,0x11,0xb0,0x0a,0xc5,0xfa,0xf0,0x09,0x09,0x80,0xc5,0xfa,0xf0,0x03,0x4c,0x16,0x86,0x4c,0x03,0x86};
        require(std::equal(expected_if.begin(), expected_if.end(), baseline.rom.begin() + file(baseline.entries[1])), "vanilla IfMusic bytes unchanged");
        for (unsigned limit : {1U,16U,18U,31U}) {
            const auto image = install(fixture, limit);
            require(image.entries == baseline.entries, "handler sizes unchanged");
            for (unsigned operand = 0; operand < 256; ++operand)
                for (unsigned state = 0; state < 256; ++state) {
                    byte music = state;
                    require(execute(image, 0, operand, music) == image.next, "SetMusic continuation");
                    require(music == (operand <= limit ? operand : state), "SetMusic range behavior");
                    music = state;
                    const bool equal = operand <= limit && (state == operand || state == (operand | 128));
                    require(execute(image, 1, operand, music) == (equal ? image.yes : image.no), "IfMusic pending/promoted range behavior");
                    require(music == state, "IfMusic is read-only");
                }
        }
        const auto explicit_auto = install(fixture, 18, 0);
        const auto expanded = install(fixture, 18);
        require(explicit_auto.rom == expanded.rom, "zero and omitted setting both detect the song count");
        const auto relocated = install(fixture, 18, {}, false, 0x15000);
        byte music = 0;
        require(execute(relocated, 0, 18, music) == relocated.next && music == 18,
            "automatic detection honors the configured table location");
        const auto override = install(fixture, 16, 18);
        music = 0;
        require(execute(override, 0, 18, music) == override.next && music == 18,
            "explicit ceiling overrides a different detected count");
        const auto custom = install(fixture, 0, 18, false, 0x50000);
        music = 0;
        require(execute(custom, 0, 18, music) == custom.next && music == 18,
            "explicit ceiling bypasses unreadable custom table layout");
        music = 0x92;
        require(execute(custom, 1, 18, music) == custom.yes,
            "custom-layout override supports promoted IfMusic");
        for (unsigned songs : {0U,32U}) {
            bool rejected = false;
            try { install(fixture, songs); }
            catch (const std::runtime_error& error) { rejected = std::string(error.what()).find("Music opcode ceiling must be between 1 and 31") != std::string::npos; }
            require(rejected, "invalid detected count rejected");
        }
        for (unsigned limit : {32U,128U,256U}) {
            bool rejected = false;
            try { install(fixture, 16, limit); }
            catch (const std::runtime_error& error) { rejected = std::string(error.what()).find("Music opcode ceiling must be between 1 and 31") != std::string::npos; }
            require(rejected, "invalid explicit ceiling rejected before byte narrowing");
        }
        std::cout << "music limit: automatic/explicit ceilings, 524288 generated-handler executions, and invalid counts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
