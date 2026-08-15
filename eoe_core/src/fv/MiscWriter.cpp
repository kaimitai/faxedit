#include "MiscWriter.h"
#include "common/klib/Kstring.h"
#include "fe/fe_app_constants.h"
#include "fv_constants.h"
#include <format>

fv::MiscWriter::MiscWriter(const std::vector<byte>& p_rom, const fe::Config& p_config,
	bool p_incl_all_sprites) :
	title_screen_str_offset{ p_config.constant(c::ID_TITLE_STRING_OFFSET) },
	title_screen_str_end_offset{ p_config.constant(c::ID_TITLE_STRING_END_OFFSET) },
	rank_string_length{ p_config.constant(c::ID_RANK_STRING_LENGTH) },
	status_string_count{ p_config.constant(c::ID_STATUS_STRING_COUNT) },
	item_string_count{ p_config.constant(c::ID_ITEM_STRING_COUNT) },
	password_string_count{ p_config.constant(c::ID_PASSWORD_STRING_COUNT) },
	sprite_count{ p_config.constant(c::ID_SPRITE_COUNT) },
	rank_count{ p_config.constant(c::ID_RANK_COUNT) },
	weapon_count{ p_config.constant(c::ID_WEAPON_COUNT) },
	magic_count{ p_config.constant(c::ID_MAGIC_COUNT) },
	armor_count{ p_config.constant(c::ID_ARMOR_COUNT) },
	wb_time_count{ p_config.constant(c::ID_WING_BOOTS_TIME_COUNT) },
	sprite_labels{ p_config.bmap(c::ID_SPRITE_LABELS) },
	iscript_chars{ p_config.bmap(c::ID_ISCRIPT_CHARS) }
{

	// generate character maps for the different string types
	// iscript_chars - comes from the config xml as they are also used in iScripts
	// item_chars - uppercase letters, numbers and some special chars
	// pw_chars - uppercase, lowercase and some special chars

	for (char c : c::CHR_PALETTE_ITEMS)
		item_chars.insert(std::make_pair(static_cast<byte>(c), std::string(1, c)));
	for (char c : c::CHR_PALETTE_MANTRA)
		mantra_chars.insert(std::make_pair(static_cast<byte>(c), std::string(1, c)));

	// add <q> as code for double quote in the mantra chr palette
	mantra_chars.insert(std::make_pair(static_cast<byte>('"'), "<q>"));

	// generate character map for the title screen which is not ascii-compatible
	byte b{ 0xd6 };
	for (char c{ '0' }; c <= '9'; ++c)
		title_screen_chars.insert(std::make_pair(b++, std::string(1, c)));
	b = 0xe0;
	for (char c{ 'A' }; c <= 'Z'; ++c)
		title_screen_chars.insert(std::make_pair(b++, std::string(1, c)));
	title_screen_chars.insert(std::make_pair(0x20, " "));
	// add the copyright symbol since we're being thorough ;)
	title_screen_chars.insert(std::make_pair(0xfa, "<copyright>"));

	// generate reverse maps for patching purposes
	title_screen_chars_rev = klib::str::invert_map(title_screen_chars);
	iscript_chars_rev = klib::str::invert_map(iscript_chars);
	item_chars_rev = klib::str::invert_map(item_chars);
	mantra_chars_rev = klib::str::invert_map(mantra_chars);

	// extract sprite categories
	for (std::size_t i{ 0 }; i < sprite_count; ++i)
		sprite_cats.push_back(p_rom.at(p_config.constant(c::ID_SPRITE_TYPE_OFFSET) + i));

	// populate set of sprites we care about - if p_incl_all_sprites is set: all sprites, else only enemies/bosses
	for (std::size_t i{ 0 }; i < sprite_count; ++i)
		if (p_incl_all_sprites || c::RELEVANT_SPRITE_CATS.contains(sprite_cats[i]))
			include_sprite_idx.insert(i);

	category_strings.insert(std::make_pair(fv::MiscCategory::TitleString, c::CAT_TITLE_STRING));
	category_strings.insert(std::make_pair(fv::MiscCategory::StatusString, c::CAT_STATUS_STRING));
	category_strings.insert(std::make_pair(fv::MiscCategory::ItemString, c::CAT_ITEM_STRING));
	category_strings.insert(std::make_pair(fv::MiscCategory::PasswordString, c::CAT_PASSWORD_STRING));
	category_strings.insert(std::make_pair(fv::MiscCategory::Rank, c::CAT_RANK));
	category_strings.insert(std::make_pair(fv::MiscCategory::Sprite, c::CAT_SPRITE));
	category_strings.insert(std::make_pair(fv::MiscCategory::Weapon, c::CAT_WEAPON));
	category_strings.insert(std::make_pair(fv::MiscCategory::Magic, c::CAT_MAGIC));
	category_strings.insert(std::make_pair(fv::MiscCategory::Armor, c::CAT_ARMOR));
	category_strings.insert(std::make_pair(fv::MiscCategory::DropTable, c::CAT_DROP_TABLE));
	category_strings.insert(std::make_pair(fv::MiscCategory::WingBoots, c::CAT_WINGBOOTS));

	field_strings.insert(std::make_pair(fv::MiscField::Text, c::FIELD_TEXT));
	field_strings.insert(std::make_pair(fv::MiscField::XP, c::FIELD_XP));
	field_strings.insert(std::make_pair(fv::MiscField::HP, c::FIELD_HP));
	field_strings.insert(std::make_pair(fv::MiscField::Gold, c::FIELD_GOLD));
	field_strings.insert(std::make_pair(fv::MiscField::Bread, c::FIELD_BREAD));
	field_strings.insert(std::make_pair(fv::MiscField::DropIndex, c::FIELD_DROP_INDEX));
	field_strings.insert(std::make_pair(fv::MiscField::Damage, c::FIELD_DAMAGE));
	field_strings.insert(std::make_pair(fv::MiscField::Glove, c::FIELD_GLOVE));
	field_strings.insert(std::make_pair(fv::MiscField::MagicDefense, c::FIELD_MAGIC_DEFENSE));
	field_strings.insert(std::make_pair(fv::MiscField::Defense, c::FIELD_DEFENSE));
	field_strings.insert(std::make_pair(fv::MiscField::Cost, c::FIELD_COST));
	field_strings.insert(std::make_pair(fv::MiscField::Seconds, c::FIELD_SECONDS));

	index_ranges = {
	{{fv::MiscCategory::StatusString, fv::MiscField::Text}, {0, status_string_count}},
	{{fv::MiscCategory::ItemString, fv::MiscField::Text}, {0, item_string_count}},
	{{fv::MiscCategory::PasswordString, fv::MiscField::Text}, {0, password_string_count}},
	{{fv::MiscCategory::Rank, fv::MiscField::Text}, {0, rank_count}},
	{{fv::MiscCategory::Rank, fv::MiscField::XP}, {1, rank_count}},
	{{fv::MiscCategory::Rank, fv::MiscField::Gold}, {1, rank_count}},
	{{fv::MiscCategory::DropTable, fv::MiscField::Gold}, {0, c::DROP_TABLE_CUTOFF}},
	{{fv::MiscCategory::DropTable, fv::MiscField::Bread}, {c::DROP_TABLE_CUTOFF, p_config.constant(c::ID_SPRITE_DROP_TABLE_COUNT)}},
	{{fv::MiscCategory::Sprite, fv::MiscField::DropIndex}, {0, p_config.constant(c::ID_SPRITE_DROP_IDX_COUNT)}},
	{{fv::MiscCategory::Sprite, fv::MiscField::HP}, {0, p_config.constant(c::ID_SPRITE_HP_COUNT)}},
	{{fv::MiscCategory::Sprite, fv::MiscField::XP}, {0, p_config.constant(c::ID_SPRITE_XP_COUNT)}},
	{{fv::MiscCategory::Sprite, fv::MiscField::MagicDefense}, {0, p_config.constant(c::ID_SPRITE_DEFENSE_COUNT)}},
	{{fv::MiscCategory::Sprite, fv::MiscField::Damage}, {0, p_config.constant(c::ID_SPRITE_DAMAGE_COUNT)}},
	{{fv::MiscCategory::Magic, fv::MiscField::Damage}, {0, magic_count}},
	{{fv::MiscCategory::Magic, fv::MiscField::Cost}, {0, magic_count}},
	{{fv::MiscCategory::Weapon, fv::MiscField::Damage}, {0, weapon_count}},
	{{fv::MiscCategory::Weapon, fv::MiscField::Glove}, {0, weapon_count}},
	{{fv::MiscCategory::Armor, fv::MiscField::Defense}, {0, armor_count}},
	{{fv::MiscCategory::WingBoots, fv::MiscField::Seconds}, {0, wb_time_count}}
	};
}

