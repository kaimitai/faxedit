#include "fe/Config.h"
#include "fh/HackManager.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<byte>;
std::size_t checks{}, executions{};
void check(bool value, const std::string& why) {
    ++checks; if (!value) throw std::runtime_error(why);
}
constexpr std::array OPS{fh::HackLib::AtlasDevSwapVar, fh::HackLib::AtlasDevRepeat,
    fh::HackLib::AtlasDevSwitch, fh::HackLib::AtlasDevIfRandomChance,
    fh::HackLib::AtlasDevPeekToVar, fh::HackLib::AtlasDevFrameCountToVar,
    fh::HackLib::AtlasDevReadFlagToVar, fh::HackLib::AtlasDevWriteVarToMetatile,
    fh::HackLib::AtlasDevRandomVar};
std::size_t file(word cpu) { return 0x28010 + cpu; }
word read_word(const Bytes& rom, std::size_t at) { return static_cast<word>(rom.at(at) | rom.at(at + 1) << 8); }
void put_word(Bytes& rom, word at, word value) { rom[file(at)] = value & 255; rom[file(at) + 1] = value >> 8; }
std::size_t fixed(std::size_t file_offset) { // bank-15 mirror: $C000-$FFFF lives at the PRG tail
    return file_offset >= 0x3c000 && file_offset < 0x40010 ? file_offset : file_offset + 0x10000;
}

