#include "AtlasMovieEngine.h"
#include "AtlasMovieAssets.h"
#include "AtlasMovieBundle.h"
#include "AtlasMovieLayout.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

using word = unsigned short;

namespace {

	constexpr word CORE_CPU{ fe::atlas_movie::layout::CORE_CPU };
	constexpr std::size_t HEADER_BYTES{ fe::atlas_movie::layout::HEADER_BYTES };
	constexpr std::size_t PRG_BANK_BYTES{
		fe::atlas_movie::layout::PRG_BANK_BYTES };
	constexpr word HANDLER_CPU{ fe::atlas_movie::layout::HANDLER_CPU };
	constexpr word TAIL_CPU{ fe::atlas_movie::layout::TAIL_CPU };
	constexpr word BUNDLE_CPU{ fe::atlas_movie::layout::BUNDLE_CPU };
	constexpr std::size_t CORE_BYTES{ fe::atlas_movie::layout::CORE_BYTES };
	constexpr std::size_t TAIL_BYTES{ fe::atlas_movie::layout::TAIL_BYTES };
	constexpr std::size_t DISPATCH_ENTRIES{
		fe::atlas_movie::layout::DISPATCH_ENTRIES };

#include "AtlasMovieEngineData.inc"

	static_assert(CORE_BYTES == GENERATED_CORE.size());
	static_assert(TAIL_BYTES == GENERATED_TAIL.size());

	std::size_t file_offset(byte p_bank, word p_cpu) {
		return fe::atlas_movie::layout::file_offset(p_bank, p_cpu);
	}

	void require_span(const std::vector<byte>& p_data, std::size_t p_offset,
		std::size_t p_size, const std::string& p_label) {
		if (p_offset > p_data.size() || p_size > p_data.size() - p_offset)
			throw std::runtime_error("Atlas Movie Engine truncated " + p_label);
	}

	word read_word(const std::vector<byte>& p_data, std::size_t p_offset) {
		require_span(p_data, p_offset, 2, "word");
		return static_cast<word>(p_data[p_offset] | p_data[p_offset + 1] << 8);
	}

	void write_word(std::vector<byte>& p_data, std::size_t p_offset, word p_value) {
		require_span(p_data, p_offset, 2, "ROM write");
		p_data[p_offset] = static_cast<byte>(p_value & 0xff);
		p_data[p_offset + 1] = static_cast<byte>(p_value >> 8);
	}

	void write_bytes(std::vector<byte>& p_rom, byte p_bank, word p_cpu,
		const byte* p_begin, std::size_t p_size) {
		const auto offset{ file_offset(p_bank, p_cpu) };
		require_span(p_rom, offset, p_size, "ROM write");
		std::copy_n(p_begin, p_size, p_rom.begin() + offset);
	}

	std::uint64_t fnv1a(const std::vector<byte>& p_rom, byte p_bank,
		word p_cpu, std::size_t p_size) {
		const auto offset{ file_offset(p_bank, p_cpu) };
		require_span(p_rom, offset, p_size, "signature");
		std::uint64_t result{ 0xcbf29ce484222325ULL };
		for (std::size_t i{ 0 }; i < p_size; ++i) {
			result ^= p_rom[offset + i];
			result *= 0x100000001b3ULL;
		}
		return result;
	}

	bool bytes_equal(const std::vector<byte>& p_rom, byte p_bank, word p_cpu,
		std::initializer_list<byte> p_expected) {
		const auto offset{ file_offset(p_bank, p_cpu) };
		if (offset + p_expected.size() > p_rom.size())
			return false;
		return std::equal(p_expected.begin(), p_expected.end(),
			p_rom.begin() + offset);
	}

	template<std::size_t N>
	bool bytes_equal(const std::vector<byte>& p_rom, byte p_bank, word p_cpu,
		const std::array<byte, N>& p_expected) {
		const auto offset{ file_offset(p_bank, p_cpu) };
		if (offset > p_rom.size() || p_expected.size() > p_rom.size() - offset)
			return false;
		return std::equal(p_expected.begin(), p_expected.end(),
			p_rom.begin() + static_cast<std::ptrdiff_t>(offset));
	}

