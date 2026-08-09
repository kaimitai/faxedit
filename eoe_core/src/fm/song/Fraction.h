#ifndef FM_FRACTION_H
#define FM_FRACTION_H

#include <string>
#include <optional>

namespace fm {

	class Fraction {

		int num, den;
		void normalize(void);

	public:
		Fraction(void);
		Fraction(int p_num, int p_den);
		bool is_zero(void) const;
		
		Fraction operator+(const Fraction& rhs) const;
		Fraction& operator+=(const Fraction& rhs);
		fm::Fraction operator*(const Fraction& rhs) const;
		fm::Fraction& operator*=(const Fraction& rhs);
		fm::Fraction operator/(const Fraction& rhs) const;
		fm::Fraction& operator/=(const Fraction& rhs);

		bool operator==(const Fraction& rhs) const;
		bool operator<(const Fraction& rhs) const;
		int extract_whole(void);

		int get_num(void) const;
		int get_den(void) const;

		bool is_integer(void) const;
		double to_double(void) const;
		std::string to_tempo_string(void) const;
	};

	// note length struct which makes use of fractions
	struct Duration {
		Fraction musical; // rational musical duration (e.g. 3/32)
		std::optional<int> raw;

		bool is_raw(void) const;
		bool is_empty(void) const;
		fm::Duration operator+(const Duration& rhs) const;

		// factory functions
		static fm::Duration from_raw_ticks(int ticks);
		static fm::Duration from_length_and_dots(int length, int dots);
	};

}

#endif