void fv::MiscWriter::load_rom(const std::vector<byte>& p_rom, const fe::Config& p_config) {

	extract_title_screen_strings(p_rom);

	for (std::size_t i{ 0 }; i < status_string_count; ++i)
		add_item(p_rom, p_config, fv::MiscCategory::StatusString, fv::MiscField::Text, i);
	for (std::size_t i{ 0 }; i < item_string_count; ++i)
		add_item(p_rom, p_config, fv::MiscCategory::ItemString, fv::MiscField::Text, i);

	if (p_config.has_constant(c::ID_PASSWORD_STRING_OFFSET))
		for (std::size_t i{ 0 }; i < password_string_count; ++i)
			add_item(p_rom, p_config, fv::MiscCategory::PasswordString, fv::MiscField::Text, i);

	for (std::size_t i{ 0 }; i < rank_count; ++i) {
		if (p_config.has_constant(c::ID_RANK_STRING_OFFSET))
			add_item(p_rom, p_config, fv::MiscCategory::Rank, fv::MiscField::Text, i);
		if (i > 0) {
			add_item(p_rom, p_config, fv::MiscCategory::Rank, fv::MiscField::XP, i);
			add_item(p_rom, p_config, fv::MiscCategory::Rank, fv::MiscField::Gold, i);
		}
	}

	for (std::size_t i{ 0 }; i < p_config.constant(c::ID_SPRITE_DROP_TABLE_COUNT); ++i)
		add_item(p_rom, p_config, fv::MiscCategory::DropTable,
			i >= c::DROP_TABLE_CUTOFF ? fv::MiscField::Bread : fv::MiscField::Gold,
			i);

	for (std::size_t i : include_sprite_idx) {
		if (i < p_config.constant(c::ID_SPRITE_HP_COUNT))
			add_item(p_rom, p_config, fv::MiscCategory::Sprite, fv::MiscField::HP, i);
		if (i < p_config.constant(c::ID_SPRITE_XP_COUNT))
			add_item(p_rom, p_config, fv::MiscCategory::Sprite, fv::MiscField::XP, i);
		if (i < p_config.constant(c::ID_SPRITE_DROP_IDX_COUNT))
			add_item(p_rom, p_config, fv::MiscCategory::Sprite, fv::MiscField::DropIndex, i);
		if (i < p_config.constant(c::ID_SPRITE_DAMAGE_COUNT))
			add_item(p_rom, p_config, fv::MiscCategory::Sprite, fv::MiscField::Damage, i);
		if (i < p_config.constant(c::ID_SPRITE_DEFENSE_COUNT))
			add_item(p_rom, p_config, fv::MiscCategory::Sprite, fv::MiscField::MagicDefense, i);
	}

	for (std::size_t i{ 0 }; i < magic_count; ++i) {
		add_item(p_rom, p_config, fv::MiscCategory::Magic, fv::MiscField::Damage, i);
		add_item(p_rom, p_config, fv::MiscCategory::Magic, fv::MiscField::Cost, i);
	}

	for (std::size_t i{ 0 }; i < weapon_count; ++i) {
		add_item(p_rom, p_config, fv::MiscCategory::Weapon, fv::MiscField::Damage, i);
		add_item(p_rom, p_config, fv::MiscCategory::Weapon, fv::MiscField::Glove, i);
	}

	for (std::size_t i{ 0 }; i < armor_count; ++i)
		add_item(p_rom, p_config, fv::MiscCategory::Armor, fv::MiscField::Defense, i);

	for (std::size_t i{ 0 }; i < wb_time_count; ++i)
		add_item(p_rom, p_config, fv::MiscCategory::WingBoots, fv::MiscField::Seconds, i);
}

