#include "MMLSong.h"
#include "common/klib/Kstring.h"
#include "mml_constants.h"
#include <algorithm>
#include <format>
#include "LilyPond.h"

std::string fm::MMLSong::get_title(void) const {
	if (m_title.empty())
		return std::format("Song {:02}", index);
	else
		return m_title;
}

std::string fm::MMLSong::get_time_sig(void) const {
	if (m_time_sig.empty())
		return "4/4";
	else
		return m_time_sig;
}

std::string fm::MMLSong::to_string(void) const {
	fm::Fraction tpq{ fm::Fraction(c::TICK_PER_MIN, 1) / tempo };

	std::string result{ std::format("#song {}\n", index) };
	result += std::format("t{}\t\t; {} ticks per quarter note\n\n",
		tempo.to_tempo_string(), tpq.to_tempo_string()
	);

	for (const auto& ch : channels)
		result += ch.to_string() + "\n";

	return result;
}

smf::MidiFile fm::MMLSong::to_midi(const std::vector<int>& p_global_transpose) {
	smf::MidiFile l_midi;

	l_midi.setTicksPerQuarterNote(60);
	l_midi.addTempo(0, 0, static_cast<double>(c::TICK_PER_MIN) / static_cast<double>(60));

	int songtransp{ channels.at(0).get_song_transpose() };

	int max_ticks{ 0 };
	max_ticks = std::max(max_ticks, channels.at(0).add_midi_track(l_midi, 0, p_global_transpose.at(0) + songtransp));
	max_ticks = std::max(max_ticks, channels.at(1).add_midi_track(l_midi, 1, p_global_transpose.at(1) + +songtransp));
	max_ticks = std::max(max_ticks, channels.at(2).add_midi_track(l_midi, 2, p_global_transpose.at(2) + songtransp));
	channels.at(3).add_midi_track(l_midi, 9, 0, max_ticks);

	return l_midi;
}

std::string fm::MMLSong::to_lilypond(const std::vector<int>& p_global_transpose,
	bool p_incl_percussion) {
	std::string l_lp{ lp::header(get_title(), tempo) };

	int songtransp{ channels.at(0).get_song_transpose() };

	for (std::size_t i{ 0 }; i < channels.size(); ++i)
		if (i != 3 || p_incl_percussion)
			channels[i].add_lilypond_staff(l_lp, p_global_transpose.at(i) + songtransp,
				get_time_sig());

	l_lp += lp::footer();
	return l_lp;
}

fm::MMLChannel fm::MMLSong::get_channel_of_type(fm::ChannelType p_chan_type) const {

	const auto get_channel_index = [*this](fm::ChannelType p_type) -> std::size_t {
		for (std::size_t i{ 0 }; i < channels.size(); ++i)
			if (channels[i].channel_type == p_type)
				return i;
		throw std::runtime_error(std::format("Song {} is missing a channel", index));
		};

	return channels[get_channel_index(p_chan_type)];
}

void fm::MMLSong::sort(void) {

	std::vector<MMLChannel> l_new_chans;

	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::sq1));
	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::sq2));
	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::tri));
	l_new_chans.push_back(get_channel_of_type(fm::ChannelType::noise));

	channels = l_new_chans;
}
