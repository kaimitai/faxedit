#include "common/klib/IPS_Patch.h"
#include "common/klib/Kfile.h"
#include "fb/BScriptReader.h"
#include "fe/Config.h"
#include "fe/xml/Xml_helper.h"
#include "fh/GeneralHack.h"
#include "fh/HackManager.h"
#include <chrono>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <sys/resource.h>
#endif

namespace {
	int failures{ 0 };

	void check(bool p_condition, const std::string& p_message) {
		if (!p_condition) {
			std::cerr << "FAIL: " << p_message << '\n';
			++failures;
		}
	}

	void expect_throw(const std::function<void()>& p_action,
		const std::string& p_message_fragment, const std::string& p_context) {
		try {
			p_action();
			check(false, p_context + " did not throw");
		}
		catch (const std::exception& error) {
			check(std::string(error.what()).find(p_message_fragment) != std::string::npos,
				std::format("{} reported unexpected error: {}", p_context, error.what()));
		}
	}

	class TestDirectory {
		std::filesystem::path path_;

	public:
		TestDirectory() {
			const auto id{ std::chrono::steady_clock::now().time_since_epoch().count() };
			path_ = std::filesystem::temp_directory_path()
				/ ("faxedit-data-integrity-" + std::to_string(id));
			std::filesystem::create_directory(path_);
		}

		~TestDirectory() {
			std::error_code ignored;
			std::filesystem::permissions(path_, std::filesystem::perms::owner_all,
				std::filesystem::perm_options::add, ignored);
			std::filesystem::remove_all(path_, ignored);
		}

		const std::filesystem::path& path() const { return path_; }
	};

#ifndef _WIN32
	class FileSizeLimit {
		struct rlimit previous_{};
		using SignalHandler = void (*)(int);
		SignalHandler previous_handler_{};

	public:
		FileSizeLimit() {
			if (getrlimit(RLIMIT_FSIZE, &previous_) != 0)
				throw std::runtime_error("Could not read RLIMIT_FSIZE");
			previous_handler_ = std::signal(SIGXFSZ, SIG_IGN);
			struct rlimit limited{ previous_ };
			limited.rlim_cur = 1;
			if (setrlimit(RLIMIT_FSIZE, &limited) != 0) {
				std::signal(SIGXFSZ, previous_handler_);
				throw std::runtime_error("Could not set RLIMIT_FSIZE");
			}
		}

		~FileSizeLimit() {
			setrlimit(RLIMIT_FSIZE, &previous_);
			std::signal(SIGXFSZ, previous_handler_);
		}
	};
#endif

	std::string read_text(const std::filesystem::path& p_path) {
		const auto bytes{ klib::file::read_file_as_bytes(p_path.string()) };
		return std::string(bytes.begin(), bytes.end());
	}