int fv::MiscWriter::patch_rom(std::vector<byte>& p_rom, const fe::Config& p_config) {
	int result{ 0 };

	for (const auto& kv : items)
		if (kv.first.first != fv::MiscCategory::TitleString)
			for (const auto& itkv : kv.second) {
				patch_item(p_rom, p_config, kv.first.first, kv.first.second,
					itkv.first, itkv.second);
				++result;
			}

	try {
		result += patch_title_strings(p_rom);
	}
	catch (const std::runtime_error& ex) {
		throw std::runtime_error(std::format("Failed to convert title screen strings: {}", ex.what()));
	}

	return result;
}

// special case - encode all title screen strings at once - they are not really individually static
int fv::MiscWriter::patch_title_strings(std::vector<byte>& p_rom) {
	int strcount{ 0 };
	std::vector<byte> bytes;

	std::map<std::size_t, fi::FaxString> faxstrings;
	auto iter{ items.find(std::make_pair(fv::MiscCategory::TitleString, fv::MiscField::Text)) };

	// return if no items at all
	if (iter == end(items))
		return 0;

	for (const auto& itemkv : iter->second)
		faxstrings.insert(std::make_pair(itemkv.first, itemkv.second.string_value));

	// already sorted, but ensure the map is not sparse
	for (std::size_t i{ 0 }; i < faxstrings.size(); ++i) {
		const auto jter{ faxstrings.find(i) };
		if (jter == end(faxstrings))
			throw std::runtime_error(std::format("Missing title string with index {}", i));
		else {
			auto strbytes{ jter->second.to_bytes(title_screen_chars_rev) };
			// change usual delimiter from 0xff to 0x00
			strbytes.back() = 0x00;
			bytes.insert(end(bytes), begin(strbytes), end(strbytes));
		}
	}

	std::size_t max_byte_len{ title_screen_str_end_offset - title_screen_str_offset };
	while (bytes.size() < max_byte_len)
		bytes.push_back(0x00);

	if (bytes.size() > max_byte_len)
		throw std::runtime_error(std::format("Title screen string byte size is {}, but can not exceed {} bytes",
			bytes.size(), max_byte_len));
	else {
		for (std::size_t i{ 0 }; i < bytes.size(); ++i)
			p_rom.at(i + title_screen_str_offset) = bytes[i];
	}

	return static_cast<int>(faxstrings.size());
}

