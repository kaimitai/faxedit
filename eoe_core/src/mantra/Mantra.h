#ifndef FM_MANTRA_H
#define FM_MANTRA_H

#include <iostream>
#include <string>
#include <utility>
#include <optional>
#include <vector>
#include "mantra_math.h"

namespace fman {

	class Mantra {

		// spawn point count
		int m_spawn_count;

		// rank, guru location
		int m_rank, m_location;
		
		// equipped gear
		std::optional<int> m_eq_weapon, m_eq_armor, m_eq_shield,
			m_eq_magic, m_eq_item;

		// owned gear
		std::vector<int> m_weapons, m_armors, m_shields, m_magics,
			m_items;

		// special items held and gamestate flags
		std::vector<int> m_special_items, m_gamestate_flags;

		// extract equipped items from the bit-stream
		void extract_equipped(const fman::fm_math& p_math,
			const std::vector<int> p_bin, std::size_t& p_idx,
			std::size_t p_item_size, std::optional<int>& p_value);

		// extract stored items from the bit-container
		void extract_stored(const fman::fm_math& p_math,
			const std::vector<int> p_bin, std::size_t& p_idx,
			std::size_t p_counter_size, std::size_t p_item_size,
			std::vector<int>& p_container,
			int p_max_item_id, std::size_t p_container_max_size);

		// store equipped items as bit-containers
		void add_equipped_bin(std::vector<int>& p_bin,
			const fm_math& p_math,
			const std::optional<int>& p_val, int p_length) const;

		// store stored items as bit-containers
		void add_stored_bin(std::vector<int>& p_bin,
			const fm_math& p_math,
			const std::vector<int>& p_val, int p_count_length,
			int p_item_length) const;

		// internal helper function for setting member ints
		void set_int_value(int& p_variable, int p_value, int p_max);
		// internal helper function for setting bools in member container
		void set_bool_value(std::vector<int>& p_container, std::size_t p_idx, bool p_value);
		// internal helper function for setting optional<int> value
		void set_optional_int_value(std::optional<int>& p_variable, int p_value, int p_max);
		// internal helper function for adding entry to container
		bool add_item_to_container(std::vector<int>& p_container, int p_value, int p_max, std::size_t p_max_csize);
		// internal helper function for popping from a container
		bool pop_container(std::vector<int>& p_container);
				
		void validate_int(int p_value, int p_max) const;

	public:
		Mantra(const std::string&, int p_spawn_count);
		Mantra(int p_spawn_count);
		std::string get_mantra(void) const;
		std::string get_printable_summary(void) const;

		// extract stored and acutal metadata values
		// { {actual char count, stored char count}, {actual checksum, stored checksum} }
		static std::pair<std::pair<int, int>, std::pair<int, int>> verify_metadata(const std::string&);

		// setters
		void set_rank(int p_rank);
		void set_location(int p_rank);
		void set_gamestate_flag(std::size_t p_idx, bool p_value);
		void set_special_item(std::size_t p_idx, bool p_value);

		void set_eq_weapon(int p_value);
		void set_eq_armor(int p_value);
		void set_eq_shield(int p_value);
		void set_eq_magic(int p_value);
		void set_eq_item(int p_value);

		bool add_stored_weapon(int p_value);
		bool add_stored_armor(int p_value);
		bool add_stored_shield(int p_value);
		bool add_stored_magic(int p_value);
		bool add_stored_item(int p_value);

		void clear_eq_weapon(void);
		void clear_eq_armor(void);
		void clear_eq_shield(void);
		void clear_eq_magic(void);
		void clear_eq_item(void);

		void clear_stored_weapons(void);
		void clear_stored_armors(void);
		void clear_stored_shields(void);
		void clear_stored_magics(void);
		void clear_stored_items(void);

		bool pop_stored_weapon(void);
		bool pop_stored_armor(void);
		bool pop_stored_shield(void);
		bool pop_stored_magic(void);
		bool pop_stored_item(void);

		// getters
		int get_rank(void) const;
		int get_location(void) const;
		bool get_gamestate(std::size_t p_idx) const;
		bool get_special_item(std::size_t p_idx) const;

		bool has_eq_weapon(void) const;
		bool has_eq_armor(void) const;
		bool has_eq_shield(void) const;
		bool has_eq_magic(void) const;
		bool has_eq_item(void) const;

		int get_eq_weapon(void) const;
		int get_eq_armor(void) const;
		int get_eq_shield(void) const;
		int get_eq_magic(void) const;
		int get_eq_item(void) const;

		std::size_t get_stored_weapon_count(void) const;
		std::size_t get_stored_armor_count(void) const;
		std::size_t get_stored_shield_count(void) const;
		std::size_t get_stored_magic_count(void) const;
		std::size_t get_stored_item_count(void) const;

		int get_stored_weapon(std::size_t p_idx) const;
		int get_stored_armor(std::size_t p_idx) const;
		int get_stored_shield(std::size_t p_idx) const;
		int get_stored_magic(std::size_t p_idx) const;
		int get_stored_item(std::size_t p_idx) const;

	};

}

#endif
