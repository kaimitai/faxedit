#include "Fraction.h"
#include <format>
#include <numeric>
#include <stdexcept>

fm::Fraction::Fraction(void) :
	num{ 0 }, den{ 1 }
{
}

fm::Fraction::Fraction(int p_num, int p_den) :
	num{ p_num }, den{ p_den }
{
	if (den == 0)
		throw std::runtime_error("Fraction has denominator 0");

	normalize();
}

int fm::Fraction::get_num(void) const {
	return num;
}

int fm::Fraction::get_den(void) const {
	return den;
}

bool fm::Fraction::is_zero(void) const {
	return num == 0;
}

bool fm::Fraction::is_integer(void) const {
	return den == 1;
}

double fm::Fraction::to_double(void) const {
	return static_cast<double>(num) / static_cast<double>(den);
}

std::string fm::Fraction::to_tempo_string(void) const {
	// whole part
	int whole = num / den;

	// remainder part
	int rem = num % den;

	// If no fractional part -> just "NNN"
	if (rem == 0) {
		return std::format("{}", whole);
	}

	// Otherwise -> "NNN+p/q"
	return std::format("{}+{}/{}", whole, rem, den);
}


fm::Fraction& fm::Fraction::operator+=(const fm::Fraction& rhs) {
	*this = *this + rhs;
	return *this;
}

fm::Fraction fm::Fraction::operator+(const fm::Fraction& rhs) const {
	int common = std::lcm(den, rhs.den);
	int a = num * (common / den);
	int b = rhs.num * (common / rhs.den);

	return Fraction(a + b, common);
}

void fm::Fraction::normalize(void) {
	if (num == 0) {
		den = 1;
		return;
	}

	int g = std::gcd(std::abs(num), std::abs(den));
	num /= g;
	den /= g;

	if (den < 0) {
		den = -den;
		num = -num;
	}
}

fm::Fraction fm::Fraction::operator*(const Fraction& rhs) const {
	Fraction out(num * rhs.num, den * rhs.den);
	return out;
}

fm::Fraction& fm::Fraction::operator*=(const Fraction& rhs) {
	num *= rhs.num;
	den *= rhs.den;
	normalize();
	return *this;
}

fm::Fraction fm::Fraction::operator/(const Fraction& rhs) const {
	// (a/b) / (c/d) = (a*d) / (b*c)
	Fraction out(num * rhs.den, den * rhs.num);
	return out;
}

fm::Fraction& fm::Fraction::operator/=(const Fraction& rhs) {
	num *= rhs.den;
	den *= rhs.num;
	normalize();   // constructor does this, but *= must do it manually
	return *this;
}

bool fm::Fraction::operator==(const Fraction& rhs) const {
	return num * rhs.den == den * rhs.num;
}

// lexiographic only, let's assume all fractions are normalized (they should be)
bool fm::Fraction::operator<(const Fraction& rhs) const {
	return num * rhs.den < den * rhs.num;
}

// Extract whole ticks from the fraction
int fm::Fraction::extract_whole(void) {
	int whole = num / den;
	num = num % den;
	normalize();

	return whole;
}

bool fm::Duration::is_raw(void) const {
	return raw.has_value();
}

bool fm::Duration::is_empty(void) const {
	return !is_raw() && musical.is_zero();
}

fm::Duration fm::Duration::operator+(const Duration& rhs) const {
	Duration out = *this;
	
	// If either side is raw, the result must be raw.
	if (is_raw() || rhs.is_raw()) {
		// Prefer the left-hand raw if present, otherwise the right-hand.
		out.raw = raw.value_or(rhs.raw.value());
		return out;
	}
	
	// Both are musical -> add fractions
	out.musical += rhs.musical;
	return out;
}

fm::Duration fm::Duration::from_raw_ticks(int ticks) {
	Duration d;
	d.raw = ticks;
	return d;
}

fm::Duration fm::Duration::from_length_and_dots(int length, int dots) {
	fm::Duration d; // base = 1/length
	fm::Fraction base(1, length); // dot_factor = (2^(dots+1) - 1) / 2^dots
	int pow2 = 1 << dots;
	
	fm::Fraction dot_factor(2 * pow2 - 1, pow2);
	d.musical = base * dot_factor;
	
	return d;
}