void fv::MiscWriter::patch_item(std::vector<byte>& p_rom, const fe::Config& p_config,
	fv::MiscCategory p_category, fv::MiscField p_field,
	std::size_t p_index, const fv::MiscItem item) {

	auto idx_iter{ index_ranges.find(std::make_pair(p_category, p_field)) };
	if (idx_iter == end(index_ranges)) {
		throw std::runtime_error("Can not validate index");
	}

	auto idx_range{ idx_iter->second };
	if (p_index < idx_range.first || p_index >= idx_range.second)
		throw std::runtime_error(
			std::format("Invalid index {} - valid range is {}-{}", p_index, idx_range.first, idx_range.second - 1)
		);

	std::size_t offset{ get_rom_offset(p_config, p_category, p_field, p_index) };
	auto itemtype{ get_type(p_category, p_field) };

	if (itemtype == fv::MiscType::Bit8 || itemtype == fv::MiscType::Binary8)
		p_rom.at(offset) = static_cast<byte>(item.numeric_value);
	else if (itemtype == fv::MiscType::Bit16) {
		p_rom.at(offset) = static_cast<byte>(item.numeric_value % 256);
		p_rom.at(offset + 1) = static_cast<byte>(item.numeric_value / 256);
	}
	else {
		// we have a string
		std::vector<byte> strbytes;
		try {
			if (itemtype == fv::MiscType::iString)
				strbytes = fax_string_to_bytes(item.string_value, iscript_chars_rev, rank_string_length,
					false, get_istring_padding());
			else if (itemtype == fv::MiscType::StringVar16)
				strbytes = fax_string_to_bytes(item.string_value, item_chars_rev, 16,
					true, c::STR_PADDING_BYTE);
			else if (itemtype == fv::MiscType::String23)
				strbytes = fax_string_to_bytes(item.string_value, mantra_chars_rev, 23,
					false, c::STR_PADDING_BYTE);
		}
		catch (const std::runtime_error& ex) {
			throw std::runtime_error(std::format("Failed to parse string '{}': {}",
				item.string_value.get_string(), ex.what()));
		}

		for (std::size_t i{ 0 }; i < strbytes.size(); ++i)
			p_rom.at(offset + i) = strbytes[i];
	}
}

void fv::MiscWriter::add_item(const std::vector<byte>& p_rom, const fe::Config& p_config,
	fv::MiscCategory p_category, fv::MiscField p_field,
	std::size_t p_index) {
	add_item(p_category, p_field, p_index,
		read_misc_item(p_rom, get_rom_offset(p_config, p_category, p_field, p_index),
			get_type(p_category, p_field)
		));
}

void fv::MiscWriter::add_item(fv::MiscCategory p_category, fv::MiscField p_field,
	std::size_t p_index, const fv::MiscItem item) {
	items[std::make_pair(p_category, p_field)].insert(std::make_pair(p_index, item));
}

void fv::MiscWriter::load_txt(const std::vector<std::string>& p_txt) {

	// reverse maps for lookup
	string_field.clear();
	string_category.clear();

	for (const auto& kv : field_strings)
		string_field.insert(std::make_pair(klib::str::to_lower(kv.second), kv.first));
	for (const auto& kv : category_strings)
		string_category.insert(std::make_pair(klib::str::to_lower(kv.second), kv.first));

	auto txts{ p_txt };

	for (std::size_t i{ 0 }; i < txts.size(); ++i) {
		std::string line{ klib::str::trim(klib::str::strip_comment(txts[i])) };
		if (!line.empty()) {
			auto tokens{ klib::str::split_whitespace(line) };
			if (tokens.size() != 2)
				throw std::runtime_error(std::format("Invalid token count on line {} - expected 2. Check whitespace", i + 1));

			fv::MiscMeta meta{ parse_txt_key(tokens[0], i + 1) };
			auto misctype{ get_type(meta.category, meta.field) };

			int number{ 0 };
			fi::FaxString itemstring;

			if (misctype == fv::MiscType::Bit8 || misctype == fv::MiscType::Bit16 || misctype == fv::MiscType::Binary8) {
				try {
					number = klib::str::parse_numeric(tokens[1]);
				}
				catch (const std::runtime_error& ex) {
					throw std::runtime_error(std::format("Invalud numeric argument on line {} - {}", i, ex.what()));
				}
			}
			else {
				if (tokens[1].size() < 2 || tokens[1][0] != '\"' || tokens[1].back() != '\"')
					throw std::runtime_error(std::format("Invalid string argument '{}' on line {}", tokens[1], i + 1));
				std::string faxstr;
				for (std::size_t cc{ 1 }; cc < tokens[1].size() - 1; ++cc)
					faxstr.push_back(tokens[1][cc]);
				itemstring = fi::FaxString(faxstr);
			}

			add_item(meta.category, meta.field, meta.index, fv::MiscItem(number, itemstring));
		}
	}
}

