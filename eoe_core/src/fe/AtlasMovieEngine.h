#ifndef FE_ATLASMOVIEENGINE_H
#define FE_ATLASMOVIEENGINE_H

#include <cstddef>
#include <vector>

using byte = unsigned char;

namespace fe {
	struct AtlasMovieBundle;

	struct AtlasMovieInstallResult {
		std::size_t bundle_bytes;
		std::size_t reserved_file_end;
	};

	class AtlasMovieEngine {
	public:
		static bool is_installed(const std::vector<byte>& p_rom);
		// build a complete AME1 package from the compiled-in engine and movie data
		static std::vector<byte> build_package(const AtlasMovieBundle& p_bundle);
		// validate engine bytes and FMB
		static void validate_package(const std::vector<byte>& p_package);
		// first free iScript file offset after AME
		static std::size_t script_data_start(const std::vector<byte>& p_rom);
		static AtlasMovieInstallResult install(
			std::vector<byte>& p_rom, const std::vector<byte>& p_package);
	};

}

#endif
