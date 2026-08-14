#include "AtlasMovieRuntime.h"
#include "AtlasMovieAssets.h"
#include "AtlasMovieBundle.h"
#include "AtlasMovieEngine.h"
#include "common/klib/Asm6502.h"
#include "common/klib/Kstring.h"
#include <array>
#include <format>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

	constexpr std::uint16_t LINK_BASE{ 0x8000 };

	constexpr std::array<byte, fe::AtlasMovieRuntime::STANDALONE_PLAYER_BYTES>
		STANDALONE_PLAYER{
#include "AtlasMovieStandaloneData.inc"
	};

#include "AtlasMovieStandaloneRelocations.inc"

	void require(bool p_condition, const std::string& p_message) {
		if (!p_condition)
			throw std::runtime_error("AtlasDevPlayMovie: " + p_message);
	}

	void relocate_word(std::vector<byte>& p_code, std::size_t p_offset, int p_delta) {
		require(p_offset + 1 < p_code.size(), "internal relocation is outside the player");
		std::uint16_t value{ static_cast<std::uint16_t>(
			p_code[p_offset] | p_code[p_offset + 1] << 8) };
		value = static_cast<std::uint16_t>(value + p_delta);
		p_code[p_offset] = static_cast<byte>(value);
		p_code[p_offset + 1] = static_cast<byte>(value >> 8);
	}

	bool named(const fi::Opcode& p_opcode, const std::string& p_name) {
		return klib::str::to_lower(p_opcode.name) == klib::str::to_lower(p_name);
	}

	std::string xml_escape(const std::string& p_text) {
		std::string result;
		for (const char ch : p_text) {
			switch (ch) {
			case '&': result += "&amp;"; break;
			case '<': result += "&lt;"; break;
			case '>': result += "&gt;"; break;
			case '"': result += "&quot;"; break;
			case '\'': result += "&apos;"; break;
			default: result += ch; break;
			}
		}
		return result;
	}

	std::string explicit_definition(const fi::Opcode& p_opcode) {
		std::string result{ "Mnemonic=" + p_opcode.name };
		if (!p_opcode.args.empty()) {
			result += ",Args=";
			for (std::size_t i{}; i < p_opcode.args.size(); ++i) {
				if (i) result += '+';
				result += klib::str::enum_to_string(p_opcode.args[i].type);
				if (p_opcode.args[i].domain != fi::ArgDomain::None)
					result += ":" + klib::str::enum_to_string(p_opcode.args[i].domain);
			}
		}
		if (p_opcode.flow != fi::Flow::Continue)
			result += ",Flow=" + klib::str::enum_to_string(p_opcode.flow);
		if (p_opcode.ends_stream)
			result += ",Terminal=true";
		return result;
	}

	bool canonical_shared_opcode(const fi::Opcode& p_opcode) {
		return p_opcode.args == std::vector<fi::Argument>{
			{ fi::ArgType::Byte, fi::ArgDomain::None } }
			&& p_opcode.flow == fi::Flow::End && p_opcode.ends_stream;
	}

}

std::vector<byte> fe::AtlasMovieRuntime::build_standalone(
	const std::vector<byte>& p_fmb, std::uint16_t p_cpu_addr) {
	require(p_cpu_addr >= 0x8000, "player starts below bank 12's CPU window");
	const auto bundle{ AtlasMovieBundleCodec::parse(p_fmb) };
	for (const auto& movie : bundle.movies)
		require(movie.imports.empty(),
			"standalone mode currently supports ROM-owned assets only; use Shared mode for imported graphics");

	std::vector<byte> code{ STANDALONE_PLAYER.begin(), STANDALONE_PLAYER.end() };
	const int delta{ static_cast<int>(p_cpu_addr) - LINK_BASE };
	for (const auto offset : GENERATED_ABSOLUTE_RELOCATIONS)
		relocate_word(code, offset, delta);

	for (const auto& [low, high] : GENERATED_SPLIT_RELOCATIONS) {
		require(low < code.size() && high < code.size(),
			"internal split relocation is outside the player");
		std::uint16_t value{ static_cast<std::uint16_t>(
			code[low] | code[high] << 8) };
		value = static_cast<std::uint16_t>(value + delta);
		code[low] = static_cast<byte>(value);
		code[high] = static_cast<byte>(value >> 8);
	}

	const auto end_cpu{ static_cast<std::size_t>(p_cpu_addr)
		+ code.size() + p_fmb.size() };
	require(end_cpu <= 0xc000,
		"player and FMB escape bank 12");
	validate_standalone_sources(p_fmb, p_cpu_addr,
		static_cast<std::uint16_t>(end_cpu));
	code.insert(code.end(), p_fmb.begin(), p_fmb.end());
	return code;
}