	void test_atomic_writers_and_xml() {
		TestDirectory test_dir;
		const auto bytes_path{ test_dir.path() / "output.nes" };
		klib::file::write_bytes_to_file({ 1, 2, 3 }, bytes_path.string());
		klib::file::write_bytes_to_file({ 4, 5 }, bytes_path.string());
		check(klib::file::read_file_as_bytes(bytes_path.string()) == std::vector<byte>({ 4, 5 }),
			"byte writer did not replace a complete file");

		const auto text_path{ test_dir.path() / "output.asm" };
		klib::file::write_string_to_file("old", text_path.string());
		klib::file::write_string_to_file("new\ntext\n", text_path.string());
		check(read_text(text_path) == "new\ntext\n", "text writer did not replace a complete file");

#ifndef _WIN32
		const auto protected_path{ test_dir.path() / "project.xml" };
		klib::file::write_string_to_file("previous-valid-project", protected_path.string());

		pugi::xml_document document;
		document.append_child("faxanadu").append_attribute("version") = "test";
		{
			FileSizeLimit limit;
			expect_throw([&] { fe::xml::save_xml_file(document, protected_path.string()); },
				"file", "XML write after an I/O failure");
		}

		check(read_text(protected_path) == "previous-valid-project",
			"failed XML save destroyed the previous project");
		std::size_t entry_count{ 0 };
		for (const auto& entry : std::filesystem::directory_iterator(test_dir.path())) {
			(void)entry;
			++entry_count;
		}
		check(entry_count == 3, "failed XML save left a temporary file behind");
#endif

		pugi::xml_document formatting_document;
		auto root{ formatting_document.append_child("root") };
		root.append_child("value").text().set("unchanged formatting");
		const auto legacy_path{ test_dir.path() / "legacy-save.xml" };
		const auto atomic_path{ test_dir.path() / "atomic-save.xml" };
		check(formatting_document.save_file(legacy_path.string().c_str()),
			"pugixml compatibility fixture could not be saved");
		fe::xml::save_xml_file(formatting_document, atomic_path.string());
		check(klib::file::read_file_as_bytes(legacy_path.string())
			== klib::file::read_file_as_bytes(atomic_path.string()),
			"atomic XML serialization changed the existing document format");

#ifndef _WIN32
		const auto private_path{ test_dir.path() / "private-project.xml" };
		klib::file::write_string_to_file("private-old", private_path.string());
		const auto private_permissions{
			std::filesystem::perms::owner_read | std::filesystem::perms::owner_write };
		std::filesystem::permissions(private_path, private_permissions,
			std::filesystem::perm_options::replace);
		klib::file::write_string_to_file("private-new", private_path.string());
		check(std::filesystem::status(private_path).permissions() == private_permissions,
			"atomic replacement changed existing file permissions");

		const auto symlink_target{ test_dir.path() / "symlink-target.xml" };
		const auto symlink_path{ test_dir.path() / "project-link.xml" };
		klib::file::write_string_to_file("target-unchanged", symlink_target.string());
		std::filesystem::create_symlink(symlink_target.filename(), symlink_path);
		expect_throw([&] {
			klib::file::write_string_to_file("must-not-be-written", symlink_path.string());
		}, "symbolic link", "atomic replacement through a symbolic link");
		check(std::filesystem::is_symlink(symlink_path),
			"rejected atomic replacement removed the symbolic link");
		check(read_text(symlink_target) == "target-unchanged",
			"rejected atomic replacement modified the symlink target");
#endif
	}

	void check_ips_round_trip(const std::vector<byte>& p_source,
		const std::vector<byte>& p_target, const std::string& p_context) {
		const auto patch{ klib::ips::generate_patch(p_source, p_target) };
		check(klib::ips::apply_patch(p_source, patch) == p_target,
			p_context + " did not round-trip exactly");
	}

	void test_ips_record_splitting() {
		std::vector<byte> source(70000, 0xaa);
		std::vector<byte> literal(70000);
		for (std::size_t i{ 0 }; i < literal.size(); ++i) {
			const unsigned int value{ static_cast<unsigned int>(i % 255) };
			literal[i] = static_cast<byte>(value >= 0xaa ? value + 1 : value);
		}
		check_ips_round_trip(source, literal, "70,000-byte literal hunk");

		std::vector<byte> rle(70000, 0x55);
		check_ips_round_trip(source, rle, "70,000-byte RLE hunk");

		std::vector<byte> append_source(32, 0x11);
		std::vector<byte> appended{ append_source };
		appended.insert(appended.end(), literal.begin(), literal.end());
		check_ips_round_trip(append_source, appended, "70,000-byte appended hunk");

		constexpr std::size_t eof_offset{ 0x454f46 };
		std::vector<byte> eof_source(eof_offset + 1, 0x00);
		auto eof_target{ eof_source };
		eof_target[eof_offset] = 0x7f;
		check_ips_round_trip(eof_source, eof_target, "hunk at the IPS EOF-marker offset");

		eof_source.pop_back();
		eof_target = eof_source;
		eof_target.insert(eof_target.end(), { 0x12, 0x34, 0x56 });
		check_ips_round_trip(eof_source, eof_target, "append at the IPS EOF-marker offset");
	}

