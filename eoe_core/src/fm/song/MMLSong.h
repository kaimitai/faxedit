#ifndef FM_MML_SONG_H
#define FM_MML_SONG_H

#include <string>
#include <optional>
#include <vector>
#include "MMLChannel.h"
#include "Fraction.h"
#include "common/midifile/MidiFile.h"

namespace fm {

	struct MMLSong {

		std::vector<fm::MMLChannel> channels;
		int index{ 0 };
		fm::Fraction tempo{ fm::Fraction(100, 1) };

		// lilypond functions
		std::string m_title, m_time_sig;

		std::string get_title(void) const;
		std::string get_time_sig(void) const;

	public:
		MMLSong(void) = default;
		std::string to_string(void) const;
		smf::MidiFile to_midi(const std::vector<int>& p_global_transpose);
		std::string to_lilypond(const std::vector<int>& p_global_transpose,
			bool p_incl_percussion);
		void sort(void);
		fm::MMLChannel get_channel_of_type(fm::ChannelType p_chan_type) const;
	};

}

#endif