std::uint16_t fe::AtlasMovieRuntime::install_standalone(const Config& p_config,
	std::vector<byte>& p_rom, std::uint16_t p_cpu_addr) {
	require(p_config.get_region() == "us",
		"the standalone runtime is deliberately pinned to USA Rev 0");
	const auto code{ build_standalone(p_config.vset("hack_movie_data"), p_cpu_addr) };
	validate_standalone_sources(p_config.vset("hack_movie_data"), p_rom,
		p_cpu_addr, static_cast<std::uint16_t>(p_cpu_addr + code.size()));
	klib::Asm6502::apply_bytes(p_rom, code, 12, p_cpu_addr);
	return static_cast<std::uint16_t>(p_cpu_addr + code.size());
}

void fe::AtlasMovieRuntime::validate_standalone_sources(
	const std::vector<byte>& p_fmb, const std::vector<byte>& p_rom,
	std::uint16_t p_cpu_begin, std::uint16_t p_cpu_end) {
	validate_standalone_sources(p_fmb, p_cpu_begin, p_cpu_end);
	const auto bundle{ AtlasMovieBundleCodec::parse(p_fmb) };
	atlas_movie::validate_movie_oam_budget(p_rom, bundle);
	atlas_movie::reject_movie_source_overlaps(p_rom, bundle, {
		{ atlas_movie::rom_offset(12, p_cpu_begin),
			static_cast<std::size_t>(p_cpu_end - p_cpu_begin), "Standalone allocation" }
	}, "AtlasDevPlayMovie Standalone allocation");
}

void fe::AtlasMovieRuntime::validate_standalone_sources(
	const std::vector<byte>& p_fmb, std::uint16_t p_cpu_begin,
	std::uint16_t p_cpu_end) {
	require(p_cpu_begin >= 0x8000 && p_cpu_end >= p_cpu_begin
		&& p_cpu_end <= 0xc000, "standalone allocation escapes bank 12");
	const auto bundle{ AtlasMovieBundleCodec::parse(p_fmb) };
	for (const auto& movie : bundle.movies) {
		for (const auto& asset : movie.assets) {
			if (asset.bank != 12) continue;
			const auto asset_end{ static_cast<std::size_t>(asset.cpu) + asset.bytes };
			require(asset_end <= p_cpu_begin || asset.cpu >= p_cpu_end,
				"standalone player overwrites a bank-12 movie asset source");
		}
		for (const auto pointer : {
			movie.metasprite_pointer_lo, movie.metasprite_pointer_hi }) {
			const auto pointer_end{ static_cast<std::size_t>(pointer)
				+ movie.metasprite_count };
			require(pointer_end <= p_cpu_begin || pointer >= p_cpu_end,
				"standalone player overwrites a bank-12 metasprite pointer table");
		}
	}
}

fi::ScriptOpcodeInfo fe::AtlasMovieRuntime::resolve_opcode_info(
	fi::ScriptOpcodeInfo p_info, const std::vector<byte>& p_rom) {
	const bool shared_installed{ AtlasMovieEngine::is_installed(p_rom) };
	bool has_standalone{ false }, has_shared{ false };
	std::optional<byte> shared_byte;
	for (const auto& [opcode_byte, opcode] : p_info.opcodes) {
		has_standalone |= named(opcode, "AtlasDevPlayMovie");
		if (named(opcode, "AtlasDevPlayMovieShared")) {
			has_shared = true;
			shared_byte = opcode_byte;
		}
	}

	require(!(has_standalone && (has_shared || shared_installed)),
		"Standalone and Shared movie modes are mutually exclusive");
	require(!has_shared || shared_installed,
		"AtlasDevPlayMovieShared requires an installed Atlas Movie Engine");
	if (!shared_installed)
		return p_info;
	if (has_shared) {
		require(shared_byte == 0x18,
			"AtlasDevPlayMovieShared must remain the preinstalled opcode at $18");
		require(canonical_shared_opcode(p_info.opcodes.at(*shared_byte)),
			"AtlasDevPlayMovieShared must use one Byte argument and terminal End flow");
		return p_info;
	}
	require(p_info.opcodes.size() == 0x18 && p_info.required_impls.empty(),
		"AME must be installed before generated extended iScript opcodes");
	p_info.opcodes.emplace(0x18, fi::Opcode("AtlasDevPlayMovieShared",
		{ { fi::ArgType::Byte, fi::ArgDomain::None } }, fi::Flow::End, true));
	p_info.base_opcode_count = 0x19;
	return p_info;
}

