#ifndef FE_ATLASMOVIERUNTIME_H
#define FE_ATLASMOVIERUNTIME_H

#include "Config.h"
#include "fi/Opcode.h"
#include <cstddef>
#include <cstdint>
#include <vector>

using byte = unsigned char;

namespace fe {

	enum class AtlasMovieRuntimeMode {
		Standalone,
		Shared
	};

	class AtlasMovieRuntime {
	public:
		static constexpr std::size_t STANDALONE_PLAYER_BYTES{ 1998 };
		static constexpr std::size_t SHARED_ADAPTER_BYTES{ 6 };

		// build the relocated player followed by FMB
		static std::vector<byte> build_standalone(const std::vector<byte>& p_fmb,
			std::uint16_t p_cpu_addr);

		// install Standalone and return the first free CPU address
		static std::uint16_t install_standalone(const Config& p_config,
			std::vector<byte>& p_rom, std::uint16_t p_cpu_addr);
		// reject ROM-owned assets hidden under the generated library allocation
		static void validate_standalone_sources(const std::vector<byte>& p_fmb,
			std::uint16_t p_cpu_begin, std::uint16_t p_cpu_end);
		static void validate_standalone_sources(const std::vector<byte>& p_fmb,
			const std::vector<byte>& p_rom, std::uint16_t p_cpu_begin,
			std::uint16_t p_cpu_end);

		// add AME's preinstalled $18 opcode and reject mixed modes
		static fi::ScriptOpcodeInfo resolve_opcode_info(fi::ScriptOpcodeInfo p_info,
			const std::vector<byte>& p_rom);
		static void validate_shared_install(const fi::ScriptOpcodeInfo& p_info);

		// export the opcode map, Standalone Impl, and FMB
		static std::string standalone_config_override(const fi::ScriptOpcodeInfo& p_info,
			const std::vector<byte>& p_fmb);
	};

}

#endif