fv::MiscMeta fv::MiscWriter::parse_txt_key(const std::string& p_key, std::size_t p_line_no) const {
	std::string cat, fld;
	std::size_t idx{ 0 };

	size_t dot = p_key.find('.');
	if (dot == std::string::npos)
		throw std::runtime_error(std::format("Invalid key {} on line {}", p_key, p_line_no));

	std::size_t numStart{ 0 };

	while (numStart < dot && !isdigit(p_key[numStart]))
		numStart++;
	std::size_t number{ 0 };

	std::string category{ p_key.substr(0, numStart) };
	try {
		number = static_cast<std::size_t>(klib::str::parse_numeric(p_key.substr(numStart, dot - numStart)));
	}
	catch (const std::runtime_error&) {
		throw std::runtime_error(std::format("Invalid index number on line {}", p_line_no));
	}

	std::string field{ p_key.substr(dot + 1) };

	if (!string_category.contains(klib::str::to_lower(category)))
		throw std::runtime_error(std::format("Invalid category {} on line {}", category, p_line_no));
	if (!string_field.contains(klib::str::to_lower(field)))
		throw std::runtime_error(std::format("Invalid category {} on line {}", field, p_line_no));

	return fv::MiscMeta(
		string_category.at(klib::str::to_lower(category)),
		string_field.at(klib::str::to_lower(field)),
		number
	);
}

// the jp version uses 8 chars per rank, the others use 16
// the padding is also different, so we use this length to determine the padding
byte fv::MiscWriter::get_istring_padding(void) const {
	return rank_string_length == 8 ? 0xff : c::STR_PADDING_BYTE;
}

std::string fv::MiscWriter::get_category_string(fv::MiscCategory p_category, fv::MiscField p_field) const {
	std::string result;

	if (p_category == fv::MiscCategory::Sprite) {
		if (p_field == fv::MiscField::XP)
			result = "Experience points received for killing sprites (0-255)";
		else if (p_field == fv::MiscField::DropIndex)
			result = "Index into the drop table - indirectly determines what a sprite drops when killed (index values are 0-63, 255 means no drop)";
		else if (p_field == fv::MiscField::Damage)
			result = "Damage taken from sprites (0-255)";
		else if (p_field == fv::MiscField::HP)
			result = "Sprite health points (0-255)";
		else if (p_field == fv::MiscField::MagicDefense)
			result = "Sprite magic damage reduction - 2 bits per magic type except Deluge";
	}
	else if (p_category == fv::MiscCategory::Weapon && p_field == fv::MiscField::Damage) {
		result = "Weapon damages; dagger, long sword, giant blade and dragon slayer";
	}
	else if (p_category == fv::MiscCategory::Weapon && p_field == fv::MiscField::Glove) {
		result = "Additional damage when using glove for; dagger, long sword, giant blade and dragon slayer";
	}
	else if (p_category == fv::MiscCategory::Magic && p_field == fv::MiscField::Damage) {
		result = "Magic damages; deluge, thunder, fire, death, tilte";
	}
	else if (p_category == fv::MiscCategory::Magic && p_field == fv::MiscField::Cost) {
		result = "Magic mana cost; deluge, thunder, fire, death, tilte";
	}
	else if (p_category == fv::MiscCategory::Armor && p_field == fv::MiscField::Defense) {
		result = "Defense multiplier per armor type; leather armor (probably ignored), studded mail, full plate and battle suit";
	}
	else if (p_category == fv::MiscCategory::TitleString)
		result = std::format("Title screen strings, variable size ({} chars total), but keep each length as-is (A-Z,0-9,space,<copyright>)",
			title_screen_str_end_offset - title_screen_str_offset);
	else if (p_category == fv::MiscCategory::StatusString)
		result = "Player status strings - max 15 characters (A-Z,0-9,space,colon)";
	else if (p_category == fv::MiscCategory::ItemString)
		result = "Item strings - max 15 characters (A-Z, 0-9, space, colon)";
	else if (p_category == fv::MiscCategory::PasswordString)
		result = "Password screen strings - max 23 characters (A-Z,a-z,0-9,space,!_-.,?,<q> for double quotes)";
	else if (p_category == fv::MiscCategory::Rank && p_field == fv::MiscField::Text)
		result = std::format("Rank name strings - max {} characters (follows iScript string rules, and is used in iScripts strings as <title>)", rank_string_length);
	else if (p_category == fv::MiscCategory::Rank && p_field == fv::MiscField::XP)
		result = "Rank XP requirements (0-65,535)";
	else if (p_category == fv::MiscCategory::Rank && p_field == fv::MiscField::Gold)
		result = "Starting gold per rank (0-65,535)";
	else if (p_category == fv::MiscCategory::DropTable && p_field == fv::MiscField::Gold)
		result = "Enemy drop table; the first 48 entries are coin values (0-255)";
	else if (p_category == fv::MiscCategory::DropTable && p_field == fv::MiscField::Bread)
		result = "Enemy drop table; the last 16 entries are bread hp values (0-255)";
	else if (p_category == fv::MiscCategory::WingBoots && p_field == fv::MiscField::Seconds)
		result = "Number of seconds Wing Boots last (changes every 4 ranks) - Higher values than 99 may not work";
	else
		result = "Unlabeled category";

	return result;
}