void fe::AtlasMovieRuntime::validate_shared_install(
	const fi::ScriptOpcodeInfo& p_info) {
	for (const auto& [opcode_byte, opcode] : p_info.opcodes) {
		(void)opcode_byte;
		require(!named(opcode, "AtlasDevPlayMovie")
			&& !named(opcode, "AtlasDevPlayMovieShared"),
			"remove the existing movie opcode configuration before installing Shared mode");
	}
	require(p_info.opcodes.size() == 0x18 && p_info.required_impls.empty(),
		"install Shared mode before configuring generated extended iScript opcodes");
}

std::string fe::AtlasMovieRuntime::standalone_config_override(
	const fi::ScriptOpcodeInfo& p_info, const std::vector<byte>& p_fmb) {
	(void)build_standalone(p_fmb, LINK_BASE);
	require(p_info.opcodes.size() >= p_info.required_impls.size(),
		"opcode implementation metadata is inconsistent");
	for (const auto& [opcode_byte, opcode] : p_info.opcodes) {
		(void)opcode_byte;
		require(!named(opcode, "AtlasDevPlayMovieShared"),
			"Shared mode cannot be exported as Standalone");
	}

	auto opcodes{ p_info.opcodes };
	auto implementations{ p_info.required_impls };
	std::optional<byte> existing_byte;
	for (const auto& [opcode_byte, opcode] : opcodes) {
		if (named(opcode, "AtlasDevPlayMovie"))
			existing_byte = opcode_byte;
	}
	if (existing_byte) {
		require(*existing_byte >= p_info.base_opcode_count,
			"AtlasDevPlayMovie exists as a preinstalled opcode instead of a generated Impl");
		const auto impl_index{ static_cast<std::size_t>(*existing_byte)
			- p_info.base_opcode_count };
		require(impl_index < implementations.size()
			&& klib::str::to_lower(implementations[impl_index]) == "atlasdevplaymovie",
			"AtlasDevPlayMovie opcode metadata does not select its generated Impl");
	}
	else {
		require(opcodes.size() < 256, "no free iScript opcode byte remains");
		const auto next{ static_cast<byte>(opcodes.size()) };
		opcodes.emplace(next, fi::Opcode("AtlasDevPlayMovie",
			{ { fi::ArgType::Byte, fi::ArgDomain::None } }, fi::Flow::End, true));
		implementations.push_back("AtlasDevPlayMovie");
	}
	const std::size_t base_count{ opcodes.size() - implementations.size() };

	std::ostringstream xml;
	xml << "<!-- Generated by Atlas Movie Creator. Use as eoe_config_override.xml,\n"
		"     or merge these sections into an existing override. Standalone mode\n"
		"     does not replace Faxanadu's internal cinematic engine. -->\n"
		"<eoe_config>\n\t<sets>\n\t\t<set name=\"hack_movie_data\" region=\"us\" values=\"";
	for (std::size_t i{}; i < p_fmb.size(); ++i) {
		if (i) xml << ',';
		xml << std::format("${:02x}", p_fmb[i]);
	}
	xml << "\" />\n\t</sets>\n\t<byte_to_string_maps>\n"
		"\t\t<byte_to_string_map name=\"iscript_opcodes\">\n";
	for (const auto& [opcode_byte, opcode] : opcodes) {
		std::string definition;
		if (opcode_byte >= base_count) {
			const auto& implementation{ implementations.at(opcode_byte - base_count) };
			definition = "Impl=" + implementation;
			if (!named(opcode, implementation))
				definition += ",Mnemonic=" + opcode.name;
		}
		else definition = explicit_definition(opcode);
		xml << std::format("\t\t\t<entry byte=\"${:02x}\" str=\"{}\" />\n",
			opcode_byte, xml_escape(definition));
	}
	xml << "\t\t</byte_to_string_map>\n\t</byte_to_string_maps>\n</eoe_config>\n";
	return xml.str();
}
