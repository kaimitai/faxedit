#include "fe/Config.h"
#include "fh/HackManager.h"
#include "fi/Opcode.h"
#include "common/klib/Kstring.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
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
constexpr std::array OPS{fh::HackLib::AtlasDevIfEffectActive, fh::HackLib::AtlasDevGetEffectTime,
    fh::HackLib::AtlasDevClearTimedEffect, fh::HackLib::AtlasDevClearTimedEffects};
std::size_t file(word cpu) { return 0x28010 + cpu; }
word read_word(const Bytes& rom, std::size_t at) { return static_cast<word>(rom.at(at) | rom.at(at + 1) << 8); }
void put_word(Bytes& rom, word at, word value) { rom[file(at)] = value & 255; rom[file(at) + 1] = value >> 8; }

// a deliberately small instruction executor; unimplemented instructions fail.
// authored interpreter helpers below model their public calling contracts.
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
        case 0x09: a = nz(a | get()); break;
        case 0x49: a = nz(a ^ get()); break;
        case 0xc9: compare(a, get()); break;
        case 0xe0: compare(x, get()); break;
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
        while (pc != stop) { if (++steps >= 1200) throw std::runtime_error("instruction limit"); step(); }
        check(sp == 0xf0, "balanced caller stack");
    }
};

struct Settings { word vars{0x03b5}, helpers{0x9000}; unsigned count{8}; };
struct Fixture {
    std::filesystem::path directory;
    std::vector<std::filesystem::path> files;
    Fixture() {
        for (unsigned i{};; ++i) {
            directory = std::filesystem::current_path() / ("atlas-effects-test-" +
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
            {"rom_iscripts_begin", s.helpers + 0x100U}, {"rom_hud_draw_timer", s.helpers + 0x140U}})
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
    const bool vars = std::find(library.begin(), library.end(), OPS[1]) != library.end()
        || std::find(library.begin(), library.end(), fh::HackLib::AtlasDevSetVar) != library.end();
    check((image.rom[file(settings.helpers + 0x100)] == 0x4c) == vars, "only variable consumers install begin reset");
    for (std::size_t i{}; i < image.rom.size(); ++i) if (image.rom[i] != before[i])
        check((i >= file(origin) && i < end_file) || (i >= file(0x8273) && i < file(0x8275))
            || (i >= file(0x8277) && i < file(0x8279))
            || (vars && i >= file(settings.helpers + 0x100) && i < file(settings.helpers + 0x106)), "installer changed an unowned byte");
    return image;
}
Cpu cpu(const Image& image) {
    Cpu c; std::copy_n(image.rom.begin() + file(0x8000), 0x4000, c.m.begin() + 0x8000);
    const auto h = image.settings.helpers;
    const auto put = [&](word address, std::initializer_list<byte> bytes) { std::copy(bytes.begin(), bytes.end(), c.m.begin() + address); };
    // loadbyte keeps x/carry; its final flags describe the pointer, not a.
    put(h, {0xa0,0,0xb1,0xdb,0xe6,0xdb,0xd0,2,0xe6,0xdc,0x60});
    const auto lo = static_cast<byte>(h), hi = static_cast<byte>(h >> 8);
    const auto next_lo = static_cast<byte>(h + 0xc0), next_hi = static_cast<byte>((h + 0xc0) >> 8);
    put(h + 0x40, {0x20,lo,hi,0x20,lo,hi,0x4c,next_lo,next_hi});
    put(h + 0x80, {0x20,lo,hi,0x48,0x20,lo,hi,0x85,0xdc,0x68,0x85,0xdb,0x4c,next_lo,next_hi});
    // observe the numeric hud input, then clobber scratch registers/flags.
    put(h + 0x140, {0x8d,0x00,0x02,0xee,0x01,0x02,0xa9,0x80,0xa2,0x97,0xa0,0xe3,0x38,0x60});
    return c;
}
void prepare(Cpu& c, byte effect, byte timer, byte flags = 0xd5, byte music = 0x8b, byte reg = 0) {
    std::fill_n(c.m.begin(), 0x800, 0x69); c.a = 0x46; c.x = 0x3b; c.y = 0x8c; c.p = 1; c.sp = 0xf0; c.writes.clear();
    c.m[0xdb] = 0xff; c.m[0xdc] = 5; c.m[0x5ff] = effect; c.m[0x600] = reg; c.m[0x601] = 0x06;
    for (word i = 0x427; i <= 0x42a; ++i) c.m[i] = static_cast<byte>(i * 19);
    c.m[0x427 + (effect & 3)] = timer; c.m[0xa5] = flags; c.m[0xfa] = music; c.m[0x3d1] = 0x23;
    c.m[0x200] = 0xff; c.m[0x201] = 0;
}
void execute(Cpu& c, const Image& image, unsigned which) {
    c.run(image.entry.at(OPS[which]), image.settings.helpers + 0xc0);
    const auto result = word(c.m[0xdb] | c.m[0xdc] << 8);
    const auto pointer = which == 0 ? (c.m[0x427 + (c.m[0x5ff] & 3)] < 128 ? 0x634 : 0x602)
        : which == 1 ? 0x601 : which == 2 ? 0x600 : 0x5ff;
    check(result == pointer, "branch destination and operand consumption");
    if (which < 2) check(c.m[0x200] == 0xff && c.m[0x201] == 0, "queries do not call the hud");
    for (const word at : c.writes)
        check((at >= 0x100 && at <= 0x1f0) || at == 0xdb || at == 0xdc || at == 0x200 || at == 0x201
            || (which == 1 && at >= image.settings.vars && at < image.settings.vars + image.settings.count)
            || (which >= 2 && ((at >= 0x427 && at <= 0x42a) || at == 0xa5 || at == 0xfa)), "unowned runtime store");
}
void clear_check(Cpu& c, const Image& image, unsigned which, byte effect, byte timer, byte flags, byte music) {
    const byte area_music = c.m[0x3d1];
    const std::array old{c.m[0x427], c.m[0x428], c.m[0x429], c.m[0x42a]}; execute(c, image, which);
    const auto target = effect & 3; const bool all = which == 3, wing = all || target == 2, hourglass = all || target == 3;
    for (word i{}; i < 4; ++i) check(c.m[0x427 + i] == (all || i == target ? 255 : old[i]), "clear only intended counters");
    check(c.m[0xa5] == (wing ? flags & 127 : flags), "preserve all other player status bits");
    check(c.m[0xfa] == (hourglass && old[3] < 128 && (music & 127) == 11 ? area_music : music), "hourglass music restoration policy");
    check(c.m[0x201] == (wing ? 1 : 0), "configured hud call count");
    if (wing) check(c.m[0x200] == 0, "hud numeric input must be zero");
    (void)timer;
}

void behavior(Fixture& fixture) {
    const auto image = install(fixture, {}, 0xae00, {OPS.begin(), OPS.end()}); auto c = cpu(image);
    for (unsigned effect{}; effect < 256; ++effect) for (unsigned timer{}; timer < 256; ++timer) {
        prepare(c, effect, timer, 0xd5, 0x8b, 0x34); execute(c, image, 0);
        check(c.m[0x427 + (effect & 3)] == timer && c.m[0xa5] == 0xd5 && c.m[0xfa] == 0x8b, "query is read-only");
        prepare(c, effect, timer); execute(c, image, 1);
        check(c.m[image.settings.vars] == (timer < 128 ? timer : 0), "active time read, including active zero");
        prepare(c, effect, timer); clear_check(c, image, 2, effect, timer, 0xd5, 0x8b);
    }
    for (unsigned flags{}; flags < 256; ++flags) for (unsigned music{}; music < 256; ++music) {
        for (const byte timer : {byte{0}, byte{0x80}, byte{0xff}}) {
            prepare(c, 3, timer, flags, music); clear_check(c, image, 3, 3, timer, flags, music);
        }
    }
    for (unsigned flags{}; flags < 256; ++flags) for (unsigned effect{}; effect < 4; ++effect)
        for (const byte music : {byte{0x0b}, byte{0x8b}, byte{0x47}}) {
            prepare(c, effect, 0, flags, music); clear_check(c, image, 2, effect, 0, flags, music);
        }
    for (unsigned timer{}; timer < 256; ++timer)
        for (const byte music : {byte{0x0b}, byte{0x8b}, byte{0x47}}) {
            prepare(c, 3, timer, 0xd5, music); clear_check(c, image, 3, 3, timer, 0xd5, music);
        }
    for (unsigned area_music{}; area_music < 256; ++area_music) for (const unsigned op : {2U, 3U})
        for (const byte timer : {byte{0}, byte{127}, byte{255}})
            for (const byte music : {byte{0x0b}, byte{0x8b}, byte{0x47}}) {
                prepare(c, 3, timer, 0xd5, music); c.m[0x3d1] = area_music;
                clear_check(c, image, op, 3, timer, 0xd5, music);
            }
    for (const unsigned op : {2U, 3U}) for (const byte music : {byte{0x0b}, byte{0x8b}}) {
        prepare(c, 3, 0, 0xd5, music); clear_check(c, image, op, 3, 0, 0xd5, music);
        c.m[0xdb] = 0xff; c.m[0xdc] = 5; c.m[0xfa] = music; c.m[0x201] = 0; c.writes.clear();
        clear_check(c, image, op, 3, 255, c.m[0xa5], music);
    }
}
void configuration(Fixture& fixture) {
    for (const Settings s : {Settings{0x310,0x9000,1}, Settings{0x3b5,0x9400,8}, Settings{0x500,0x9000,128}})
        for (const word origin : {word{0xa800}, word{0xb900}}) {
            const auto image = install(fixture, s, origin, {OPS.begin(), OPS.end()}); auto c = cpu(image);
            for (unsigned reg{}; reg < 256; ++reg) for (const byte timer : {byte{0}, byte{127}, byte{128}, byte{255}}) {
                prepare(c, 0xfe, timer, 0x51, 0x47, reg); execute(c, image, 1);
                if (reg >= s.count) check(std::none_of(c.writes.begin(), c.writes.end(), [&](word at) {
                    return at >= s.vars && at < s.vars + s.count;
                }), "invalid registers do not write the variable block");
                for (unsigned i{}; i < s.count; ++i)
                    check(c.m[s.vars + i] == (reg < s.count && i == reg ? timer < 128 ? timer : 0 : 0x69), "configured register domain");
            }
            for (const byte a : {byte{0}, byte{0x45}, byte{0xff}}) {
                prepare(c, 0, 0); c.a = a; c.run(s.helpers + 0x100, s.helpers + 0x106);
                for (unsigned i{}; i < s.count; ++i) check(c.m[s.vars + i] == 0, "begin clears configured variables");
                check(c.a == (a == 255 ? 31 : a), "begin reproduces displaced accumulator behavior");
                for (const word at : c.writes) check((at >= s.vars && at < s.vars + s.count)
                    || (at >= 0x100 && at <= 0x1f0), "begin reset writes only variables and its stack");
            }
        }
    for (const auto opcode : OPS) {
        const auto image = install(fixture, {}, 0xae00, {opcode});
        const auto table = read_word(image.rom, file(0x8277));
        check(image.entry.at(opcode) - image.begin == (opcode == OPS[1] ? 21 : 0),
              "get installs only the variable reset; other effects add no shared helper");
        std::cout << "handler " << magic_enum::enum_name(opcode) << ": " << table - image.entry.at(opcode)
                  << " bytes; shared initialization " << image.entry.at(opcode) - image.begin << " bytes\n";
    }
    for (const unsigned count : {0U, 129U}) {
        for (const unsigned i : {0U, 2U, 3U}) install(fixture, {0x3b5,0x9000,count}, 0xae00, {OPS[i]});
        bool refused = false; try { install(fixture, {0x3b5,0x9000,count}, 0xae00, {OPS[1]}); }
        catch (const std::runtime_error& e) { refused = std::string(e.what()).find("count") != std::string::npos; }
        check(refused, "variable consumer enforces native count bounds");
    }
    const auto alone = install(fixture, {}, 0xa800, {OPS.begin(), OPS.end()});
    const auto combined = install(fixture, {}, 0xb100, {fh::HackLib::AtlasDevApplyEffect, OPS[3], OPS[1], fh::HackLib::AtlasDevSetVar, OPS[0], OPS[2]});
    auto a = cpu(alone), b = cpu(combined);
    for (unsigned op{}; op < 4; ++op) for (unsigned effect{}; effect < 256; ++effect) {
        prepare(a, effect, effect, 0xe7, 0x8b, op == 0 ? 0x34 : 3); prepare(b, effect, effect, 0xe7, 0x8b, op == 0 ? 0x34 : 3);
        execute(a, alone, op); execute(b, combined, op);
        for (const word at : {word{0xa5},word{0xfa},word{0x427},word{0x428},word{0x429},word{0x42a},word{0x3b8},word{0xdb},word{0xdc},word{0x200},word{0x201}})
            check(a.m[at] == b.m[at], "relocated mixed library behavior parity");
    }
}

void public_selection(Fixture& fixture) {
    const auto cfg = fixture.config(Bytes(0x40010, 0xff), {});
    const std::array<std::string, 4> names{"AtlasDevIfEffectActive", "AtlasDevGetEffectTime", "AtlasDevClearTimedEffect", "AtlasDevClearTimedEffects"};
    std::map<byte, std::string> selected;
    for (byte i{}; i < names.size(); ++i) {
        selected[i] = "Impl=" + names[i];
        check(klib::str::parse_enum_ci<fh::HackLib>(names[i]) == OPS[i], "public implementation name resolves");
        auto lower = names[i]; std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
        check(klib::str::parse_enum_ci<fh::HackLib>(lower) == OPS[i], "public implementation names are case insensitive");
    }
    const auto info = fi::load_iscript_opcodes_from_config(selected, cfg.str_map("iscript_opcode_impls"));
    check(info.required_impls == std::vector<std::string>(names.begin(), names.end()) && info.base_opcode_count == 0,
          "public configuration selects all four implementation names");
    for (byte i{}; i < names.size(); ++i) {
        const auto& op = info.opcodes.at(i);
        check(op.size() == std::array<unsigned, 4>{4,3,2,1}[i], "public opcode operand width");
        check(op.flow == (i == 0 ? fi::Flow::Jump : fi::Flow::Continue), "public opcode control flow");
    }
}
void executor_test() {
    Cpu c; c.m[0x8000]=0xa9; c.m[0x8001]=0x80; c.m[0x8002]=0x48; c.m[0x8003]=0xa9; c.m[0x8004]=0;
    c.m[0x8005]=0x68; c.m[0x8006]=0xc9; c.m[0x8007]=0x80; c.m[0x8008]=0xf0; c.m[0x8009]=2;
    c.m[0x800a]=0; c.m[0x800b]=0; c.run(0x8000,0x800c); check(c.a==128 && (c.p&3)==3, "executor flags, branch, push/pop");
    bool rejects=false; try { c.pc=0x8100; c.step(); } catch(const std::runtime_error&) { rejects=true; } check(rejects,"executor rejects unknown instructions");
}
}
int main() {
    try { executor_test(); Fixture fixture; public_selection(fixture); behavior(fixture); configuration(fixture);
        std::cout << "atlas_timed_effects_regression: " << checks << " checks, " << executions << " executions passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr << "atlas_timed_effects_regression: " << e.what() << '\n'; return 1; }
}