	std::string bscript_config_xml() {
		return R"xml(<?xml version="1.0"?>
<eoe_config>
  <regions><region name="test" /></regions>
  <pointers><pointer name="bscript_ptr" value="0" zero_addr="0" /></pointers>
  <consts>
    <const name="sprite_count" value="1" />
    <const name="bscript_data_rg1_end" value="256" />
    <const name="bscript_data_rg2_start" value="512" />
    <const name="bscript_data_rg2_end" value="768" />
  </consts>
  <byte_to_string_maps>
    <byte_to_string_map name="bscript_opcodes">
      <entry byte="$06" str="Mnemonic=AddValue,Arg=ram:value" />
      <entry byte="$ff" str="Mnemonic=End,Flow=End" />
    </byte_to_string_map>
    <byte_to_string_map name="bscript_behaviors">
      <entry byte="$01" str="Mnemonic=Behavior_Wait,Arg=ticks" />
    </byte_to_string_map>
  </byte_to_string_maps>
</eoe_config>
)xml";
	}

	void test_bscript_validation() {
		TestDirectory test_dir;
		const auto config_path{ test_dir.path() / "bscript-config.xml" };
		klib::file::write_string_to_file(bscript_config_xml(), config_path.string());
		const fe::Config config(config_path.string(), "", {}, "test");

		expect_throw([&] {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[bscript]", ".entrypoint 0",
				"Behavior_Wait ticks=256", "End" }, config);
		}, "outside 0..255", "out-of-range BScript byte operand");
		expect_throw([&] {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[bscript]", ".entrypoint 0",
				"AddValue ram=$0200 value=-129", "End" }, config);
		}, "outside -128..255", "out-of-range signed BScript byte operand");

		expect_throw([&] {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[bscript]", ".entrypoint 0", ".entrypoint 0", "End" }, config);
		}, "Duplicate entrypoint 0", "duplicate BScript entrypoint");

		expect_throw([&] {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[bscript]", ".entrypoint 0", "@again:", "@again:", "End" }, config);
		}, "Duplicate label '@again'", "duplicate BScript label");
		expect_throw([&] {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[defines]", "define DELAY 1", "define DELAY 2",
				"[bscript]", ".entrypoint 0", "Behavior_Wait ticks=DELAY", "End" }, config);
		}, "Duplicate define 'DELAY'", "duplicate BScript define");
		expect_throw([&] {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[bscript]", ".entrypoint 0",
				"Behavior_Wait ticks=1 ticks=2", "End" }, config);
		}, "Argument type ticks specified more than once", "duplicate BScript named operand");

		try {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[defines]", "define DELAY 1", "[bscript]",
				".entrypoint 0", "Behavior_Wait ticks=DELAY", "End" }, config);
			reader.read_asm({ "[defines]", "define DELAY 2", "[bscript]",
				".entrypoint 0", "Behavior_Wait ticks=DELAY", "End" }, config);
			const auto [first_region, second_region]{ reader.to_bytes() };
			check(first_region == std::vector<byte>({ 0x02, 0x00, 0x00, 0x01, 0x02, 0xff }),
				"reused BScript reader retained a stale define value");
			check(second_region.empty(), "reused small BScript unexpectedly used region 2");
		}
		catch (const std::exception& error) {
			check(false, std::string("reusing BScript reader failed: ") + error.what());
		}

		try {
			fb::BScriptReader reader(config);
			reader.read_asm({ "[bscript]", ".entrypoint 0",
				"AddValue ram=$0200 value=-1", "End" }, config);
			const auto [first_region, second_region]{ reader.to_bytes() };
			check(first_region == std::vector<byte>({ 0x02, 0x00, 0x06, 0x00, 0x02, 0xff, 0xff }),
				"valid signed BScript byte did not encode as $ff");
			check(second_region.empty(), "small signed-byte BScript unexpectedly used region 2");
		}
		catch (const std::exception& error) {
			check(false, std::string("valid signed BScript byte was rejected: ") + error.what());
		}
	}

	void test_config_and_parameter_validation() {
		check(fe::xml::parse_numeric("12") == 12, "valid decimal value changed");
		check(fe::xml::parse_numeric("$ff") == 255, "valid hexadecimal value changed");
		expect_throw([] { (void)fe::xml::parse_numeric("12A"); },
			"Invalid digit", "hexadecimal digit in decimal XML value");
		expect_throw([] { (void)fe::xml::parse_numeric("$"); },
			"no digits", "empty dollar-prefixed XML value");
		expect_throw([] { (void)fe::xml::parse_numeric("0x"); },
			"no digits", "empty 0x-prefixed XML value");
		const std::string overflow{ std::to_string(std::numeric_limits<std::size_t>::max()) + "0" };
		expect_throw([&] { (void)fe::xml::parse_numeric(overflow); },
			"out of range", "overflowing XML value");

		TestDirectory test_dir;
		const auto config_path{ test_dir.path() / "regions.xml" };
		klib::file::write_string_to_file(bscript_config_xml(), config_path.string());
		expect_throw([&] { fe::Config config(config_path.string(), "", {}, "typo"); },
			"Unknown ROM region 'typo'", "unknown region override");

		expect_throw([] {
			(void)fh::parse_general_hacks("AtlasDevDayNightCycle lenght=7200");
		}, "Unknown parameter 'lenght'", "unknown general-hack parameter");
		expect_throw([] {
			const auto hacks{ fh::parse_general_hacks("FogRules rules=256:300") };
			(void)hacks.at(0).split_byte_optional_byte("rules");
		}, "not a valid byte", "wrapping FogRules values");
	}

	void test_general_hack_install_is_transactional() {
		std::vector<byte> rom(0x40010, 0xff);
		const auto original{ rom };
		const fe::Config config;
		const fh::HackManager manager;
		const std::vector<fh::GeneralHack> hacks{ fh::GeneralHack("KillSwitch") };

		expect_throw([&] {
			(void)manager.install_general_hacks(config, rom, 15, 0xc000, 0xc000, hacks);
		}, "Hack overflow", "general-hack capacity failure");
		check(rom == original, "failed general-hack installation modified the caller's ROM");

		expect_throw([&] {
			(void)manager.install_general_hacks(config, rom, 15, 0xfff1, 0xffff, hacks);
		}, "Hack address wrapped", "general-hack address wrap");
		check(rom == original, "wrapped general-hack installation modified the caller's ROM");
		expect_throw([&] {
			(void)manager.install_general_hacks(config, rom, 15, 0x10000, 0x10000, hacks);
		}, "Invalid general-hack CPU range", "truncated general-hack start address");
		check(rom == original, "invalid general-hack range modified the caller's ROM");
		expect_throw([&] {
			(void)manager.install_general_hacks(config, rom, 15, 0xbfff, 0xc100, hacks);
		}, "Invalid general-hack CPU range", "bank-15 range below the fixed-bank window");
		check(rom == original, "below-window general-hack range modified the caller's ROM");
		expect_throw([&] {
			(void)manager.install_general_hacks(config, rom, 14, 0xbff0, 0xc001, hacks);
		}, "Invalid general-hack CPU range", "bank-14 range crossing the switchable-bank window");
		check(rom == original, "cross-window general-hack range modified the caller's ROM");
		expect_throw([&] {
			(void)manager.install_general_hacks(config, rom, 13, 0x8000, 0x8100, hacks);
		}, "Unsupported general-hack PRG bank", "unsupported general-hack bank");
		check(rom == original, "unsupported general-hack bank modified the caller's ROM");

		const auto installed_size{
			manager.install_general_hacks(config, rom, 15, 0xc000, 0xc100, hacks) };
		check(installed_size > 0 && rom != original,
			"successful general-hack transaction was not committed");
	}
}

int main() {
	test_atomic_writers_and_xml();
	test_ips_record_splitting();
	test_bscript_validation();
	test_config_and_parameter_validation();
	test_general_hack_install_is_transactional();

	if (failures == 0)
		std::cout << "All data-integrity tests passed\n";
	return failures == 0 ? 0 : 1;
}
