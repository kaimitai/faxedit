#ifndef FE_CLIPBOARD_MANAGER_H
#define FE_CLIPBOARD_MANAGER_H

#include <string>
#include <string_view>
#include <vector>

using byte = unsigned char;

namespace fe {

	enum class ClipBoardType { Tilemap, Palette };

	class ClipboardManager {
		static constexpr std::string_view HEADER_TILEMAP = "EOE-TILEMAP";
		static constexpr std::string_view HEADER_PALETTE = "EOE-PALETTE";

		std::string to_hex_string(const std::vector<byte>& p_bytes) const;
		std::string get_clipboard_text(void) const;

		void validate_header_type(const std::string& text, fe::ClipBoardType p_type) const;
		std::vector<std::string> tokenize(const std::string& text) const;
		byte parse_hex_byte(const std::string& token) const;

	public:
		ClipboardManager(void) = default;

		// tilemap
		void copy_tilemap(const std::vector<std::vector<byte>>& p_tilemap) const;
		std::vector<std::vector<byte>> paste_tilemap() const;

		// palettes
		void copy_palette(const std::vector<byte>& p_palette) const;
		std::vector<byte> paste_palette(void) const;

		// helpers
		static std::vector<std::string> split_lines(const std::string& text);
		static fe::ClipBoardType string_to_clp_type(const std::string& text);
	};

}

#endif
