#include "Mantra.h"
#include "mantra_math.h"
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

fman::Mantra::Mantra(int p_spawn_count) : m_special_items(std::vector<int>(c::BITS_SPECIAL_ITEMS_LENGTH, false)),
m_gamestate_flags(std::vector<int>(c::BITS_GAMESTATE_FLAGS_LENGTH, false)),
m_spawn_count{ p_spawn_count },
m_location{ 0 },
m_rank{ 0 }
{
	if (m_spawn_count < 0 || m_spawn_count > 256)
		throw std::runtime_error(std::format("Location count must be in range 1-256 (was {})", m_spawn_count));
}

fman::Mantra::Mantra(const std::string& p_str, int p_spawn_count) :
	Mantra(p_spawn_count)
{
	fm_math l_math;
	auto l_bin{ l_math.str_to_bin(p_str) };

	std::size_t idx{ c::BITS_CHAR_COUNT + c::BITS_CHAR_COUNT_LENGTH };

	// dynamic location width
	int loc_bits = l_math.bits_needed(m_spawn_count);

	m_location = l_math.extract_value(l_bin, idx, loc_bits);
	idx += loc_bits;

	m_rank = l_math.extract_value(l_bin, idx, c::BITS_RANK_LENGTH);
	idx += c::BITS_RANK_LENGTH;

	for (std::size_t i{ 0 }; i < c::BITS_SPECIAL_ITEMS_LENGTH; ++i)
		m_special_items[i] = l_math.extract_value(l_bin, idx++, 1);

	for (std::size_t i{ 0 }; i < c::BITS_GAMESTATE_FLAGS_LENGTH; ++i)
		m_gamestate_flags[i] = l_math.extract_value(l_bin, idx++, 1);

	// equipped weapon, armor, shield, magic, item
	extract_equipped(l_math, l_bin, idx, c::BITS_WEAPON_ID, m_eq_weapon);
	extract_equipped(l_math, l_bin, idx, c::BITS_ARMOR_ID, m_eq_armor);
	extract_equipped(l_math, l_bin, idx, c::BITS_SHIELD_ID, m_eq_shield);
	extract_equipped(l_math, l_bin, idx, c::BITS_MAGIC_ID, m_eq_magic);
	extract_equipped(l_math, l_bin, idx, c::BITS_ITEM_ID, m_eq_item);

	// owned weapons, armor, shields, magics, items
	extract_stored(l_math, l_bin, idx, c::BITS_WEAPON_COUNT, c::BITS_WEAPON_ID, m_weapons, c::MAX_WEAPON_ID, c::MAX_CAP_WEAPONS);
	extract_stored(l_math, l_bin, idx, c::BITS_ARMOR_COUNT, c::BITS_ARMOR_ID, m_armors, c::MAX_ARMOR_ID, c::MAX_CAP_ARMORS);
	extract_stored(l_math, l_bin, idx, c::BITS_SHIELD_COUNT, c::BITS_SHIELD_ID, m_shields, c::MAX_SHIELD_ID, c::MAX_CAP_SHIELDS);
	extract_stored(l_math, l_bin, idx, c::BITS_MAGIC_COUNT, c::BITS_MAGIC_ID, m_magics, c::MAX_MAGIC_ID, c::MAX_CAP_MAGICS);
	extract_stored(l_math, l_bin, idx, c::BITS_ITEM_COUNT, c::BITS_ITEM_ID, m_items, c::MAX_ITEM_ID, c::MAX_CAP_ITEMS);
}

void fman::Mantra::extract_equipped(const fman::fm_math& p_math,
	const std::vector<int> p_bin, std::size_t& p_idx,
	std::size_t p_item_size, std::optional<int>& p_value) {

	// check if the equipped flag is set, and if so - extract the item ID which follows
	if (p_math.extract_value(p_bin, p_idx, c::BITS_EQ_ITEM_FLAG_LENGTH) == 1) {
		p_value = p_math.extract_value(p_bin, p_idx + c::BITS_EQ_ITEM_FLAG_LENGTH, p_item_size);
		p_idx += p_item_size + c::BITS_EQ_ITEM_FLAG_LENGTH;
	}
	else
		p_idx += c::BITS_EQ_ITEM_FLAG_LENGTH;
}