std::string fv::MiscWriter::get_line_comment(fv::MiscCategory p_category, fv::MiscField p_field,
	std::size_t p_index, const fv::MiscItem& p_item) const {
	std::string result;

	if (p_category == fv::MiscCategory::Sprite && sprite_labels.contains(static_cast<byte>(p_index)))
		result = sprite_labels.at(static_cast<byte>(p_index));
	else if (p_category == fv::MiscCategory::TitleString) {
		const auto bytes{ p_item.string_value.to_bytes(title_screen_chars_rev) };
		result = std::format("{} characters", bytes.size() - 1);
	}

	if (p_field == fv::MiscField::MagicDefense) {
		result += std::format(" ({})", get_magic_def_string(static_cast<byte>(p_item.numeric_value)));
	}

	return result;
}

std::string fv::MiscWriter::generate_txt(void) const {
	std::string txt{
		std::format(" ; Faxanadu miscellaneous data file extracted by {} {}\n ; {}\n",
			fe::c::APP_NAME, fe::c::APP_VERSION, fe::c::APP_URL)
	};

	for (const auto& kv : items) {
		txt += std::format("\n ; {}\n", get_category_string(kv.first.first, kv.first.second));
		for (const auto& itm : kv.second) {
			txt += std::format("{}",
				to_misc_string(kv.first.first, kv.first.second, itm.first, itm.second)
			);

			std::string line_comment{ get_line_comment(kv.first.first, kv.first.second, itm.first, itm.second) };
			if (!line_comment.empty())
				txt += std::format(" \t; {}", line_comment);

			txt.push_back('\n');
		}
	}

	return txt;
}

std::size_t fv::MiscWriter::get_data_size(fv::MiscType misctype, std::size_t p_count) const {
	std::size_t base_byte_size{ 1 };

	if (misctype == MiscType::Bit16)
		base_byte_size = 2;
	else if (misctype == MiscType::iString)
		base_byte_size = rank_string_length;
	else if (misctype == MiscType::StringVar16)
		base_byte_size = 16;
	else if (misctype == MiscType::String23)
		base_byte_size = 23;

	return base_byte_size * p_count;
}

fv::MiscItem fv::MiscWriter::read_misc_item(const std::vector<byte>& p_rom, std::size_t p_offset,
	fv::MiscType type) const {
	int numeric_value{ 0 };
	fi::FaxString string_value;

	if (type == fv::MiscType::StringVar16) {
		string_value = extract_fax_string(p_rom, p_offset, item_chars, 16, true, false, c::STR_PADDING_BYTE);
	}
	else if (type == fv::MiscType::iString) {
		string_value = extract_fax_string(p_rom, p_offset, iscript_chars, rank_string_length, false, true, get_istring_padding());
	}
	else if (type == fv::MiscType::String23) {
		string_value = extract_fax_string(p_rom, p_offset, mantra_chars, 23, false, false, get_istring_padding());
	}
	else if (type == fv::MiscType::Bit8 || type == fv::MiscType::Binary8)
		numeric_value = static_cast<int>(p_rom.at(p_offset));
	else if (type == fv::MiscType::Bit16)
		numeric_value = static_cast<int>(p_rom.at(p_offset)) +
		256 * static_cast<int>(p_rom.at(p_offset + 1));

	return fv::MiscItem(numeric_value, string_value);
}

