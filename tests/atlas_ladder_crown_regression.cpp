#include "common/klib/Asm6502.h"
#include "fe/Config.h"
#include "fh/AtlasDevFrameScheduler.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Installation checks do not require a game ROM.
// The optional exporter installs into a supplied ROM; it is not the
// editor's project-save path and does not execute the game.
namespace {

constexpr word ORG{ 0xfcce }, END{ 0xffe0 };
constexpr std::size_t ROM_SIZE{ 0x40010 };
std::size_t checks{ 0 };

void require(bool condition, const std::string& message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

std::vector<byte> unhex(std::string_view text) {
    if (text.size() % 2) throw std::runtime_error("odd fixture hex length");
    const auto nibble = [](char c) -> byte {
        if (c >= '0' && c <= '9') return static_cast<byte>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<byte>(c - 'a' + 10);
        throw std::runtime_error("invalid fixture hex");
    };
    std::vector<byte> result;
    for (std::size_t i{ 0 }; i < text.size(); i += 2)
        result.push_back(static_cast<byte>((nibble(text[i]) << 4) | nibble(text[i + 1])));
    return result;
}

std::size_t offset(word cpu) { return klib::Asm6502::get_file_offset(15, cpu); }

struct Site { word cpu; std::string_view bytes; bool crown_only{ false }; bool floor_only{ false }; };
const std::vector<Site> SITES{
    { 0xe307, "a20220c8e6" }, { 0xe2c8, "a5a5100a" },
    { 0xc145, "206fcd", true },
    { 0xc9af, "a9078d1440", true }, { 0xc9de, "8d0120a55a", true },
    { 0xe8c6, "984aa89008b93c044a4a4a4a60b93c04290f60" },
    { 0xe87c, "bc000620c6e8a8b9d9e860" },
    { 0xe86c, "a5b629f08500a5b54a4a4a4a0500aa60" },
    { 0xe6c8, "8af0c6caf08dcaf053" },
    { 0xcd6f, "a9098599a900859860", true },
    { 0xe30c, "d035e6a3a5a5100a" },
    { 0xe349, "a5a429da85a4a20320c8e6", true },
    { 0xe399, "a5a6c9209006", false, true },
    { 0xe43a, "a5a429fb85a4a90085b1" },
    { 0xe3cd, "a5a51000" }, { 0xe444, "a5a44ab01a" }, { 0xe463, "a6a6e010" },
    { 0xe3d1, "a5a4090485a4" }, { 0xe3f5, "a5a118690885a1" }, { 0xe182, "a5a42905f00f" },
    { 0xe31e, "a5a038e9a085a0" }, { 0xe325, "a5a1e90085a1" },
    { 0xe36c, "a5a01869c085a0" }, { 0xe373, "a5a1690085a1" },
};
const std::array<std::string_view, 2> HEADERS{
    "4e45531a100010000000000000000000", "4e45531a100010080000000700000001"
};

void put(std::vector<byte>& rom, word cpu, std::string_view hex) {
    const auto bytes{ unhex(hex) };
    std::copy(bytes.begin(), bytes.end(), rom.begin() + offset(cpu));
}

std::vector<byte> fixture(std::size_t header = 0) {
    std::vector<byte> rom(ROM_SIZE, 0xff);
    const auto bytes{ unhex(HEADERS.at(header)) };
    std::copy(bytes.begin(), bytes.end(), rom.begin());
    for (const auto& site : SITES) put(rom, site.cpu, site.bytes);
    // Region signature and scheduler preimages permit independent composition
    // refusal tests instead of failing first on unrelated missing fixtures.
    rom[0x3025d] = 0x6b; rom[0x3025e] = 0x9f;
    return rom;
}

fe::Config config(const std::string& region = "us") {
    fe::Config result;
    result.load_definitions(EOE_TEST_CONFIG_PATH, "");
    result.set_region(region);
    return result;
}

std::size_t install(std::vector<byte>& rom, const std::string& spec,
    std::size_t origin = ORG, std::size_t end = END, const std::string& region = "us") {
    const auto all{ fh::parse_general_hacks(spec) };
    const auto selected{ fh::filter_general_hacks(15, all) };
    return fh::HackManager{}.install_general_hacks(config(region), rom, 15, origin, end, selected, nullptr);
}

void refuses(std::vector<byte> rom, const std::string& spec,
    std::size_t origin = ORG, std::size_t end = END, const std::string& region = "us",
    const std::string& diagnostic = "") {
    const auto before{ rom };
    bool threw{ false };
    try { install(rom, spec, origin, end, region); }
    catch (const std::exception& error) {
        threw = true;
        if (!diagnostic.empty())
            require(std::string(error.what()).find(diagnostic) != std::string::npos,
                "wrong refusal reason for " + spec + ": " + error.what());
    }
    require(threw, "unexpected acceptance: " + spec);
    require(rom == before, "refusal changed the ROM: " + spec);
}

word target(const std::vector<byte>& rom, word hook) {
    return static_cast<word>(rom.at(offset(hook) + 1) | (rom.at(offset(hook) + 2) << 8));
}

std::uint64_t fingerprint(const std::vector<byte>& rom, word origin, std::size_t size) {
    std::uint64_t value{ 14695981039346656037ULL };
    for (std::size_t i{ 0 }; i < size; ++i)
        value = (value ^ rom.at(offset(origin) + i)) * 1099511628211ULL;
    return value;
}

void owned_changes(const std::vector<byte>& before, const std::vector<byte>& after,
    word origin, std::size_t size, bool crown) {
    require(before.size() == after.size(), "installer changed ROM length");
    std::size_t changed{ 0 };
    for (std::size_t i{ 0 }; i < before.size(); ++i) {
        if (before[i] == after[i]) continue;
        ++changed;
        const bool body{ i >= offset(origin) && i < offset(origin) + size };
        const bool ascent{ i >= offset(0xe307) && i < offset(0xe307) + 5 };
        const bool entry{ i >= offset(0xe2c8) && i < offset(0xe2c8) + 4 };
        const bool reset{ crown && i >= offset(0xc145) && i < offset(0xc145) + 3 };
        require(body || ascent || entry || reset, "write outside Crown-owned ranges at " + std::to_string(i));
    }
    require(changed > 0, "enabled installer changed nothing");
    for (word hook : { word{ 0xe307 }, word{ 0xe2c8 } }) {
        require(after.at(offset(hook)) == 0x4c, "movement hook is not JMP");
        require(target(after, hook) >= origin && target(after, hook) < origin + size, "hook target outside body");
    }
    require(after.at(offset(0xe30a)) == 0xea && after.at(offset(0xe30b)) == 0xea,
        "ascent hook padding");
    require(after.at(offset(0xe2cb)) == 0xea, "entry hook padding");
    if (crown) {
        require(after.at(offset(0xc145)) == 0x20, "reset hook is not JSR");
        require(target(after, 0xc145) >= origin && target(after, 0xc145) < origin + size,
            "reset target outside body");
    }
}

void test_defaults_and_relocation() {
    for (std::size_t header{ 0 }; header < HEADERS.size(); ++header) {
        for (bool crown : { true, false }) {
            const std::string spec{ crown ? "AtlasDevLadderCrown" : "AtlasDevLadderCrown mode=floor" };
            const std::size_t expected_size{ crown ? 605U : 315U };
            for (word origin : { word{0xc400}, word{0xf100}, word{ORG - 1}, ORG,
                    word{ORG + 1}, static_cast<word>(END - expected_size) }) {
                auto rom{ fixture(header) }; const auto before{ rom };
                const auto size{ install(rom, spec, origin) };
                require(size == expected_size, "default body size");
                owned_changes(before, rom, origin, size, crown);
                require(target(rom, 0xe307) == origin, "ascent entry location");
                require(target(rom, 0xe2c8) == origin + (crown ? 152 : 110), "guard entry location");
                if (crown) require(target(rom, 0xc145) == origin + 597, "reset wrapper location");
                if (origin == ORG) {
                    // Keep the original fixed emitter oracle, normalizing
                    // only Crown's relocated latch operands back to $04e3.
                    // This is not a cryptographic identity or ROM data.
                    auto normalized{rom};
                    if (crown) {
                        const auto reset{unhex("a9008de0044c6fcd")};
                        require(std::equal(reset.begin(), reset.end(), rom.begin() + offset(ORG) + 597),
                            "room initialization must clear Crown's own $04e0 latch");
                        for (std::size_t at{offset(origin)}; at + 2 < offset(origin) + size; ++at)
                            if (rom[at] == 0xad || rom[at] == 0x8d || rom[at] == 0xcd) {
                                require(rom[at + 1] != 0xe3 || rom[at + 2] != 4,
                                    "Crown must not access DayNight's former latch cell");
                                if (rom[at + 1] == 0xe0 && rom[at + 2] == 4)
                                    normalized[at + 1] = 0xe3;
                            }
                    }
                    require(fingerprint(normalized, origin, size) == (crown
                        ? 0x2bcd34f40eb56a4cULL : 0xe6775c20867cfa53ULL), "default body fingerprint");
                }
                auto repeated{ before };
                require(install(repeated, spec, origin) == size && repeated == rom, "deterministic emission");
            }
            auto exact{ fixture(header) };
            require(install(exact, spec, ORG, static_cast<word>(ORG + expected_size)) == expected_size,
                "exact capacity should fit");
            refuses(fixture(header), spec, ORG, static_cast<word>(ORG + expected_size - 1));
        }
    }
    for (const std::string spec : { "", "AtlasDevLadderCrown mode=vanilla" }) {
        auto rom{ fixture() }; const auto before{ rom };
        require(install(rom, spec) == 0 && rom == before, "disabled build must be byte-identical");
    }
    std::vector<byte> empty;
    require(install(empty, "AtlasDevLadderCrown mode=vanilla", 0xc000, 0xc000, "eu") == 0 && empty.empty(),
        "vanilla mode must not inspect or allocate a body");
}

void test_options() {
    for (int hold{ 0 }; hold <= 8; ++hold)
        for (int align{ 0 }; align <= 2; ++align) {
            const auto spec{ "AtlasDevLadderCrown downhold=" + std::to_string(hold) + " align=" + std::to_string(align) };
            auto rom{ fixture() }; const auto before{ rom };
            const auto size{ install(rom, spec) };
            require(size > 0 && size <= 713, "assistance body bound");
            owned_changes(before, rom, ORG, size, true);
        }
    for (const std::string mode : { "crown", "floor" })
        for (const std::string policy : { "allow", "deny" }) {
            const auto prefix{ "AtlasDevLadderCrown mode=" + mode + " roompolicy=" + policy + " rooms=" };
            auto first{ fixture() }, second{ fixture() };
            const auto before{ first };
            const auto size{ install(first, prefix + "255:255+6:3+0:0+3:12+1:2+8:7+4:5+2:1") };
            require(install(second, prefix + "0:0+1:2+2:1+3:12+4:5+6:3+8:7+255:255") == size && first == second,
                "room ordering must be canonical");
            owned_changes(before, first, ORG, size, mode == "crown");
        }
    auto maximum{ fixture() }; const auto before{ maximum };
    const auto size{ install(maximum, "AtlasDevLadderCrown downhold=8 align=2 roompolicy=deny rooms=0:0+1:1+2:2+3:3+4:4+5:5+6:3+255:255") };
    require(size == 713, "maximum supported body size");
    owned_changes(before, maximum, ORG, size, true);
    auto numeric{ fixture() }, ordinary{ fixture() };
    install(numeric, "atlasdevladdercrown downhold=$08 align=%10 roompolicy=allow rooms=0x06:$03");
    install(ordinary, "AtlasDevLadderCrown downhold=8 align=2 roompolicy=allow rooms=6:3");
    require(numeric == ordinary, "public parser numeric forms");
    for (const auto& speeds : { std::array<byte, 4>{ 1, 0, 1, 0 }, std::array<byte, 4>{ 0, 8, 0, 8 } }) {
        auto rom{ fixture() };
        rom[offset(0xe322)] = speeds[0]; rom[offset(0xe328)] = speeds[1];
        rom[offset(0xe370)] = speeds[2]; rom[offset(0xe376)] = speeds[3];
        const auto original{ rom }; const auto used{ install(rom, "AtlasDevLadderCrown") };
        owned_changes(original, rom, ORG, used, true);
    }
}

void test_invalid_settings() {
    for (const std::string option : {
        "mode=bad", "mode=Crown", "downhold=-1", "downhold=9", "downhold=256", "downhold=1.0",
        "downhold=true", "align=-1", "align=3", "align=256", "align=2junk", "roompolicy=none",
        "roompolicy=Allow", "roompolicy=allow", "roompolicy=deny", "rooms=6:3",
        "roompolicy=all rooms=6:3", "roompolicy=allow rooms=6", "roompolicy=allow rooms=6:3:2",
        "roompolicy=allow rooms=:3", "roompolicy=allow rooms=6:", "roompolicy=allow rooms=-1:3",
        "roompolicy=allow rooms=256:3", "roompolicy=allow rooms=6:256", "roompolicy=allow rooms=6:3junk",
        "roompolicy=allow rooms=6:3+6:3", "roompolicy=allow rooms=0:0+1:1+2:2+3:3+4:4+5:5+6:6+7:7+8:8",
        "roompolicy=allow rooms=+6:3", "roompolicy=allow rooms=6:3+", "roompolicy=allow rooms=6:3++7:4",
        "mode=floor downhold=1", "mode=floor align=1", "mode=vanilla downhold=1", "mode=vanilla align=1",
        "mode=vanilla roompolicy=allow rooms=6:3", "flag=0", "activation=off", "up=320", "attack=1",
        "downhold=1 downhold=2", "downhold=1 DOWNHOLD=2", "align=", "=2", "downhold", "align 2" })
        refuses(fixture(), "AtlasDevLadderCrown " + option);
}

void test_structural_headers() {
    const auto accepts = [](const std::vector<byte>& source, const std::string& region = "us") {
        for (const bool crown : {true, false}) {
            auto rom{source};
            const auto size{install(rom, crown ? "AtlasDevLadderCrown" : "AtlasDevLadderCrown mode=floor",
                ORG, END, region)};
            require(size == (crown ? 605 : 315), "compatible header or region changed the body size");
            require(std::equal(source.begin(), source.begin() + 16, rom.begin()), "accepted header must be preserved");
            owned_changes(source, rom, ORG, size, crown);
        }
    };
    for (std::size_t header{}; header < HEADERS.size(); ++header) {
        for (const byte flags : {byte{0x10}, byte{0x11}, byte{0x12}, byte{0x13}}) {
            auto rom{fixture(header)}; rom[6] = flags;
            accepts(rom);
        }
        for (std::size_t index{8}; index < 16; ++index)
            for (const byte metadata : {byte{1}, byte{0x7f}, byte{0xff}}) {
                auto rom{fixture(header)};
                if (header == 1 && index == 9) continue;
                rom[index] = header == 1 && index == 8 ? static_cast<byte>(metadata & 0xf0) : metadata;
                accepts(rom);
            }
        auto all_metadata{fixture(header)};
        std::fill(all_metadata.begin() + 8, all_metadata.begin() + 16, 0xff);
        if (header == 1) { all_metadata[8] = 0xf0; all_metadata[9] = 0; }
        accepts(all_metadata);
        for (std::size_t index{}; index < 4; ++index) {
            auto rom{fixture(header)}; rom[index] ^= 0x80;
            for (const std::string mode : {"crown", "floor"})
                refuses(rom, "AtlasDevLadderCrown mode=" + mode);
        }
        for (const std::size_t index : {4U, 5U, 6U, 7U})
            for (unsigned value{}; value < 256; ++value) {
                const bool allowed{index == 4 ? value == 16 : index == 5 ? value == 0
                    : index == 6 ? value >= 0x10 && value <= 0x13 : value == 0 || value == 8};
                if (allowed) continue;
                auto rom{fixture(header)}; rom[index] = static_cast<byte>(value);
                for (const std::string mode : {"crown", "floor"})
                    refuses(rom, "AtlasDevLadderCrown mode=" + mode);
            }
    }
    for (const std::size_t index : {8U, 9U})
        for (unsigned value{1}; value < 256; ++value) {
            if (index == 8 && (value & 15) == 0) continue;
            auto rom{fixture(1)}; rom[index] = static_cast<byte>(value);
            for (const std::string mode : {"crown", "floor"})
                refuses(rom, "AtlasDevLadderCrown mode=" + mode);
        }
    // region labels and script tables do not identify compatible movement code.
    // expanded images are checked separately.
    for (const std::string region : {"eu", "jp", "us-rev-a", "us-512"}) accepts(fixture(), region);
    for (const auto signature : { std::pair<byte, byte>{0xb6, 0x9f}, {0x67, 0xa3}, {0x6b, 0x00} }) {
        auto rom{ fixture() };
        rom[0x3025d] = signature.first; rom[0x3025e] = signature.second;
        accepts(rom);
    }
}

void test_identity_ownership_capacity() {
    for (std::size_t size : { std::size_t{ 0 }, std::size_t{ 15 }, ROM_SIZE - 1, ROM_SIZE + 1, ROM_SIZE * 2 }) {
        auto rom{ fixture() }; rom.resize(size, 0xff);
        for (const std::string mode : {"crown", "floor"}) refuses(rom, "AtlasDevLadderCrown mode=" + mode);
    }
    for (const auto& site : SITES)
        for (std::size_t index{ 0 }; index < site.bytes.size() / 2; ++index) {
            const word address{ static_cast<word>(site.cpu + index) };
            if (address == 0xe322 || address == 0xe328 || address == 0xe370 || address == 0xe376) continue;
            auto rom{ fixture() }; rom[offset(address)] ^= 0xff;
            for (const std::string region : {"us", "jp"}) {
                if (!site.floor_only) refuses(rom, "AtlasDevLadderCrown", ORG, END, region);
                if (!site.crown_only) refuses(rom, "AtlasDevLadderCrown mode=floor", ORG, END, region);
            }
        }
    for (word low : { word{ 0xe322 }, word{ 0xe370 } }) {
        auto rom{ fixture() }; rom[offset(low)] = 0; rom[offset(static_cast<word>(low + 6))] = 0;
        refuses(rom, "AtlasDevLadderCrown");
        rom[offset(low)] = 1; rom[offset(static_cast<word>(low + 6))] = 8;
        refuses(rom, "AtlasDevLadderCrown");
    }
    refuses(fixture(), "AtlasDevLadderCrown", static_cast<word>(END - 604));
    refuses(fixture(), "AtlasDevLadderCrown", END);
    auto installed{ fixture() }; install(installed, "AtlasDevLadderCrown");
    refuses(installed, "AtlasDevLadderCrown");
}

void test_allocator_owned_space() {
    for (const bool crown : {true, false}) {
        const std::string spec{crown ? "AtlasDevLadderCrown" : "AtlasDevLadderCrown mode=floor"};
        const std::size_t bytes{crown ? 605U : 315U};
        for (const word origin : {word{0xc400}, word{0xf100}, word{ORG - 1}}) {
            auto expected{fixture()};
            require(install(expected, spec, origin, origin + bytes) == bytes, "allocator exact fit");
            for (const byte stale : {byte{0}, byte{0xa5}, byte{0xea}}) {
                auto reclaimed{fixture()};
                std::fill_n(reclaimed.begin() + offset(origin), bytes, stale);
                const auto before{reclaimed};
                require(install(reclaimed, spec, origin, origin + bytes) == bytes && reclaimed == expected,
                    "allocator-owned stale data must not require FF padding");
                owned_changes(before, reclaimed, origin, bytes, crown);
                refuses(before, spec, origin, origin + bytes - 1, "us", "overflow");
            }
            // changed hooks still prevent reinstalling at the same or a new address.
            refuses(expected, spec, origin, origin + bytes, "us", "required instructions");
            refuses(expected, spec, 0xf600, 0xf900, "us", "required instructions");
        }
        refuses(fixture(), spec, 0xbfff, 0xc400, "us", "CPU range");
        refuses(fixture(), spec, 0xf100, 0x10001, "us", "CPU range");
        refuses(fixture(), spec, 0x10000, 0x10000, "us", "CPU range");
        refuses(fixture(), spec, 0xff00, 0x10000, "us", "fixed-bank span");
        // the 16-bit cursor cannot hold $10000. reject an exact end fit
        // without wrapping or changing the rom.
        refuses(fixture(), spec, 0x10000 - bytes, 0x10000, "us", "wrapped");
    }
    for (const auto& [spec, bytes] : {
            std::pair{std::string{"AtlasDevLadderCrown\nAtlasDevFrameScheduler"}, 761U},
            std::pair{std::string{"AtlasDevLadderCrown\nAtlasDevJumpControl"}, 785U}})
        for (const std::size_t end : {0xd400U, 0xfff0U}) {
            auto rom{fixture()};
            require(install(rom, spec, end - bytes, end) == bytes,
                "companions must use the supplied allocation end, including beyond FFE0");
            refuses(fixture(), spec, end - bytes, end - 1, "us", "overflow");
        }
}

void test_composition() {
    for (const std::string first : { "AtlasDevLadderCrown", "AtlasDevLadderCrown mode=floor", "AtlasDevLadderCrown mode=vanilla" })
        for (const std::string second : { "AtlasDevLadderCrown", "AtlasDevLadderCrown mode=floor", "AtlasDevLadderCrown mode=vanilla" })
            refuses(fixture(), first + "\n" + second, ORG, END, "us", "AtlasDevLadderCrown");
    // Only Crown moves to the front. Companion bodies, allocation and order
    // must match an independent legacy installation at the shifted cursor.
    const auto composition = [&](const std::string& crown, const std::string& companions) {
        auto expected{fixture()};
        const auto crown_size{install(expected, crown)};
        const auto companion_size{install(expected, companions, static_cast<word>(ORG + crown_size))};
        for (const auto& spec : {crown + '\n' + companions, companions + '\n' + crown}) {
            auto actual{fixture()};
            require(install(actual, spec) == crown_size + companion_size && actual == expected,
                "Crown composition must preserve exact standalone companion emission: " + spec);
            require(target(actual, 0xe307) == ORG, "active Crown owns the first allocation");
        }
    };
    for (const std::string crown : { "AtlasDevLadderCrown", "AtlasDevLadderCrown mode=floor" }) {
        for (const std::string roles : {"AtlasDevDayNightCycle", "AtlasDevInfectedTint", "AtlasDevTimeOfDay",
                "AtlasDevDayNightCycle\nAtlasDevInfectedTint\nAtlasDevTimeOfDay",
                "AtlasDevDayNightCycle\nAtlasDevTimeOfDay\nAtlasDevInfectedTint"})
            composition(crown, "AtlasDevFrameScheduler\n" + roles);
        composition(crown, "AtlasDevFallControl profile=zelda2");
        composition(crown, "AtlasDevJumpControl coyote=1 buffer=0 shorthop=0 airjumps=0");
        // Crown does not repair another hack's dependency order or move
        // DayNight ahead of an existing POST chain.
        refuses(fixture(), "AtlasDevInfectedTint\nAtlasDevFrameScheduler\n" + crown,
            ORG, END, "us", "installed first");
        refuses(fixture(), "AtlasDevFrameScheduler\nAtlasDevInfectedTint\nAtlasDevDayNightCycle\n" + crown,
            ORG, END, "us", "bank 9 window at $8000 is not free");
    }
    for (const auto& [ladder, companions] : {
            std::pair{std::string{"AtlasDevLadderCrown mode=floor"}, std::string{"AtlasDevJumpControl"}},
            std::pair{std::string{"AtlasDevLadderCrown mode=floor"}, std::string{"AtlasDevJumpControl\nAtlasDevFallControl profile=zelda2"}},
            std::pair{std::string{"AtlasDevLadderCrown"}, std::string{"AtlasDevJumpControl"}}}) {
        const bool crown{ladder == "AtlasDevLadderCrown"};
        auto actual{fixture()}, reordered{fixture()};
        const auto total{install(actual, ladder + '\n' + companions)};
        require(install(reordered, companions + '\n' + ladder) == total && actual == reordered,
            "buffered ladder composition must be independent of Crown's list position");
        const word jump_origin{target(actual, 0xe3cd)};
        require(jump_origin == ORG + (crown ? 633 : 340), "shared ladder buffer helper has a bounded fixed-bank cost");
        if (crown) require(total == 785, "default Crown and Jump fit this allocation with one byte remaining");
        require(actual[offset(0xe34f)] == 0x4c && target(actual, 0xe34f) >= ORG
                && target(actual, 0xe34f) < jump_origin,
            "buffered descent uses a ladder-owned fixed-bank wrapper");
        const auto tick{unhex("addf04290ff003cedf0460")};
        require(std::equal(tick.begin(), tick.end(), actual.begin() + offset(jump_origin) - tick.size()),
            "shared climb timer decrements only a nonzero low buffer nibble");
        auto expected{fixture()};
        std::copy(actual.begin() + offset(ORG), actual.begin() + offset(jump_origin), expected.begin() + offset(ORG));
        for (const auto [pc, bytes] : {std::pair{word{0xe307}, 5U}, {word{0xe2c8}, 4U}, {word{0xe34f}, 5U}})
            std::copy_n(actual.begin() + offset(pc), bytes, expected.begin() + offset(pc));
        if (crown) std::copy_n(actual.begin() + offset(0xc145), 3, expected.begin() + offset(0xc145));
        require(install(expected, companions, jump_origin) == total - (jump_origin - ORG) && expected == actual,
            "buffered ladder support must not alter companion implementations or unrelated bytes");
    }
    // this fixture has 786 bytes. larger combinations must fail.
    // Shift the origin so the caller's window is one byte too small, while
    // keeping every companion preimage inside the physical ROM image.
    refuses(fixture(), "AtlasDevLadderCrown\nAtlasDevFrameScheduler",
        static_cast<word>(ORG + 26), END, "us", "overflow");
    for (const std::string spec : {"AtlasDevLadderCrown\nAtlasDevJumpControl",
            "AtlasDevJumpControl\nAtlasDevLadderCrown"})
        refuses(fixture(), spec, static_cast<word>(ORG + 2), END, "us", "overflow");
    for (const std::string movement : {
        "AtlasDevJumpControl coyote=0 buffer=0 shorthop=0 airjumps=0",
        "AtlasDevJumpControl", "AtlasDevFallControl profile=zelda2",
        "AtlasDevFrameScheduler\nAtlasDevDayNightCycle\nAtlasDevInfectedTint\nAtlasDevTimeOfDay" }) {
        auto baseline{ fixture() }; const auto size{ install(baseline, movement) };
        for (const std::string& spec : { "AtlasDevLadderCrown mode=vanilla\n" + movement,
            movement + "\nAtlasDevLadderCrown mode=vanilla" }) {
            auto rom{ fixture() };
            require(install(rom, spec) == size && rom == baseline, "vanilla Crown must not block movement controls");
        }
    }
}

void test_companion_capacity_preflight() {
    // Pin the existing Jump emitter, then compose it with Crown at an exact
    // fit and one byte short; the orchestrator's own bounds check refuses the
    // short window, since installers bound their scans by the ROM image.
    // Bits select coyote, buffer, short hop, air jumps and switchable;
    // nonzero numeric values do not change instruction lengths.
    constexpr std::array<std::size_t, 32> jump_bytes{
        0, 50, 74, 127, 25, 75, 99, 152, 121, 139, 171, 189, 146, 164, 196, 214,
        0, 74, 132, 185, 49, 123, 181, 234, 179, 197, 229, 247, 228, 246, 278, 296
    };
    for (std::size_t shape{}; shape < jump_bytes.size(); ++shape) {
        for (const unsigned value : {1U, 15U}) {
            const auto setting = [&](unsigned bit) { return std::to_string(shape & bit ? value : 0); };
            const std::string jump{"AtlasDevJumpControl coyote=" + setting(1) + " buffer=" + setting(2)
                + " shorthop=" + setting(4) + " airjumps=" + setting(8)
                + " switchable=" + std::to_string((shape & 16) != 0)};
            const std::string companions{(shape & 16 ? "AtlasDevFrameScheduler\n" : "") + jump};
            const std::size_t companion_bytes{jump_bytes[shape] + (shape & 16 ? fh::afs::CORE_SIZE : 0)};
            auto legacy{fixture()};
            require(install(legacy, companions) == companion_bytes,
                "native Jump body size drifted for shape " + std::to_string(shape));
            auto vanilla{fixture()};
            require(install(vanilla, "AtlasDevLadderCrown mode=vanilla\n" + companions) == companion_bytes
                    && vanilla == legacy,
                "vanilla Crown must not alter companion paths");

            const std::string composed{"AtlasDevLadderCrown mode=floor\n" + companions};
            const std::size_t total{(shape & 2 ? 340U : 315U) + companion_bytes};
            if (total <= END - ORG) {
                auto exact{fixture()};
                require(install(exact, composed, static_cast<word>(END - total)) == total,
                    "an exact-fit native Jump shape must install beside Crown");
                if (jump_bytes[shape])
                    refuses(fixture(), composed, static_cast<word>(END - total + 1), END, "us",
                        "overflow");
            }
            else
                refuses(fixture(), composed, ORG, END, "us", "overflow");
        }
    }
    // These would scan beyond physical $ffff in the native installers if
    // the Crown-only guard ran after installation instead of beforehand.
    for (const std::string spec : {
            "AtlasDevLadderCrown\nAtlasDevJumpControl\nAtlasDevFrameScheduler",
            "AtlasDevLadderCrown\nAtlasDevFrameScheduler\nAtlasDevJumpControl",
            "AtlasDevLadderCrown downhold=8 align=2 roompolicy=deny rooms=0:0+1:1+2:2+3:3+4:4+5:5+6:3+255:255\nAtlasDevFrameScheduler"})
        refuses(fixture(), spec, ORG, END, "us", "overflow");
}

void test_ladder_control_composition() {
    auto source{fixture()};
    put(source, 0xe314, "a5a138e90185a1");
    put(source, 0xe35c, "a5a018698085a0");
    put(source, 0xe363, "a5a1690185a1");
    put(source, 0xe107, "20f6ecb00d");
    put(source, 0xecac, "a5a44a9007a5a43017a90360");
    const auto overlay{unhex("a5a44a9007a5a43028a90360")};
    std::copy(overlay.begin(), overlay.end(), source.begin()
        + klib::Asm6502::get_file_offset(14, 0xb927));

    for (const std::string mode : {"crown", "floor", "vanilla"}) {
        const std::string crown{"AtlasDevLadderCrown mode=" + mode};
        const std::size_t body{mode == "crown" ? 605U : mode == "floor" ? 315U : 0U};
        for (const std::string options : {"", " up=384 down=448 wingup=2 wingdown=512",
                " attack=1 attackpose=1", " up=2048 down=2048 attackflag=4 attackpose=1",
                " attackflag=0", " attackflag=247", " attackflag=255"}) {
            const std::string control{"AtlasDevLadderControl" + options};
            auto expected{source};
            require(install(expected, crown) == body, "Crown size with ladder controls");
            const auto helper{install(expected, control, ORG + body)};
            const bool runtime{options.find("attackflag=") != std::string::npos
                && options.find("attackflag=255") == std::string::npos};
            require(helper == (runtime ? 14U : 0U), "ladder control helper size");
            for (const std::string spec : {crown + '\n' + control, control + '\n' + crown}) {
                auto actual{source};
                require(install(actual, spec, ORG, ORG + body + helper) == body + helper
                        && actual == expected,
                    "ladder controls must match standalone output in either list order");
            }
        }
        if (body) {
            const std::string spec{crown + "\nAtlasDevLadderControl attackflag=4"};
            refuses(source, spec, ORG, ORG + body + 13, "us", "overflow");
            // reject before the helper scans past the physical bank end.
            refuses(source, spec, 0x10000 - body - 7, 0x10000, "us", "overflow");
        }
    }
}

void test_crown_script_ram() {
    struct Overrides {
        std::filesystem::path directory;
        Overrides() {
            for (unsigned attempt{}; ; ++attempt) {
                directory = std::filesystem::current_path() / ("crown-ram-regression-"
                    + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
                    + '-' + std::to_string(attempt));
                if (std::filesystem::create_directory(directory)) break;
            }
        }
        ~Overrides() {
            std::error_code ignored;
            std::filesystem::remove(directory / "override.xml", ignored);
            std::filesystem::remove(directory, ignored);
        }
        fe::Config config(const std::vector<byte>& rom, const std::string& key, std::size_t value) const {
            const auto file{directory / "override.xml"};
            std::ofstream out(file);
            out << "<eoe_config><consts><const name=\"" << key << "\" value=\"" << value
                << "\"/></consts></eoe_config>";
            out.close();
            if (!out) throw std::runtime_error("cannot write authored Crown RAM configuration");
            return fe::Config(EOE_TEST_CONFIG_PATH, file.string(), rom, "us");
        }
    } overrides;
    for (const std::string key : {"hack_script_jsr_ram_addr_lo", "hack_script_jsr_ram_addr_hi",
            "hack_script_selected_flag_ram_addr", "hack_script_var_ram_addr"}) {
        for (std::size_t mirror{}; mirror < 0x2000; mirror += 0x800)
            for (std::size_t cell : {0x04e0U, 0x04e1U}) {
                const auto source{fixture()};
                const auto cfg{overrides.config(source, key, cell + mirror)};
                auto rom{source};
                bool refused{false};
                try { fh::HackManager{}.install_general_hacks(cfg, rom, 15, ORG, END,
                    fh::parse_general_hacks("AtlasDevLadderCrown")); }
                catch (const std::runtime_error& error) {
                    refused = true;
                    require(std::string(error.what()).find("overlaps Crown RAM") != std::string::npos,
                        "script overlap must fail specifically on Crown's storage claim");
                }
                require(refused && rom == source, "Crown must reject script RAM overlapping either owned cell or mirror");
                for (const std::string mode : {"floor", "vanilla"}) {
                    rom = source;
                    require(fh::HackManager{}.install_general_hacks(cfg, rom, 15, ORG, END,
                        fh::parse_general_hacks("AtlasDevLadderCrown mode=" + mode)) == (mode == "floor" ? 315 : 0),
                        "RAM-free Floor/vanilla must not inherit Crown's storage claim");
                }
            }
    }
    auto source{fixture()};
    const auto cfg{overrides.config(source, "hack_script_var_ram_addr", 0x04da)};
    auto rom{source};
    bool refused{false};
    try { fh::HackManager{}.install_general_hacks(cfg, rom, 15, ORG, END,
        fh::parse_general_hacks("AtlasDevLadderCrown")); }
    catch (const std::runtime_error& error) {
        refused = true;
        require(std::string(error.what()).find("overlaps Crown RAM") != std::string::npos,
            "script-array overlap must fail specifically on Crown's storage claim");
    }
    require(refused && rom == source, "Crown must check the complete configured script-variable array");
}

void test_preinstalled_scheduler_ownership() {
    auto neutral{ fixture() };
    require(install(neutral, "AtlasDevFrameScheduler") == fh::afs::CORE_SIZE, "neutral scheduler fixture size");
    require(fh::afs::find_base(neutral) == ORG, "neutral scheduler fixture recognized");
    const auto original{ neutral };
    const word crown_origin{ static_cast<word>(ORG + fh::afs::CORE_SIZE) };
    require(install(neutral, "AtlasDevLadderCrown", crown_origin) == 605, "neutral scheduler allows Crown");
    owned_changes(original, neutral, crown_origin, 605, true);
    for (const std::size_t site : { fh::afs::OFF_PRE0, fh::afs::OFF_PRE1, fh::afs::OFF_PRE2, fh::afs::OFF_POST }) {
        auto claimed{ original };
        claimed[offset(ORG) + site] = static_cast<byte>(ORG & 255);
        claimed[offset(ORG) + site + 1] = static_cast<byte>(ORG >> 8);
        require(fh::afs::find_base(claimed) == ORG, "claimed vector retains recognized scheduler identity");
        refuses(claimed, "AtlasDevLadderCrown", crown_origin, END, "us", "AtlasDevLadderCrown");
    }
    auto armed{ original };
    armed[offset(ORG) + fh::afs::OFF_POSTARMED] = 1;
    require(fh::afs::find_base(armed) == ORG, "armed POST retains recognized scheduler identity");
    refuses(armed, "AtlasDevLadderCrown", crown_origin, END, "us", "AtlasDevLadderCrown");
    auto corrupt{ original };
    corrupt[offset(ORG)] ^= 0xff;
    require(fh::afs::find_base(corrupt) == 0, "corrupted scheduler is unrecognized");
    refuses(corrupt, "AtlasDevLadderCrown", crown_origin, END, "us", "AtlasDevLadderCrown");
    auto daynight{ fixture() };
    require(install(daynight, "AtlasDevFrameScheduler\nAtlasDevDayNightCycle") == fh::afs::CORE_SIZE,
        "preinstalled Day/Night fixture");
    require(fh::afs::find_base(daynight) == ORG, "Day/Night scheduler recognized");
    refuses(daynight, "AtlasDevLadderCrown", crown_origin, END, "us", "preinstalled");
    const auto before_floor{ daynight };
    require(install(daynight, "AtlasDevLadderCrown mode=floor", crown_origin) == 315,
        "preinstalled Day/Night allows RAM-free floor mode");
    owned_changes(before_floor, daynight, crown_origin, 315, false);
    for (const std::string role : {"AtlasDevInfectedTint", "AtlasDevTimeOfDay"}) {
        auto installed{fixture()};
        install(installed, "AtlasDevFrameScheduler\n" + role);
        for (bool disabled : {false, true}) {
            auto candidate{installed};
            if (disabled) std::fill_n(candidate.begin() + offset(ORG) + fh::afs::OFF_ARM0, 3, 0);
            refuses(candidate, "AtlasDevLadderCrown", crown_origin, END, "us", "preinstalled");
        }
    }
}

word parse_origin(std::string_view value) {
    int base{ 10 };
    if (value.starts_with("0x")) { base = 16; value.remove_prefix(2); }
    unsigned int result{ 0 };
    const auto parsed{ std::from_chars(value.data(), value.data() + value.size(), result, base) };
    if (value.empty() || parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result > 0xffff)
        throw std::runtime_error("origin must be an unsigned decimal or 0x hexadecimal 16-bit address");
    return static_cast<word>(result);
}

void export_rom(const char* source_name, const char* output_name, const char* origin_text, const char* spec) {
    const std::filesystem::path source{ source_name }, output{ output_name };
    if (std::filesystem::exists(output) || std::filesystem::is_symlink(output))
        throw std::runtime_error("export output already exists; supply a fresh path");
    if (std::filesystem::file_size(source) != ROM_SIZE)
        throw std::runtime_error("source must be an unexpanded NES ROM");
    std::ifstream input(source, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open source ROM");
    std::vector<byte> rom((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (input.bad() || rom.size() != ROM_SIZE) throw std::runtime_error("source must be an unexpanded NES ROM");
    fe::Config source_config;
    source_config.load_definitions(EOE_TEST_CONFIG_PATH, "");
    source_config.determine_region(rom);
    const auto origin{ parse_origin(origin_text) };
    const auto parsed{ fh::parse_general_hacks(spec) };
    const auto used{ fh::HackManager{}.install_general_hacks(source_config, rom, 15, origin, END,
        fh::filter_general_hacks(15, parsed), nullptr) };
    std::ofstream file(output, std::ios::binary);
    if (!file) throw std::runtime_error("cannot create export output");
    file.write(reinterpret_cast<const char*>(rom.data()), static_cast<std::streamsize>(rom.size()));
    file.close();
    if (!file) throw std::runtime_error("could not finish export output");
    std::cout << "{\"origin\":" << origin << ",\"body_bytes\":" << used << ",\"rom_bytes\":" << rom.size() << "}\n";
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 6 && std::string_view(argv[1]) == "--export") {
            export_rom(argv[2], argv[3], argv[4], argv[5]);
            return 0;
        }
        if (argc != 1) throw std::runtime_error("usage: atlas_ladder_crown_regression [--export SOURCE OUT ORIGIN SPEC]");
        test_defaults_and_relocation();
        test_options();
        test_invalid_settings();
        test_structural_headers();
        test_identity_ownership_capacity();
        test_allocator_owned_space();
        test_composition();
        test_companion_capacity_preflight();
        test_ladder_control_composition();
        test_crown_script_ram();
        test_preinstalled_scheduler_ownership();
    }
    catch (const std::exception& error) {
        std::cerr << "atlas_ladder_crown_regression: " << error.what() << '\n';
        return 1;
    }
    std::cout << "atlas_ladder_crown_regression: " << checks << " checks passed\n";
    return 0;
}