struct Cpu {
    std::array<byte, 65536> m{};
    byte a{}, x{}, y{}, p{1}, sp{0xf0}; word pc{};
    std::vector<word> writes;
    byte get() { return m[pc++]; }
    word address() { const auto lo = get(); return static_cast<word>(lo | get() << 8); }
    byte nz(byte v) { p = static_cast<byte>((p & ~0x82) | (v == 0 ? 2 : 0) | (v & 128)); return v; }
    void store(word at, byte v) { writes.push_back(at); m[at] = v; }
    void push(byte v) { store(static_cast<word>(0x100 | sp--), v); }
    byte pop() { return m[0x100 | ++sp]; }
    void compare(byte left, byte right) { p = static_cast<byte>((p & ~1) | (left >= right)); nz(static_cast<byte>(left - right)); }
    void branch(bool yes) { const auto delta = static_cast<std::int8_t>(get()); if (yes) pc = static_cast<word>(pc + delta); }
    void step() {
        const auto opcode = get(); word at; byte lo;
        switch (opcode) {
        case 0xa9: a = nz(get()); break;
        case 0xa5: a = nz(m[get()]); break;
        case 0xad: a = nz(m[address()]); break;
        case 0xbd: a = nz(m[static_cast<word>(address() + x)]); break;
        case 0xb9: a = nz(m[static_cast<word>(address() + y)]); break;
        case 0xb1: lo = get(); at = static_cast<word>(m[lo] | m[static_cast<byte>(lo + 1)] << 8); a = nz(m[static_cast<word>(at + y)]); break;
        case 0xa2: x = nz(get()); break;
        case 0xa6: x = nz(m[get()]); break;
        case 0xa0: y = nz(get()); break;
        case 0x85: store(get(), a); break;
        case 0x8d: store(address(), a); break;
        case 0x9d: store(static_cast<word>(address() + x), a); break;
        case 0x99: store(static_cast<word>(address() + y), a); break;
        case 0x86: store(get(), x); break;
        case 0x96: store(static_cast<word>(address() + y), x); break;
        case 0xaa: x = nz(a); break;
        case 0xa8: y = nz(a); break;
        case 0x8a: a = nz(x); break;
        case 0x98: a = nz(y); break;
        case 0xba: x = nz(sp); break;
        case 0x9a: sp = x; break;
        case 0x48: push(a); break;
        case 0x68: a = nz(pop()); break;
        case 0x08: push(static_cast<byte>(p | 0x30)); break;
        case 0x28: p = static_cast<byte>(pop() & 0xcf); break;
        case 0x29: a = nz(a & get()); break;
        case 0x39: a = nz(a & m[static_cast<word>(address() + y)]); break;
        case 0x09: a = nz(a | get()); break;
        case 0x49: a = nz(a ^ get()); break;
        case 0x45: a = nz(a ^ m[get()]); break;
        case 0xc9: compare(a, get()); break;
        case 0xe0: compare(x, get()); break;
        case 0xc5: compare(a, m[get()]); break;
        case 0xc0: compare(y, get()); break;
        case 0x18: p &= ~1; break;
        case 0x38: p |= 1; break;
        case 0xe8: x = nz(static_cast<byte>(x + 1)); break;
        case 0xca: x = nz(static_cast<byte>(x - 1)); break;
        case 0xc8: y = nz(static_cast<byte>(y + 1)); break;
        case 0x88: y = nz(static_cast<byte>(y - 1)); break;
        case 0xe6: at = get(); store(at, nz(static_cast<byte>(m[at] + 1))); break;
        case 0xee: at = address(); store(at, nz(static_cast<byte>(m[at] + 1))); break;
        case 0xc6: at = get(); store(at, nz(static_cast<byte>(m[at] - 1))); break;
        case 0xce: at = address(); store(at, nz(static_cast<byte>(m[at] - 1))); break;
        case 0x4a: { const word r = a; p = static_cast<byte>((p & ~1) | (r & 1)); a = nz(static_cast<byte>(r >> 1)); } break;
        case 0x0a: { const word r = static_cast<word>(a << 1); p = static_cast<byte>((p & ~1) | (r > 255)); a = nz(static_cast<byte>(r)); } break;
        case 0x46: at = get(); { const word r = m[at]; p = static_cast<byte>((p & ~1) | (r & 1)); store(at, nz(static_cast<byte>(r >> 1))); } break;
        case 0x6a: { const word r = static_cast<word>(a) | (p & 1 ? 0x100 : 0); p = static_cast<byte>((p & ~1) | (r & 1)); a = nz(static_cast<byte>(r >> 1)); } break;
        case 0x69: { const word r = a + get() + (p & 1); p = static_cast<byte>((p & ~1) | (r > 255)); a = nz(static_cast<byte>(r)); } break;
        case 0x65: { const word r = a + m[get()] + (p & 1); p = static_cast<byte>((p & ~1) | (r > 255)); a = nz(static_cast<byte>(r)); } break;
        case 0xe9: { const word r = a - get() - (1 - (p & 1)); p = static_cast<byte>((p & ~1) | (r < 0x100)); a = nz(static_cast<byte>(r)); } break;
        case 0xf0: branch(p & 2); break;
        case 0xd0: branch(!(p & 2)); break;
        case 0x30: branch(p & 128); break;
        case 0x10: branch(!(p & 128)); break;
        case 0xb0: branch(p & 1); break;
        case 0x90: branch(!(p & 1)); break;
        case 0x4c: pc = address(); break;
        case 0x20: at = address(); push(static_cast<byte>((pc - 1) >> 8)); push(static_cast<byte>(pc - 1)); pc = at; break;
        case 0x60: lo = pop(); pc = static_cast<word>((lo | pop() << 8) + 1); break;
        case 0xea: break;
        default: throw std::runtime_error("unsupported instruction " + std::to_string(opcode) + " at " + std::to_string(pc - 1));
        }
    }
    void run(word entry, word stop) {
        pc = entry; std::size_t steps{}; ++executions;
        while (pc != stop) { if (++steps >= 4000) throw std::runtime_error("instruction limit"); step(); }
        check(sp == 0xf0, "balanced caller stack");
    }
};

