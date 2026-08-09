#ifndef FM_LILYPOND_H
#define FM_LILYPOND_H

#include "Fraction.h"
#include <string>

namespace fm {

	namespace lp {

		std::string gen_tagline(void);
		std::string header(const std::string& p_title, const fm::Fraction& p_tempo);
		std::string footer(void);
		std::string emit_octave(int p_octave);
		std::string emit_note(int p_pitch, int p_octave, const std::string& p_length);
		std::string emit_percussion(int p_perc_no, const std::string& p_length);
		std::string emit_rest(const std::string& p_length);

		std::string emit_midi_instrument(int p_duty_cycle);
	}

}

#endif