void fman::Mantra::extract_stored(const fman::fm_math& p_math,
	const std::vector<int> p_bin, std::size_t& p_idx,
	std::size_t p_counter_size, std::size_t p_item_size,
	std::vector<int>& p_container,
	int p_max_item_id, std::size_t p_container_max_size) {

	// get the number of stored items
	std::size_t l_stored_count{
		static_cast<std::size_t>(p_math.extract_value(p_bin, p_idx, p_counter_size)) };
	p_idx += p_counter_size;

	// for each item, extract its ID and add it to the container
	for (std::size_t i{ 0 }; i < l_stored_count; ++i) {
		add_item_to_container(p_container,
			p_math.extract_value(p_bin, p_idx, p_item_size),
			p_max_item_id, p_container_max_size);
		p_idx += p_item_size;
	}

}

void fman::Mantra::add_equipped_bin(std::vector<int>& p_bin,
	const fm_math& p_math,
	const std::optional<int>& p_val, int p_length) const {

	if (p_val.has_value()) {
		p_bin.push_back(1);
		p_math.append_bin(p_bin, p_val.value(), p_length);
	}
	else
		p_bin.push_back(0);
}

void fman::Mantra::add_stored_bin(std::vector<int>& p_bin,
	const fm_math& p_math,
	const std::vector<int>& p_val, int p_count_length,
	int p_item_length) const {

	p_math.append_bin(p_bin, static_cast<int>(p_val.size()), p_count_length);

	for (int n : p_val)
		p_math.append_bin(p_bin, n, p_item_length);
}

std::string fman::Mantra::get_mantra(void) const {
	fm_math l_math;

	// we'll fill in checksum (8 bits) and number of chars (5 bits) later
	std::vector<int> l_bin(c::BITS_LOCATION, 0);

	l_math.append_bin(l_bin, m_location, static_cast<std::size_t>(l_math.bits_needed(m_spawn_count)));
	l_math.append_bin(l_bin, m_rank, c::BITS_RANK_LENGTH);

	l_bin.insert(end(l_bin), begin(m_special_items), end(m_special_items));
	l_bin.insert(end(l_bin), begin(m_gamestate_flags), end(m_gamestate_flags));

	add_equipped_bin(l_bin, l_math, m_eq_weapon, c::BITS_WEAPON_ID);
	add_equipped_bin(l_bin, l_math, m_eq_armor, c::BITS_ARMOR_ID);
	add_equipped_bin(l_bin, l_math, m_eq_shield, c::BITS_SHIELD_ID);
	add_equipped_bin(l_bin, l_math, m_eq_magic, c::BITS_MAGIC_ID);
	add_equipped_bin(l_bin, l_math, m_eq_item, c::BITS_ITEM_ID);

	add_stored_bin(l_bin, l_math, m_weapons, c::BITS_WEAPON_COUNT, c::BITS_WEAPON_ID);
	add_stored_bin(l_bin, l_math, m_armors, c::BITS_ARMOR_COUNT, c::BITS_ARMOR_ID);
	add_stored_bin(l_bin, l_math, m_shields, c::BITS_SHIELD_COUNT, c::BITS_SHIELD_ID);
	add_stored_bin(l_bin, l_math, m_magics, c::BITS_MAGIC_COUNT, c::BITS_MAGIC_ID);
	add_stored_bin(l_bin, l_math, m_items, c::BITS_ITEM_COUNT, c::BITS_ITEM_ID);

	// make sure the bit container's length is a multiple of the character bit size
	while (l_bin.size() % c::BITS_PER_CHAR != 0)
		l_bin.push_back(0);

	// calculate the number of characters in the final mantra and store this as
	// a 5-bit number starting at location 8
	l_math.overwrite_bin(l_bin,
		l_math.dec_to_bin(static_cast<int>(l_bin.size()) / c::BITS_PER_CHAR, c::BITS_CHAR_COUNT_LENGTH),
		c::BITS_CHAR_COUNT);

	// get the checksum
	l_math.overwrite_bin(l_bin, l_math.get_checksum_bin(l_bin), c::BITS_CHECKSUM);

	// the bit-container is ready - return it as a string
	return l_math.bin_to_str(l_bin);
}