struct Settings { word vars{0x03b5}, helpers{0x9000}, clear_flags{0xf800}; unsigned count{8}; };
struct Fixture {
    std::filesystem::path directory;
    std::vector<std::filesystem::path> files;
    Fixture() {
        for (unsigned i{};; ++i) {
            directory = std::filesystem::current_path() / ("atlas-flow-test-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(i));
            if (std::filesystem::create_directory(directory)) break;
        }
    }
    ~Fixture() { std::error_code ignored; for (const auto& path : files) std::filesystem::remove(path, ignored); std::filesystem::remove(directory, ignored); }
    fe::Config config(const Bytes& rom, Settings s) {
        const auto path = directory / (std::to_string(files.size()) + ".xml"); files.push_back(path);
        std::ofstream out(path);
        out << "<eoe_config><consts>";
        for (const auto& [key, value] : std::map<std::string, unsigned>{
            {"hack_script_var_ram_addr", s.vars}, {"hack_script_var_count", s.count},
            {"rom_iscripts_loadbyte", s.helpers}, {"rom_iscripts_skipaddrandinvoke", s.helpers + 0x40U},
            {"rom_iscripts_jumptonextaddr", s.helpers + 0x80U}, {"rom_iscripts_invokenextaction", s.helpers + 0xc0U},
            {"rom_iscripts_begin", s.helpers + 0x100U}, {"hack_clear_persistent_flags", s.clear_flags}})
            out << "<const name=\"" << key << "\" value=\"" << value << "\"/>";
        out << "</consts></eoe_config>"; out.close(); check(bool(out), "authored configuration write");
        return fe::Config(EOE_TEST_CONFIG_PATH, path.string(), rom, "us");
    }
};
struct Image { Bytes rom; std::map<fh::HackLib, word> entry; word begin{}, end{}; Settings settings; };
Image install(Fixture& fixture, Settings settings, word origin, const std::vector<fh::HackLib>& library) {
    Bytes rom(0x40010, 0xff); put_word(rom, 0x8273, 0x8f10); put_word(rom, 0x8277, 0x8f00);
    put_word(rom, 0x8f00, 0x2222); put_word(rom, 0x8f10, 0x3333);
    const auto before = rom; const auto cfg = fixture.config(rom, settings);
    const auto end_file = fh::HackManager{}.apply_script_library(cfg, rom, file(origin), library, 2);
    const auto low = read_word(rom, file(0x8277)), high = read_word(rom, file(0x8273));
    check(rom[file(low)] == 0x22 && rom[file(low + 1)] == 0x22 && rom[file(high)] == 0x33 && rom[file(high + 1)] == 0x33,
          "existing dispatch entries preserved");
    Image image{std::move(rom), {}, origin, static_cast<word>(end_file - 0x28010), settings};
    for (std::size_t i{}; i < library.size(); ++i)
        image.entry[library[i]] = static_cast<word>(1 + image.rom[file(low) + i + 2] + (image.rom[file(high) + i + 2] << 8));
    const bool needs_flags = std::find(library.begin(), library.end(), fh::HackLib::AtlasDevReadFlagToVar) != library.end();
    for (std::size_t i{}; i < image.rom.size(); ++i) if (image.rom[i] != before[i])
        check((i >= file(origin) && i < end_file) || (i >= file(0x8273) && i < file(0x8275))
            || (i >= file(0x8277) && i < file(0x8279))
            || (needs_flags && fixed(i) >= 0x3c950 && fixed(i) < 0x3c978)
            || (needs_flags && fixed(i) >= 0x3f800 && fixed(i) < 0x3f880)
            || (i >= file(settings.helpers + 0x100) && i < file(settings.helpers + 0x106)),
            "installer changed an unowned byte at cpu " + std::to_string(i - 0x28010));
    return image;
}
Cpu cpu(const Image& image) {
    Cpu c; std::copy_n(image.rom.begin() + file(0x8000), 0x4000, c.m.begin() + 0x8000);
    const auto h = image.settings.helpers;
    const auto put = [&](word address, std::initializer_list<byte> bytes) { std::copy(bytes.begin(), bytes.end(), c.m.begin() + address); };
    put(h, {0xa0,0,0xb1,0xdb,0xe6,0xdb,0xd0,2,0xe6,0xdc,0x60});
    const auto lo = static_cast<byte>(h), hi = static_cast<byte>(h >> 8);
    const auto next_lo = static_cast<byte>(h + 0xc0), next_hi = static_cast<byte>((h + 0xc0) >> 8);
    put(h + 0x40, {0x20,lo,hi,0x20,lo,hi,0x4c,next_lo,next_hi});
    put(h + 0x80, {0x20,lo,hi,0x48,0x20,lo,hi,0x85,0xdc,0x68,0x85,0xdb,0x4c,next_lo,next_hi});
    put(0xd7b0, {0x60}); // the vanilla metatile writer stands in as an rts
    return c;
}
void prepare(Cpu& c, const std::vector<byte>& operands) {
    std::fill_n(c.m.begin(), 0x800, 0x69); c.a = 0x46; c.x = 0x3b; c.y = 0x8c; c.p = 1; c.sp = 0xf0; c.writes.clear();
    c.m[0xdb] = 0x00; c.m[0xdc] = 0x06; // the operand stream starts at $0600
    for (std::size_t i{}; i < operands.size(); ++i) c.m[0x600 + i] = operands[i];
}
word pointer(const Cpu& c) { return word(c.m[0xdb] | c.m[0xdc] << 8); }
word settings_vars(const Image& image) { return image.settings.vars; }
void execute(Cpu& c, const Image& image, fh::HackLib which) { c.run(image.entry.at(which), image.settings.helpers + 0xc0); }
word start(Cpu& c, const Image& image, fh::HackLib which, const std::vector<byte>& operands) {
    prepare(c, operands); execute(c, image, which); return pointer(c);
}

void swap_behavior(const Image& image) {
    { Cpu c = cpu(image); prepare(c, {1, 4}); c.m[0x03b6] = 0x22; c.m[0x03b9] = 0x44;
      execute(c, image, fh::HackLib::AtlasDevSwapVar);
      check(c.m[0x03b6] == 0x44 && c.m[0x03b9] == 0x22, "SwapVar exchanges two registers");
      check(pointer(c) == 0x602, "SwapVar consumes both operands"); }
    { Cpu c = cpu(image); prepare(c, {8, 4}); c.m[0x03b9] = 0x44;
      execute(c, image, fh::HackLib::AtlasDevSwapVar);
      check(c.m[0x03b9] == 0x44, "SwapVar invalid register does nothing");
      check(pointer(c) == 0x602, "invalid SwapVar still consumes both operands"); }
    { Cpu c = cpu(image); prepare(c, {0, 8}); c.m[0x03b5] = 0x21;
      execute(c, image, fh::HackLib::AtlasDevSwapVar);
      check(c.m[0x03b5] == 0x21, "SwapVar invalid second register does nothing"); }
}

void repeat_behavior(const Image& image) {
    // one prepared pass each: fresh stores, running decrements, final falls
    // through.  the jump stub loads the label into the pointer, so a jumping
    // pass ends with the pointer equal to the label, and a falling pass
    // consumes the label linearly.
    const auto pass = [&](byte held) {
        Cpu c = cpu(image); prepare(c, {2, 2, 0x34, 0x12}); c.m[0x03b7] = held;
        execute(c, image, fh::HackLib::AtlasDevRepeat); return std::pair<byte, word>{c.m[0x03b7], pointer(c)};
    };
    auto [held, ptr] = pass(0x00);
    check(held == 2 && ptr == 0x1234, "fresh Repeat stores the count and jumps back");
    std::tie(held, ptr) = pass(0x02);
    check(held == 1 && ptr == 0x1234, "a running pass decrements and jumps");
    std::tie(held, ptr) = pass(0x01);
    check(held == 0 && ptr == 0x604, "the last pass falls through past the label");
    // count 0: immediate fall through, no jump, no store
    Cpu z = cpu(image); prepare(z, {5, 0, 0x78, 0x56}); z.m[0x03ba] = 0x00;
    execute(z, image, fh::HackLib::AtlasDevRepeat);
    check(pointer(z) == 0x604 && z.m[0x03ba] == 0x00, "Repeat with count 0 never jumps and never stores");
}

void switch_behavior(const Image& image) {
    for (byte rows{1}; rows <= 4; ++rows)
        for (int value{-1}; value <= rows + 1; ++value) {
            Cpu c = cpu(image); prepare(c, {0, rows});
            c.m[0x03b5] = static_cast<byte>(value < 0 ? 0x80 | 3 : value);
            execute(c, image, fh::HackLib::AtlasDevSwitch);
            const byte selector = c.m[0x03b5];
            const byte skipped = selector >= rows ? rows : selector;
            check(pointer(c) == word(0x602 + 3 * skipped), "Switch skips exactly three bytes per row below the bound");
        }
}

// the reference step, mirroring what AtlasDevRandomVar's emitter computes
byte reference_step(byte state, byte frame) {
    byte next = state == 0 ? static_cast<byte>(frame | 1) : state;
    word shifted = static_cast<word>(next << 1);
    if (shifted > 255) shifted = static_cast<word>(shifted ^ 0x11d);
    next = static_cast<byte>(shifted);
    return static_cast<byte>(next ^ frame);
}

void random_behavior(const Image& image) {
    for (const auto [state, frame] : std::array<std::pair<byte, byte>, 5>{{{0x00, 0x37}, {0x01, 0x00}, {0x5a, 0xc3}, {0xff, 0x80}, {0x80, 0x1a}}}) {
        // probability 0: no branch, and the generator is not touched
        { Cpu c = cpu(image); prepare(c, {0, 0x78, 0x56}); c.m[0xda] = state; c.m[0x1a] = frame;
          execute(c, image, fh::HackLib::AtlasDevIfRandomChance);
          check(pointer(c) == 0x603, "probability 0 takes the false path past the label");
          check(c.m[0xda] == state, "probability 0 does not step the generator"); }
        // any probability: the decision matches RandomVar's byte for the same state
        for (const byte probability : {byte{1}, byte{0x40}, byte{0xfe}}) {
            Cpu c = cpu(image); prepare(c, {probability, 9}); // RandomVar operands: register 9 is invalid, harmless
            c.m[0xda] = state; c.m[0x1a] = frame;
            execute(c, image, fh::HackLib::AtlasDevIfRandomChance);
            const byte stepped = c.m[0xda];
            const byte expected_state = state == 0 ? static_cast<byte>(frame | 1) : state;
            const word shifted = static_cast<word>(expected_state << 1);
            const byte after = static_cast<byte>(shifted > 255 ? shifted ^ 0x1d : shifted);
            check(stepped == after, "IfRandomChance steps the shared generator exactly like RandomVar");
            (void)reference_step;
        }
        // the branch itself: compare against RandomVar drawing with maximum 255
        for (const byte probability : {byte{1}, byte{0x40}, byte{0x80}, byte{0xff}}) {
            Cpu r = cpu(image); prepare(r, {3, 255}); r.m[0xda] = state; r.m[0x1a] = frame;
            execute(r, image, fh::HackLib::AtlasDevRandomVar);
            const byte drawn = r.m[0x03b8];
            Cpu c = cpu(image); prepare(c, {probability, 0x77, 0x66}); // label $6677
            c.m[0xda] = state; c.m[0x1a] = frame;
            execute(c, image, fh::HackLib::AtlasDevIfRandomChance);
            const bool jumped = pointer(c) == 0x6677;
            check(jumped == (drawn < probability), "IfRandomChance decides exactly as RandomVar draws below Probability");
        }
    }
}

void peek_and_clock_behavior(const Image& image) {
    { Cpu c = cpu(image); c.m[0x1234] = 0x7e; const word used = start(c, image, fh::HackLib::AtlasDevPeekToVar, {0x34, 0x12, 1});
      check(c.m[0x03b6] == 0x7e, "PeekToVar reads the addressed byte");
      check(used == 0x603, "PeekToVar consumes three operands"); }
    { Cpu c = cpu(image); c.m[0x1234] = 0x7e; const word used = start(c, image, fh::HackLib::AtlasDevPeekToVar, {0x34, 0x12, 8});
      check(c.m[0x03bd] == 0x69, "PeekToVar invalid register does not store");
      check(used == 0x603, "invalid PeekToVar still consumes three operands"); }
    { Cpu c = cpu(image); prepare(c, {4}); c.m[0x021d] = 0xa5;
      execute(c, image, fh::HackLib::AtlasDevFrameCountToVar);
      check(c.m[0x03b9] == 0xa5, "FrameCountToVar copies the dialogue timer"); }
    { Cpu c = cpu(image); prepare(c, {8}); c.m[0x021d] = 0xa5;
      execute(c, image, fh::HackLib::AtlasDevFrameCountToVar);
      check(pointer(c) == 0x601 && c.m[0x03bd] == 0x69, "FrameCountToVar invalid register consumes and does nothing"); }
}

void flag_behavior(const Image& image) {
    // flag 34 lives at $0101 + 4, bit 0x04
    { Cpu c = cpu(image); prepare(c, {34, 5}); c.m[0x0105] = 0x04;
      execute(c, image, fh::HackLib::AtlasDevReadFlagToVar);
      check(c.m[0x03ba] == 1, "ReadFlagToVar yields canonical 1 for a set flag"); check(pointer(c) == 0x602, "operands consumed"); }
    { Cpu c = cpu(image); prepare(c, {34, 5}); c.m[0x0105] = 0x00;
      execute(c, image, fh::HackLib::AtlasDevReadFlagToVar);
      check(c.m[0x03ba] == 0, "ReadFlagToVar yields canonical 0 for a clear flag"); }
    { Cpu c = cpu(image); prepare(c, {247, 5}); c.m[0x011f] = 0x80;
      execute(c, image, fh::HackLib::AtlasDevReadFlagToVar);
      check(c.m[0x03ba] == 1, "flag 247 is still inside the block"); }
    { Cpu c = cpu(image); prepare(c, {248, 5}); const std::array before{c.m[0x0100], c.m[0x0101], c.m[0x011f], c.m[0x0120]};
      execute(c, image, fh::HackLib::AtlasDevReadFlagToVar);
      check(pointer(c) == 0x602, "out-of-range flag still consumes both operands");
      check(c.m[0x03ba] == 0, "out-of-range flag stores canonical 0");
      check(c.m[0x0100] == before[0] && c.m[0x0101] == before[1] && c.m[0x011f] == before[2] && c.m[0x0120] == before[3],
            "out-of-range flag never reads or writes beyond the block"); }
}

void metatile_behavior(const Image& image) {
    { Cpu c = cpu(image); prepare(c, {0x45, 0}); c.m[0x03b5] = 0x5a; c.m[0x24] = 0;
      execute(c, image, fh::HackLib::AtlasDevWriteVarToMetatile);
      check(c.m[0x03ce] == 0x5a && c.m[0x03cf] == 0x45, "WriteVarToMetatile stages the register tile and the packed position");
      check(pointer(c) == 0x602, "WriteVarToMetatile consumes two operands"); }
    { Cpu c = cpu(image); prepare(c, {0xd5, 0}); c.m[0x03b5] = 0x5a;
      execute(c, image, fh::HackLib::AtlasDevWriteVarToMetatile);
      check(c.m[0x03ce] == 0x69 && c.m[0x03cf] == 0x69, "packed row above 12 is rejected"); }
    { Cpu c = cpu(image); prepare(c, {0x45, 0}); c.m[0x03b5] = 0x5a; c.m[0x24] = 0x08;
      execute(c, image, fh::HackLib::AtlasDevWriteVarToMetatile);
      check(c.m[0x03ce] == 0x69 && c.m[0x03cf] == 0x69, "world at or above 8 is rejected"); }
    { Cpu c = cpu(image); prepare(c, {0x45, 8}); c.m[0x03b5] = 0x5a;
      execute(c, image, fh::HackLib::AtlasDevWriteVarToMetatile);
      check(pointer(c) == 0x602 && c.m[0x03ce] == 0x69, "invalid register is rejected after consuming both operands"); }
}
}

int main() {
    Fixture fixture;
    const Settings settings;
    const auto image = install(fixture, settings, 0xa000, {OPS.begin(), OPS.end()});
    swap_behavior(image);
    repeat_behavior(image);
    switch_behavior(image);
    random_behavior(image);
    peek_and_clock_behavior(image);
    flag_behavior(image);
    metatile_behavior(image);
    std::cout << "atlas_script_flow_regression: " << checks << " checks, " << executions << " handler executions ok\\n";
    return 0;
}