std::size_t fv::MiscWriter::get_rom_offset(const fe::Config& p_config,
	fv::MiscCategory p_category, fv::MiscField p_field, std::size_t p_index) const {
	if (p_category == fv::MiscCategory::StatusString)
		return p_config.constant(c::ID_STATUS_STRING_OFFSET) + get_data_size(fv::MiscType::StringVar16, p_index);
	else if (p_category == fv::MiscCategory::ItemString)
		return p_config.constant(c::ID_ITEM_STRING_OFFSET) + get_data_size(fv::MiscType::StringVar16, p_index);
	else if (p_category == fv::MiscCategory::PasswordString)
		return p_config.constant(c::ID_PASSWORD_STRING_OFFSET) + get_data_size(fv::MiscType::String23, p_index);
	else if (p_category == fv::MiscCategory::Rank) {
		if (p_field == fv::MiscField::Text)
			return p_config.constant(c::ID_RANK_STRING_OFFSET) + get_data_size(fv::MiscType::iString, p_index);

		std::size_t result{ p_config.constant(c::ID_RANK_DATA_OFFSET) };

		if (p_field == fv::MiscField::XP) {
			result += get_data_size(fv::MiscType::Bit16, p_index - 1);		// rank 0 has no xp req
		}
		else if (p_field == fv::MiscField::Gold) {
			result += get_data_size(fv::MiscType::Bit16, rank_count - 1);	// rank 0 has no xp req
			result += get_data_size(fv::MiscType::Bit16, p_index - 1);		// rank 0 has no starting gold
		}
		return result;
	}
	else if (p_category == fv::MiscCategory::Sprite) {
		if (p_field == fv::MiscField::HP)
			return p_config.constant(c::ID_SPRITE_HP_OFFSET) + p_index;
		else if (p_field == fv::MiscField::XP)
			return p_config.constant(c::ID_SPRITE_XP_OFFSET) + p_index;
		else if (p_field == fv::MiscField::DropIndex)
			return p_config.constant(c::ID_SPRITE_DROP_IDX_OFFSET) + p_index;
		else if (p_field == fv::MiscField::Damage)
			return p_config.constant(c::ID_SPRITE_DAMAGE_OFFSET) + p_index;
		else if (p_field == fv::MiscField::MagicDefense)
			return p_config.constant(c::ID_SPRITE_DEFENSE_OFFSET) + p_index;
		else
			throw std::runtime_error("Invalid sprite field");
	}
	else if (p_category == fv::MiscCategory::Magic) {
		if (p_field == fv::MiscField::Damage)
			return p_config.constant(c::ID_MAGIC_DAMAGE_OFFSET) + p_index;
		else
			return p_config.constant(c::ID_MAGIC_COST_OFFSET) + p_index;
	}
	else if (p_category == fv::MiscCategory::Weapon) {
		if (p_field == fv::MiscField::Damage)
			return p_config.constant(c::ID_WEAPON_DAMAGE_OFFSET) + p_index;
		else if (p_field == fv::MiscField::Glove)
			return p_config.constant(c::ID_WEAPON_GLOVE_DAMAGE_OFFSET) + p_index;
	}
	else if (p_category == fv::MiscCategory::Armor) {
		return p_config.constant(c::ID_ARMOR_DEFENSE_OFFSET) + p_index;
	}
	else if (p_category == fv::MiscCategory::DropTable) {
		return p_config.constant(c::ID_SPRITE_DROP_TABLE_OFFSET) + p_index;
	}
	else if (p_category == fv::MiscCategory::WingBoots) {
		return p_config.constant(c::ID_WING_BOOTS_TIME_OFFSET) + p_index;
	}

	throw std::runtime_error(
		std::format("Could not determine ROM offset")
	);
}

fv::MiscType fv::MiscWriter::get_type(fv::MiscCategory p_category, fv::MiscField p_field) const {
	if (p_category == fv::MiscCategory::TitleString)
		return fv::MiscType::TitleString;
	else if (p_category == fv::MiscCategory::StatusString || p_category == fv::MiscCategory::ItemString)
		return fv::MiscType::StringVar16;
	else if (p_category == fv::MiscCategory::PasswordString)
		return fv::MiscType::String23;
	else if (p_category == fv::MiscCategory::Rank) {
		if (p_field == fv::MiscField::Text)
			return MiscType::iString;
		else if (p_field == fv::MiscField::Gold)
			return MiscType::Bit16;
		else if (p_field == fv::MiscField::XP)
			return MiscType::Bit16;
	}
	else if (p_field == fv::MiscField::MagicDefense)
		return MiscType::Binary8;
	else
		return MiscType::Bit8;

	throw std::runtime_error("Could not deduce data type from category and field");
}

std::string fv::MiscWriter::to_value_string(fv::MiscType p_type, const fv::MiscItem item) const {
	if (p_type == MiscType::Bit8)
		return std::to_string(static_cast<byte>(item.numeric_value));
	else if (p_type == MiscType::Binary8)
		return std::format("%{}", klib::str::to_binary(static_cast<byte>(item.numeric_value)));
	else if (p_type == MiscType::Bit16)
		return std::to_string(item.numeric_value);
	else if (p_type == MiscType::TitleString ||
		p_type == MiscType::StringVar16 ||
		p_type == MiscType::String23 ||
		p_type == MiscType::iString) {
		return std::format("\"{}\"", item.string_value.get_string());
	}

	throw std::runtime_error(
		std::format("Could not turn misc data type into string")
	);
}