std::pair<std::pair<int, int>, std::pair<int, int>> fman::Mantra::verify_metadata(const std::string& p_mantra) {
	fman::fm_math l_math;

	auto l_bin{ l_math.str_to_bin(p_mantra) };

	// extract the actual check-sum and character counts from the mantra
	int l_char_count = l_math.extract_value(l_bin, c::BITS_CHAR_COUNT, c::BITS_CHAR_COUNT_LENGTH);
	int l_checksum = l_math.extract_value(l_bin, c::BITS_CHECKSUM, c::BITS_CHECKSUM_LENGTH);

	// return { {actual char count, stored char count},
	//      	{actual checksum, stored checksum} }
	return std::make_pair(
		std::make_pair(static_cast<int>(p_mantra.size()), l_char_count),
		std::make_pair(l_math.get_checksum_dec(l_bin), l_checksum)
	);
}

void fman::Mantra::set_int_value(int& p_variable, int p_value, int p_max) {
	validate_int(p_value, p_max);
	p_variable = p_value;
}

void fman::Mantra::set_bool_value(std::vector<int>& p_container, std::size_t p_idx, bool p_value) {
	if (p_idx < p_container.size())
		p_container[p_idx] = static_cast<int>(p_value);
	else
		throw std::runtime_error("Invalid index " + std::to_string(p_idx));
}

void fman::Mantra::set_optional_int_value(std::optional<int>& p_variable, int p_value, int p_max) {
	validate_int(p_value, p_max);
	p_variable = p_value;
}

bool fman::Mantra::add_item_to_container(std::vector<int>& p_container,
	int p_value, int p_max, std::size_t p_max_csize) {
	validate_int(p_value, p_max);

	if (p_container.size() < p_max_csize) {
		p_container.push_back(p_value);
		return true;
	}
	else
		return false;
}

bool fman::Mantra::pop_container(std::vector<int>& p_container) {
	if (p_container.empty())
		return false;
	else {
		p_container.pop_back();
		return true;
	}
}

void fman::Mantra::validate_int(int p_value, int p_max) const {
	if (p_value < 0 || p_value > p_max)
		throw std::runtime_error("Invalid value " + std::to_string(p_value));
}

// setters
void fman::Mantra::set_rank(int p_rank) {
	set_int_value(m_rank, p_rank, c::MAX_RANK);
}

void fman::Mantra::set_location(int p_location) {
	set_int_value(m_location, p_location, c::MAX_LOCATION);

	if (m_location >= m_spawn_count)
		m_spawn_count = m_location + 1;
}

void fman::Mantra::set_gamestate_flag(std::size_t p_idx, bool p_value) {
	set_bool_value(m_gamestate_flags, p_idx, p_value);
}

void fman::Mantra::set_special_item(std::size_t p_idx, bool p_value) {
	set_bool_value(m_special_items, p_idx, p_value);
}

void fman::Mantra::set_eq_weapon(int p_value) {
	set_optional_int_value(m_eq_weapon, p_value, c::MAX_WEAPON_ID);
}

void fman::Mantra::set_eq_armor(int p_value) {
	set_optional_int_value(m_eq_armor, p_value, c::MAX_ARMOR_ID);
}

void fman::Mantra::set_eq_shield(int p_value) {
	set_optional_int_value(m_eq_shield, p_value, c::MAX_SHIELD_ID);
}

void fman::Mantra::set_eq_magic(int p_value) {
	set_optional_int_value(m_eq_magic, p_value, c::MAX_MAGIC_ID);
}

void fman::Mantra::set_eq_item(int p_value) {
	set_optional_int_value(m_eq_item, p_value, c::MAX_ITEM_ID);
}

bool fman::Mantra::add_stored_weapon(int p_value) {
	return add_item_to_container(m_weapons, p_value, c::MAX_WEAPON_ID, c::MAX_CAP_WEAPONS);
}

bool fman::Mantra::add_stored_armor(int p_value) {
	return add_item_to_container(m_armors, p_value, c::MAX_ARMOR_ID, c::MAX_CAP_ARMORS);
}

