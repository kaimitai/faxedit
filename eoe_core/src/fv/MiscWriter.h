#ifndef FV_MISCWRITER_H
#define FV_MISCWRITER_H

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "fe/Config.h"
#include "fi/FaxString.h"

using byte = unsigned char;

namespace fv {

	enum class MiscType { Bit8, Binary8, Bit16, iString, String23, StringVar16, TitleString };
	enum class MiscCategory { TitleString, StatusString, ItemString, PasswordString, Rank, DropTable, Sprite, Weapon, Magic, Armor, WingBoots };
	enum class MiscField { Text, DropIndex, XP, HP, Gold, Bread, Damage, Glove, Defense, MagicDefense, Cost, Seconds };

	struct MiscMeta {
		fv::MiscCategory category;
		fv::MiscField field;
		std::size_t index;
	};

	struct MiscItem {
		int numeric_value;
		fi::FaxString string_value;
	};

	class MiscWriter {

		std::map<fv::MiscCategory, std::string> category_strings;
		std::map<fv::MiscField, std::string> field_strings;

		std::map<std::string, fv::MiscField> string_field;
		std::map<std::string, fv::MiscCategory> string_category;

		std::size_t title_screen_str_offset, title_screen_str_end_offset,
			rank_string_length, status_string_count, item_string_count,
			password_string_count, sprite_count, rank_count, weapon_count, magic_count,
			armor_count, wb_time_count;

		std::set<std::size_t> include_sprite_idx;
		std::map<byte, std::string> sprite_labels,
			title_screen_chars, iscript_chars, item_chars, mantra_chars;
		std::map<std::string, byte> title_screen_chars_rev, iscript_chars_rev, item_chars_rev, mantra_chars_rev;
		std::vector<byte> sprite_cats;

		std::map<std::pair<fv::MiscCategory, fv::MiscField>,
			std::pair<std::size_t, std::size_t>> index_ranges;
		std::map<std::pair<fv::MiscCategory, fv::MiscField>,
			std::map<std::size_t, MiscItem>> items;
		void add_item(fv::MiscCategory p_category, fv::MiscField p_field,
			std::size_t p_index, const fv::MiscItem item);
		void add_item(const std::vector<byte>& p_rom, const fe::Config& p_config,
			fv::MiscCategory p_category, fv::MiscField p_field,
			std::size_t p_index);

		MiscItem read_misc_item(const std::vector<byte>& p_rom, std::size_t p_offset, fv::MiscType type) const;
		std::size_t get_rom_offset(const fe::Config& p_config,
			fv::MiscCategory p_category, fv::MiscField p_field, std::size_t p_index) const;
		std::size_t get_data_size(fv::MiscType misctype, std::size_t p_count) const;

		fv::MiscType get_type(fv::MiscCategory p_category, fv::MiscField p_field) const;

		std::string get_category_string(fv::MiscCategory p_category, fv::MiscField p_field) const;
		std::string get_line_comment(fv::MiscCategory p_category, fv::MiscField p_field,
			std::size_t p_index, const fv::MiscItem& p_item) const;
		std::string to_misc_string(fv::MiscCategory p_category, fv::MiscField p_field,
			std::size_t p_index, const fv::MiscItem item) const;
		std::string to_value_string(fv::MiscType, const fv::MiscItem item) const;

		void patch_item(std::vector<byte>& p_rom, const fe::Config& p_config,
			fv::MiscCategory p_category, fv::MiscField p_field,
			std::size_t p_index, const fv::MiscItem item);
		int patch_title_strings(std::vector<byte>& p_rom);

		std::string get_magic_def_string(byte p_seed) const;
		byte get_magic_defense(byte p_seed, byte p_weapon_no) const;

		MiscMeta parse_txt_key(const std::string& p_key, std::size_t p_line_no) const;

		byte get_istring_padding(void) const;

		void extract_title_screen_strings(const std::vector<byte>& p_rom);

		fi::FaxString extract_fax_string(const std::vector<byte>& p_rom,
			std::size_t p_offset,
			const std::map<byte, std::string>& charmap,
			std::size_t max_length,
			bool p_length_encoded,
			bool p_pop_padding,
			byte p_padding_byte) const;

		std::vector<byte> fax_string_to_bytes(const fi::FaxString& p_str,
			const std::map<std::string, byte>& rev_charmap,
			std::size_t max_length,
			bool p_length_encoded,
			byte p_padding_byte
		) const;

	public:
		MiscWriter(const std::vector<byte>& p_rom, const fe::Config& p_config,
			bool p_incl_all_sprites = false);
		void load_rom(const std::vector<byte>& p_rom, const fe::Config& p_config);
		int patch_rom(std::vector<byte>& p_rom, const fe::Config& p_config);

		void load_txt_file(const std::string& p_txt_file);
		void write_txt_file(const std::string& p_filename) const;
	};

}

#endif