// special case; variable length strings
void fv::MiscWriter::extract_title_screen_strings(const std::vector<byte>& p_rom) {
	auto cursor{ begin(p_rom) + title_screen_str_offset };
	auto strings_end{ begin(p_rom) + title_screen_str_end_offset };
	std::vector<fi::FaxString> faxstrings;

	while (true) {
		auto iter{ std::find(cursor, strings_end, 0x00) };
		if (iter >= strings_end)
			break;
		else {
			std::size_t stringlength{ static_cast<std::size_t>(iter - cursor) };
			faxstrings.push_back(extract_fax_string(p_rom, cursor - begin(p_rom), title_screen_chars, stringlength,
				false, false, 0x00));
			cursor = iter + 1;
		}
	}

	// pop empty strings at end - is possible nothing but padding
	while (!faxstrings.empty() && faxstrings.back().get_string().empty())
		faxstrings.pop_back();

	for (std::size_t i{ 0 }; i < faxstrings.size(); ++i)
		add_item(fv::MiscCategory::TitleString, fv::MiscField::Text, i, fv::MiscItem(0, faxstrings[i]));
}

fi::FaxString fv::MiscWriter::extract_fax_string(const std::vector<byte>& p_rom,
	std::size_t p_offset,
	const std::map<byte, std::string>& charmap,
	std::size_t max_length,
	bool p_length_encoded,
	bool p_pop_padding,
	byte p_padding_byte) const {

	std::size_t strlen{ max_length };
	std::size_t cursor{ 0 };

	if (p_length_encoded) {
		strlen = static_cast<std::size_t>(p_rom.at(p_offset));
		++cursor;
	}

	std::vector<byte> bytestr;
	for (; cursor <= strlen && cursor < max_length; ++cursor)
		bytestr.push_back(p_rom.at(p_offset + cursor));

	if (p_pop_padding) {
		while (bytestr.back() == p_padding_byte)
			bytestr.pop_back();
	}

	std::string result;

	for (byte b : bytestr) {
		if (charmap.contains(b))
			result += charmap.at(b);
		else
			result += std::format("<${:02x}>", b);
	}

	return fi::FaxString(result);
}

std::vector<byte> fv::MiscWriter::fax_string_to_bytes(const fi::FaxString& p_str,
	const std::map<std::string, byte>& rev_charmap,
	std::size_t max_length,
	bool p_length_encoded,
	byte p_padding_byte
) const {
	std::vector<byte> result;

	std::vector<byte> strbytes{ p_str.to_bytes(rev_charmap) };
	// we do not need the final 0xff in this context (which is guaranteed to be there)
	strbytes.pop_back();

	if (p_length_encoded)
		result.push_back(static_cast<byte>(strbytes.size()));

	for (std::size_t i{ 0 }; result.size() < max_length && i < strbytes.size(); ++i)
		result.push_back(strbytes[i]);

	while (result.size() < max_length)
		result.push_back(p_padding_byte);

	return result;
}

std::string fv::MiscWriter::to_misc_string(fv::MiscCategory p_category, fv::MiscField p_field,
	std::size_t p_index, const fv::MiscItem item) const {
	return std::format("{}{}.{} {}", category_strings.at(p_category),
		p_index, field_strings.at(p_field),
		to_value_string(get_type(p_category, p_field), item)
	);
}

std::string fv::MiscWriter::get_magic_def_string(byte p_seed) const {
	std::string result;

	for (std::size_t i{ 1 }; i < c::LABEL_MAGICS.size(); ++i) {
		byte def{ get_magic_defense(p_seed, static_cast<byte>(i)) };
		std::string reduction;
		if (def == 0)
			reduction = "0%";
		else if (def == 1)
			reduction = "50%";
		else if (def == 2)
			reduction = "100%";
		else if (def == 3)
			reduction = "100%";

		result += std::format("{}={} ", c::LABEL_MAGICS[i], reduction);
	}

	if (!result.empty())
		result.pop_back();
	return result;
}

byte fv::MiscWriter::get_magic_defense(byte p_seed, byte p_weapon_no) const {
	// no reduction for deluge
	if (p_weapon_no == 0)
		return 0;

	byte A{ p_seed };
	byte temp{ 0 };

	for (byte i{ 0 }; i < p_weapon_no; i++) {
		// First ASL/ROL pair
		byte carry = (A & 0x80) >> 7;
		A = (A << 1) & 0xFF;
		temp = ((temp << 1) & 0xFF) | carry;
		// Second ASL/ROL pair
		carry = (A & 0x80) >> 7;
		A = (A << 1) & 0xFF;
		temp = ((temp << 1) & 0xFF) | carry;
	}

	// returns magic reduction% index: 0=0%, 1=50%, 2=100%, 3=100%
	return temp & 0x03;
}