bool fman::Mantra::add_stored_shield(int p_value) {
	return add_item_to_container(m_shields, p_value, c::MAX_SHIELD_ID, c::MAX_CAP_SHIELDS);
}

bool fman::Mantra::add_stored_magic(int p_value) {
	return add_item_to_container(m_magics, p_value, c::MAX_MAGIC_ID, c::MAX_CAP_MAGICS);
}

bool fman::Mantra::add_stored_item(int p_value) {
	return add_item_to_container(m_items, p_value, c::MAX_ITEM_ID, c::MAX_CAP_ITEMS);
}

void fman::Mantra::clear_eq_weapon(void) {
	m_eq_weapon.reset();
}

void fman::Mantra::clear_eq_armor(void) {
	m_eq_armor.reset();
}
void fman::Mantra::clear_eq_shield(void) {
	m_eq_shield.reset();
}
void fman::Mantra::clear_eq_magic(void) {
	m_eq_magic.reset();
}
void fman::Mantra::clear_eq_item(void) {
	m_eq_item.reset();
}

void fman::Mantra::clear_stored_weapons(void) {
	m_weapons.clear();
}

void fman::Mantra::clear_stored_armors(void) {
	m_armors.clear();
}

void fman::Mantra::clear_stored_shields(void) {
	m_shields.clear();
}

void fman::Mantra::clear_stored_magics(void) {
	m_magics.clear();
}

void fman::Mantra::clear_stored_items(void) {
	m_items.clear();
}

bool fman::Mantra::pop_stored_weapon(void) {
	return pop_container(m_weapons);
}

bool fman::Mantra::pop_stored_armor(void) {
	return pop_container(m_armors);
}

bool fman::Mantra::pop_stored_shield(void) {
	return pop_container(m_shields);
}

bool fman::Mantra::pop_stored_magic(void) {
	return pop_container(m_magics);
}

bool fman::Mantra::pop_stored_item(void) {
	return pop_container(m_items);
}

// getters
int fman::Mantra::get_rank(void) const {
	return m_rank;
}

int fman::Mantra::get_location(void) const {
	return m_location;
}

bool fman::Mantra::get_gamestate(std::size_t p_idx) const {
	return m_gamestate_flags.at(p_idx);
}

bool fman::Mantra::get_special_item(std::size_t p_idx) const {
	return m_special_items.at(p_idx);
}

bool fman::Mantra::has_eq_weapon(void) const {
	return m_eq_weapon.has_value();
}

bool fman::Mantra::has_eq_armor(void) const {
	return m_eq_armor.has_value();
}

bool fman::Mantra::has_eq_shield(void) const {
	return m_eq_shield.has_value();
}

bool fman::Mantra::has_eq_magic(void) const {
	return m_eq_magic.has_value();
}

bool fman::Mantra::has_eq_item(void) const {
	return m_eq_item.has_value();
}

int fman::Mantra::get_eq_weapon(void) const {
	return m_eq_weapon.value();
}

int fman::Mantra::get_eq_armor(void) const {
	return m_eq_armor.value();
}

int fman::Mantra::get_eq_shield(void) const {
	return m_eq_shield.value();
}

int fman::Mantra::get_eq_magic(void) const {
	return m_eq_magic.value();
}

int fman::Mantra::get_eq_item(void) const {
	return m_eq_item.value();
}

std::size_t fman::Mantra::get_stored_weapon_count(void) const {
	return m_weapons.size();
}

std::size_t fman::Mantra::get_stored_armor_count(void) const {
	return m_armors.size();
}

std::size_t fman::Mantra::get_stored_shield_count(void) const {
	return m_shields.size();
}

std::size_t fman::Mantra::get_stored_magic_count(void) const {
	return m_magics.size();
}

std::size_t fman::Mantra::get_stored_item_count(void) const {
	return m_items.size();
}

int fman::Mantra::get_stored_weapon(std::size_t p_idx) const {
	return m_weapons[p_idx];
}

int fman::Mantra::get_stored_armor(std::size_t p_idx) const {
	return m_armors[p_idx];
}