	bool bytes_are(const std::vector<byte>& p_rom, byte p_bank, word p_cpu,
		std::size_t p_size, byte p_expected) {
		const auto offset{ file_offset(p_bank, p_cpu) };
		if (offset > p_rom.size() || p_size > p_rom.size() - offset)
			return false;
		return std::all_of(p_rom.begin() + static_cast<std::ptrdiff_t>(offset),
			p_rom.begin() + static_cast<std::ptrdiff_t>(offset + p_size),
			[p_expected](byte p_value) { return p_value == p_expected; });
	}

}

bool fe::AtlasMovieEngine::is_installed(const std::vector<byte>& p_rom) {
	try {
		if (!bytes_equal(p_rom, 15, 0xfc98,
				{ 0x20, 0x59, 0xf8, 0x0c, 0x07, 0xa7 })
			|| !bytes_equal(p_rom, 12, 0x82ae,
				{ 0x20, 0x0c, 0xa7, 0x4c, 0x13, 0xc9 })
			|| !bytes_equal(p_rom, 12, HANDLER_CPU,
				{ 0x20, 0xa4, 0x87, 0x4c, 0x13, 0xa7 })
			|| !bytes_equal(p_rom, 12, CORE_CPU, GENERATED_CORE)
			|| !bytes_equal(p_rom, 12, TAIL_CPU, GENERATED_TAIL))
			return false;

		const auto bundle_offset{ file_offset(12, BUNDLE_CPU) };
		const auto bank_end{ file_offset(12, 0xbfff) + 1 };
		if (bundle_offset >= p_rom.size() || bank_end > p_rom.size())
			return false;
		const std::vector<byte> bank_tail(
			p_rom.begin() + static_cast<std::ptrdiff_t>(bundle_offset),
			p_rom.begin() + static_cast<std::ptrdiff_t>(bank_end));
		const auto bundle_bytes{
			AtlasMovieBundleCodec::validated_prefix_size(bank_tail) };
		const auto bundle_end{ static_cast<std::size_t>(BUNDLE_CPU) + bundle_bytes };

		const auto low{ read_word(p_rom,
			file_offset(12, atlas_movie::layout::DISPATCH_LOW_REF)) };
		const auto high{ read_word(p_rom,
			file_offset(12, atlas_movie::layout::DISPATCH_HIGH_REF)) };
		if (low < bundle_end || high < low + DISPATCH_ENTRIES)
			return false;
		const auto opcode_count{ static_cast<std::size_t>(high - low) };
		if (static_cast<std::size_t>(high) + opcode_count > 0xc000)
			return false;

		for (std::size_t i{}; i < atlas_movie::layout::HANDLERS.size(); ++i) {
			const auto handler{ atlas_movie::layout::HANDLERS[i] };
			if (p_rom[file_offset(12, low) + i] != static_cast<byte>(handler)
				|| p_rom[file_offset(12, high) + i]
					!= static_cast<byte>(handler >> 8))
				return false;
		}
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

std::vector<byte> fe::AtlasMovieEngine::build_package(
	const AtlasMovieBundle& p_bundle) {
	const auto fmb{ AtlasMovieBundleCodec::compile(p_bundle) };
	if (fmb.size() > 0xffff)
		throw std::runtime_error("Atlas Movie Engine bundle exceeds AME1 limits");
	std::vector<byte> result{ 'A', 'M', 'E', '1', 1,
		static_cast<byte>(CORE_BYTES), static_cast<byte>(CORE_BYTES >> 8),
		static_cast<byte>(TAIL_BYTES), static_cast<byte>(TAIL_BYTES >> 8),
		static_cast<byte>(fmb.size()), static_cast<byte>(fmb.size() >> 8) };
	result.insert(result.end(), GENERATED_CORE.begin(), GENERATED_CORE.end());
	result.insert(result.end(), GENERATED_TAIL.begin(), GENERATED_TAIL.end());
	result.insert(result.end(), fmb.begin(), fmb.end());
	validate_package(result);
	return result;
}

void fe::AtlasMovieEngine::validate_package(const std::vector<byte>& p_package) {
	constexpr std::array<byte, 5> AME_MAGIC{ 'A', 'M', 'E', '1', 1 };
	if (p_package.size() < 11 || !std::equal(
		AME_MAGIC.begin(), AME_MAGIC.end(), p_package.begin()))
		throw std::runtime_error("Invalid Atlas Movie Engine AME1 package");

	const std::size_t core_size{ read_word(p_package, 5) };
	const std::size_t tail_size{ read_word(p_package, 7) };
	const std::size_t bundle_size{ read_word(p_package, 9) };
	if (core_size != CORE_BYTES || tail_size != TAIL_BYTES)
		throw std::runtime_error("Unsupported Atlas Movie Engine code version");
	if (11 + core_size + tail_size + bundle_size != p_package.size())
		throw std::runtime_error("Atlas Movie Engine package length is inconsistent");
	if (!std::equal(GENERATED_CORE.begin(), GENERATED_CORE.end(),
			p_package.begin() + 11)
		|| !std::equal(GENERATED_TAIL.begin(), GENERATED_TAIL.end(),
			p_package.begin() + static_cast<std::ptrdiff_t>(11 + core_size)))
		throw std::runtime_error(
			"Atlas Movie Engine executable bytes do not match supported AME1 version 1");
	const std::size_t bundle_offset{ 11 + core_size + tail_size };
	(void)AtlasMovieBundleCodec::parse(std::vector<byte>(
		p_package.begin() + static_cast<std::ptrdiff_t>(bundle_offset), p_package.end()));
}

std::size_t fe::AtlasMovieEngine::script_data_start(const std::vector<byte>& p_rom) {
	if (!is_installed(p_rom))
		throw std::runtime_error("Atlas Movie Engine is not installed");
	const auto low{ read_word(p_rom,
		file_offset(12, atlas_movie::layout::DISPATCH_LOW_REF)) };
	const auto high{ read_word(p_rom,
		file_offset(12, atlas_movie::layout::DISPATCH_HIGH_REF)) };
	const auto end_cpu{ static_cast<std::size_t>(high) + (high - low) };
	if (end_cpu > 0xc000)
		throw std::runtime_error("Installed Atlas Movie Engine overflows bank 12");
	return file_offset(12, static_cast<word>(end_cpu));
}

fe::AtlasMovieInstallResult fe::AtlasMovieEngine::install(
	std::vector<byte>& p_rom, const std::vector<byte>& p_package) {
	if (is_installed(p_rom))
		throw std::runtime_error("Atlas Movie Engine is already installed");
	if (p_rom.size() != HEADER_BYTES + 16 * PRG_BANK_BYTES)
		throw std::runtime_error("Atlas Movie Engine requires an unexpanded 256 KiB PRG ROM");
	validate_package(p_package);

	const std::size_t core_size{ read_word(p_package, 5) };
	const std::size_t tail_size{ read_word(p_package, 7) };
	const std::size_t bundle_size{ read_word(p_package, 9) };
	const std::size_t bundle_offset{ 11 + core_size + tail_size };

	if (fnv1a(p_rom, 12, CORE_CPU, 0x037b) != 0x500777943fdb2d92ULL
		|| fnv1a(p_rom, 15, 0xfc9c, 2) != 0x0a341107b6a00e89ULL
		|| fnv1a(p_rom, 12, 0x82ae, 3) != 0xc3736d17ce7e651aULL
		|| fnv1a(p_rom, 12, 0xaa83, 17) != 0x4b9cac1c2c83a089ULL
		|| fnv1a(p_rom, 12, 0xaa9f, 120) != 0x423b9391916d3b2bULL)
		throw std::runtime_error(
			"ROM is not compatible USA Rev 0, or its cinematic machinery was already modified");

	const auto dispatch_ref_hi{ file_offset(
		12, atlas_movie::layout::DISPATCH_HIGH_REF) };
	const auto dispatch_ref_lo{ file_offset(
		12, atlas_movie::layout::DISPATCH_LOW_REF) };
	if (read_word(p_rom, dispatch_ref_hi) != 0x8293
		|| read_word(p_rom, dispatch_ref_lo) != 0x827b)
		throw std::runtime_error(
			"Install Atlas Movie Engine before adding other extended iScript opcodes");

	const std::size_t dispatch_low_size{ static_cast<std::size_t>(BUNDLE_CPU) + bundle_size };
	const std::size_t dispatch_high_size{ dispatch_low_size + DISPATCH_ENTRIES };
	const std::size_t end_cpu_size{ dispatch_high_size + DISPATCH_ENTRIES };
	if (end_cpu_size > 0xc000)
		throw std::runtime_error("Atlas Movie Engine package overflows bank 12");
	if (!bytes_are(p_rom, 12, HANDLER_CPU, end_cpu_size - HANDLER_CPU, 0xff))
		throw std::runtime_error(
			"Atlas Movie Engine destination space is already used by another ROM patch");
	const word dispatch_low{ static_cast<word>(dispatch_low_size) };
	const word dispatch_high{ static_cast<word>(dispatch_high_size) };
	const word end_cpu{ static_cast<word>(end_cpu_size) };
	const std::vector<byte> fmb(
		p_package.begin() + static_cast<std::ptrdiff_t>(bundle_offset),
		p_package.end());
	const auto parsed_bundle{ AtlasMovieBundleCodec::parse(fmb) };
	atlas_movie::validate_movie_oam_budget(p_rom, parsed_bundle);
	atlas_movie::reject_movie_source_overlaps(p_rom, parsed_bundle, {
		{ file_offset(12, CORE_CPU), core_size, "Shared core" },
		{ file_offset(12, HANDLER_CPU), end_cpu_size - HANDLER_CPU,
			"Shared adapter, tail, FMB, and dispatch" },
		{ dispatch_ref_hi, 2, "Shared high dispatch reference" },
		{ dispatch_ref_lo, 2, "Shared low dispatch reference" },
		{ file_offset(15, 0xfc9c), 2, "Shared title hook" },
		{ file_offset(12, 0x82ae), 3, "Shared ending hook" },
	}, "Atlas Movie Engine Shared install");

	const byte* core{ p_package.data() + 11 };
	const byte* tail{ core + core_size };
	const byte* bundle_data{ tail + tail_size };
	write_bytes(p_rom, 12, CORE_CPU, core, core_size);
	constexpr std::array<byte, 6> ADAPTER{ 0x20, 0xa4, 0x87, 0x4c, 0x13, 0xa7 };
	write_bytes(p_rom, 12, HANDLER_CPU, ADAPTER.data(), ADAPTER.size());
	write_bytes(p_rom, 12, TAIL_CPU, tail, tail_size);
	write_bytes(p_rom, 12, BUNDLE_CPU, bundle_data, bundle_size);

	constexpr auto& handlers{ atlas_movie::layout::HANDLERS };
	std::array<byte, handlers.size()> low{}, high{};
	for (std::size_t i{ 0 }; i < handlers.size(); ++i) {
		low[i] = static_cast<byte>(handlers[i] & 0xff);
		high[i] = static_cast<byte>(handlers[i] >> 8);
	}
	write_bytes(p_rom, 12, dispatch_low, low.data(), low.size());
	write_bytes(p_rom, 12, dispatch_high, high.data(), high.size());
	write_word(p_rom, dispatch_ref_hi, dispatch_high);
	write_word(p_rom, dispatch_ref_lo, dispatch_low);
	constexpr std::array<byte, 2> TITLE_HOOK{ 0x07, 0xa7 };
	constexpr std::array<byte, 3> ENDING_HOOK{ 0x20, 0x0c, 0xa7 };
	write_bytes(p_rom, 15, 0xfc9c, TITLE_HOOK.data(), TITLE_HOOK.size());
	write_bytes(p_rom, 12, 0x82ae, ENDING_HOOK.data(), ENDING_HOOK.size());

	return {
		.bundle_bytes = bundle_size,
		.reserved_file_end = file_offset(12, end_cpu),
	};
}
