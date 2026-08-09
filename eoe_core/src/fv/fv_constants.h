#ifndef FV_CONSTANTS_H
#define FV_CONSTANTS_H

#include <map>
#include <set>
#include <string>
#include <vector>

using byte = unsigned char;

namespace fv {

	namespace c {

		// xml identifiers
		constexpr char ID_ISCRIPT_CHARS[]{ "iscript_string_characters" };
		constexpr char ID_TITLE_STRING_OFFSET[]{ "title_string_offset" };
		constexpr char ID_TITLE_STRING_END_OFFSET[]{ "title_string_end_offset" };
		constexpr char ID_STATUS_STRING_COUNT[]{ "status_string_count" };
		constexpr char ID_STATUS_STRING_OFFSET[]{ "status_string_offset" };
		constexpr char ID_ITEM_STRING_COUNT[]{ "item_string_count" };
		constexpr char ID_ITEM_STRING_OFFSET[]{ "item_string_offset" };
		constexpr char ID_PASSWORD_STRING_COUNT[]{ "password_string_count" };
		constexpr char ID_PASSWORD_STRING_OFFSET[]{ "password_string_offset" };

		constexpr char ID_RANK_COUNT[]{ "rank_count" };
		constexpr char ID_RANK_STRING_LENGTH[]{ "rank_string_length" };
		constexpr char ID_RANK_STRING_OFFSET[]{ "rank_string_offset" };
		constexpr char ID_RANK_DATA_OFFSET[]{ "rank_data_offset" };

		constexpr char ID_SPRITE_COUNT[]{ "sprite_count" };
		constexpr char ID_SPRITE_LABELS[]{ "sprite_labels" };
		constexpr char ID_SPRITE_DROP_TABLE_OFFSET[]{ "sprite_drop_table_offset" };
		constexpr char ID_SPRITE_DROP_TABLE_COUNT[]{ "sprite_drop_table_count" };
		constexpr char ID_SPRITE_TYPE_OFFSET[]{ "sprite_type_offset" };
		constexpr char ID_SPRITE_HP_OFFSET[]{ "sprite_hp_offset" };
		constexpr char ID_SPRITE_HP_COUNT[]{ "sprite_hp_count" };
		constexpr char ID_SPRITE_XP_OFFSET[]{ "sprite_xp_offset" };
		constexpr char ID_SPRITE_XP_COUNT[]{ "sprite_xp_count" };
		constexpr char ID_SPRITE_DROP_IDX_OFFSET[]{ "sprite_drop_idx_offset" };
		constexpr char ID_SPRITE_DROP_IDX_COUNT[]{ "sprite_drop_idx_count" };
		constexpr char ID_SPRITE_DAMAGE_OFFSET[]{ "sprite_damage_offset" };
		constexpr char ID_SPRITE_DAMAGE_COUNT[]{ "sprite_damage_count" };
		constexpr char ID_SPRITE_DEFENSE_OFFSET[]{ "sprite_defense_offset" };
		constexpr char ID_SPRITE_DEFENSE_COUNT[]{ "sprite_defense_count" };

		constexpr char ID_WEAPON_COUNT[]{ "weapon_count" };
		constexpr char ID_MAGIC_COUNT[]{ "magic_count" };
		constexpr char ID_ARMOR_COUNT[]{ "armor_count" };
		constexpr char ID_WEAPON_DAMAGE_OFFSET[]{ "weapon_damage_offset" };
		constexpr char ID_WEAPON_GLOVE_DAMAGE_OFFSET[]{ "weapon_glove_damage_offset" };
		constexpr char ID_MAGIC_DAMAGE_OFFSET[]{ "magic_damage_offset" };
		constexpr char ID_MAGIC_COST_OFFSET[]{ "magic_cost_offset" };
		constexpr char ID_ARMOR_DEFENSE_OFFSET[]{ "armor_defense_offset" };

		constexpr char ID_WING_BOOTS_TIME_COUNT[]{ "wing_boots_time_count" };
		constexpr char ID_WING_BOOTS_TIME_OFFSET[]{ "wing_boots_time_offset" };

		constexpr char CAT_TITLE_STRING[]{ "TitleString" };
		constexpr char CAT_STATUS_STRING[]{ "StatusString" };
		constexpr char CAT_ITEM_STRING[]{ "ItemString" };
		constexpr char CAT_PASSWORD_STRING[]{ "PasswordString" };
		constexpr char CAT_MAGIC[]{ "Magic" };
		constexpr char CAT_RANK[]{ "Rank" };
		constexpr char CAT_SPRITE[]{ "Sprite" };
		constexpr char CAT_WEAPON[]{ "Weapon" };
		constexpr char CAT_ARMOR[]{ "Armor" };
		constexpr char CAT_DROP_TABLE[]{ "DropTable" };
		constexpr char CAT_WINGBOOTS[]{ "WingBoots" };

		constexpr char FIELD_TEXT[]{ "Text" };
		constexpr char FIELD_XP[]{ "XP" };
		constexpr char FIELD_HP[]{ "HP" };
		constexpr char FIELD_GOLD[]{ "Gold" };
		constexpr char FIELD_BREAD[]{ "Bread" };
		constexpr char FIELD_DROP_INDEX[]{ "DropIndex" };
		constexpr char FIELD_DAMAGE[]{ "Damage" };
		constexpr char FIELD_GLOVE[]{ "Glove" };
		constexpr char FIELD_COST[]{ "Cost" };
		constexpr char FIELD_MAGIC_DEFENSE[]{ "MagicDefense" };
		constexpr char FIELD_DEFENSE[]{ "Defense" };
		constexpr char FIELD_SECONDS[]{ "Seconds" };

		// where bread drops (an no longer coin drops) start
		constexpr std::size_t DROP_TABLE_CUTOFF{ 0x30 };
		constexpr byte STR_PADDING_BYTE{ 0x20 };

		inline const std::map<byte, std::string> SPRITE_CATS{
			{0x00, "Enemy"},
			{0x01, "Dropped"},
			{0x02, "NPC"},
			{0x03, "Effect"},
			{0x04, "Trigger"},
			{0x05, "Item"},
			{0x06, "Magic"},
			{0x07, "Boss"}
		};

		// by default we only consider sprites of types enemy and boss
		inline const std::set<byte> RELEVANT_SPRITE_CATS{ 0x00, 0x07 };
		inline const std::vector<std::string> LABEL_MAGICS{ "Deluge", "Thunder", "Fire", "Death", "Tilte" };
		inline const std::vector<std::string> LABEL_WEAPONS{ "Hand Dagger", "Long Sword", "Giant Blade", "Dragon Slayer" };

		// characters that map to ascii for the item and password strings
		constexpr char CHR_PALETTE_ITEMS[]{ "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789: " };
		constexpr char CHR_PALETTE_MANTRA[]{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!_-.,? " };
	}

}

#endif