int fman::Mantra::get_stored_shield(std::size_t p_idx) const {
	return m_shields[p_idx];
}

int fman::Mantra::get_stored_magic(std::size_t p_idx) const {
	return m_magics[p_idx];
}

int fman::Mantra::get_stored_item(std::size_t p_idx) const {
	return m_items[p_idx];
}

std::string fman::Mantra::get_printable_summary(void) const {

	const auto str_eq = []<std::size_t N>(const std::array<std::string_view, N>&p_labels,
		const std::string & p_type, const std::optional<int>&p_val) -> std::string {
		std::string l_result{ "Equipped " + p_type + ": " };

		if (p_val.has_value())
			l_result += std::string(p_labels[p_val.value()]);
		else
			l_result += "(none)";

		return l_result;
	};

	const auto str_stored = []<std::size_t N>(const std::array<std::string_view, N>&p_labels,
		const std::string & p_type, const std::vector<int>&p_container) -> std::string {
		std::string l_result{ "Stored " + p_type + "s (" + std::to_string(p_container.size()) + "): " };

		if (!p_container.empty()) {
			for (std::size_t i{ 0 }; i < p_container.size(); ++i) {
				l_result += std::string(p_labels[p_container[i]]);
				if (i != p_container.size() - 1)
					l_result += ", ";
			}
		}
		else
			l_result += "(none)";

		return l_result;
	};

	const auto str_bools = []<std::size_t N>(const std::array<std::string_view, N>&p_labels,
		const std::string & p_type, const std::vector<int>&p_container) -> std::string {

		std::vector<std::string> l_set_values;
		for (std::size_t i{ 0 }; i < p_container.size(); ++i)
			if (p_container[i])
				l_set_values.push_back(std::string(p_labels[i]));

		std::string l_result{ p_type + " (" + std::to_string(l_set_values.size()) +
		"): " };

		if (!l_set_values.empty()) {
			for (std::size_t i{ 0 }; i < l_set_values.size(); ++i) {
				l_result += l_set_values[i];
				if (i != l_set_values.size() - 1)
					l_result += ", ";
			}
		}
		else
			l_result += "(none)";

		return l_result;
	};

	std::string l_mantra{ get_mantra() };
	auto l_metadata{ verify_metadata(l_mantra) };

	std::string result{ "Mantra: " + l_mantra };
	result += "\nChecksum: " + std::to_string(l_metadata.second.first);
	result += "\nCharacter count: " + std::to_string(l_metadata.first.first);

	if (m_location < 8)
		result += std::format("\n\nLocation: {} ({} of {})",
			std::string(fman::c::LABELS_LOCATIONS[m_location]), m_location, m_spawn_count);
	else
		result += std::format("\n\nLocation: {} (of {})", m_location, m_spawn_count);

	result += "\nRank: " + std::string(fman::c::LABELS_RANKS[m_rank]);

	result += "\n\n" + str_eq(fman::c::LABELS_WEAPONS, "weapon", m_eq_weapon);
	result += "\n" + str_eq(fman::c::LABELS_ARMORS, "armor", m_eq_armor);
	result += "\n" + str_eq(fman::c::LABELS_SHIELDS, "shield", m_eq_shield);
	result += "\n" + str_eq(fman::c::LABELS_MAGICS, "magic", m_eq_magic);
	result += "\n" + str_eq(fman::c::LABELS_ITEMS, "item", m_eq_item);

	result += "\n\n" + str_stored(fman::c::LABELS_WEAPONS, "weapon", m_weapons);
	result += "\n" + str_stored(fman::c::LABELS_ARMORS, "armor", m_armors);
	result += "\n" + str_stored(fman::c::LABELS_SHIELDS, "shield", m_shields);
	result += "\n" + str_stored(fman::c::LABELS_MAGICS, "magic", m_magics);
	result += "\n" + str_stored(fman::c::LABELS_ITEMS, "item", m_items);

	result += "\n\n" + str_bools(fman::c::LABELS_SPECIAL_ITEMS, "Special items", m_special_items);
	result += "\n" + str_bools(fman::c::LABELS_GAMESTATE_FLAGS, "Gamestate flags", m_gamestate_flags);

	return result;
}
