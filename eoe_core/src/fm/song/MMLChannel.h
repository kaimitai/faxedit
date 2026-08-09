#ifndef FM_MML_CHANNEL_H
#define FM_MML_CHANNEL_H

#include <map>
#include <optional>
#include <set>
#include <stack>
#include <string>
#include <utility>
#include <vector>
#include "MMLEvent.h"
#include "Fraction.h"
#include "mml_constants.h"
#include "fm/MusicOpcode.h"
#include "common/midifile/MidiFile.h"

using byte = unsigned char;

namespace fm {

	struct ChannelBytecodeExport {
		std::vector<fm::MusicInstruction> instrs;
		std::size_t entrypt_idx;
	};

	struct TieResult {
		int pitch;
		Duration total;
	};

	struct DefaultLength {
		std::optional<int> length;
		std::optional<int> raw;
		int dots = 0;
	};

	struct ChannelState {
		int sq_duty_cycle = 0;
		int sq_vol = 0;

		int octave = 4;
		int ticklength = 1;
		int pitchoffset = 0;
		int volume = 15;
		fm::Fraction fraq;
		DefaultLength default_length;
	};

	struct VM {
		std::size_t pc{ 0 };
		fm::Fraction tempo = fm::Fraction(0, 1);
		ChannelState st;
		int loop_count{ 0 }, loop_iters{ 0 };
		std::optional<std::size_t> push_addr, jsr_addr,
			loop_addr, loop_end_addr;
	};

	struct VMState {
		std::size_t pc;
		int loop_count, loop_iters, pitch_offset, volume, duty_cycle;
		std::optional<std::size_t> push_addr, jsr_addr,
			loop_addr, loop_end_addr;

		bool operator<(const VMState& rhs) const;
	};

	struct TickResult {
		int whole; // whole ticks
		fm::Fraction fraq;
	};

	struct MMLChannel {

		// VM variables
		VM vm;
		void reset_vm(bool point_at_start = false);

		fm::ChannelType channel_type;
		std::vector<fm::MmlEvent> events;
		fm::Fraction song_tempo;

		// lilypond
		std::string m_clef;

		MMLChannel(fm::Fraction p_song_tempo);
		std::size_t get_start_index(void) const;
		std::size_t get_label_addr(const std::string& label);
		int get_song_transpose(void) const;

		fm::TickResult tick_length(const fm::Duration& dur) const;
		void emit_set_length_bytecode_if_necessary(
			std::vector<fm::MusicInstruction>& instrs,
			std::optional<int> length = std::nullopt,
			int dots = 0,
			std::optional<int> raw = std::nullopt,
			bool no_vm_advance = false);
		void emit_set_length_bytecode_if_necessary(
			std::vector<fm::MusicInstruction>& instrs,
			const fm::Duration& p_duration,
			bool no_vm_advance = false);
		int advance_vm_ticks(const fm::Duration& dur,
			bool no_vm_advance = false);
		fm::Duration resolve_duration(std::optional<int> length = std::nullopt,
			int dots = 0,
			std::optional<int> raw = std::nullopt) const;
		int note_index(int octave, int pitch) const;
		std::pair<int, int> split_note(byte p_note_no) const;
		fm::TieResult resolve_tie_chain(void);
		byte ints_to_sq_control(int duty, int envLoop, int constVol, int volume) const;
		byte int_to_volume_byte(int p_volume) const;
		byte int_to_byte(int n) const;
		int byte_to_int(byte b) const;
		int byte_to_volume(byte b) const;

		// DEBUG functions
		std::string note_index_to_str(int p_idx) const;

		// output functions
		std::string note_no_to_str(int p_note_no) const;
		std::string to_string(void) const;
		std::string channel_type_to_string(void) const;
		bool is_square_channel(void) const;

		// bytecode to mml function
		void parse_bytecode(const std::vector<fm::MusicInstruction>& instrs,
			std::size_t entrypoint_idx, std::set<std::size_t> jump_targets);
		std::vector<DefaultLength> calc_tick_lengths(void) const;

		// midi functions
		int add_midi_track(smf::MidiFile& p_midi, int p_channel_no,
			int p_pitch_offset, int p_max_ticks = -1);

		// lilypond export
		int add_lilypond_staff(std::string& p_lp, int p_pitch_offset, const std::string& p_time_sig);
		std::string to_lilypond_length(std::optional<int> p_length,
			int p_dots, std::optional<int> p_raw, fm::Fraction& p_bar_accum) const;
		std::string get_lilypond_clef(void) const;

		std::string volume_to_marker(int p_vol) const;
		bool is_empty(void) const;

		// mml to bytecode function
		fm::ChannelBytecodeExport to_bytecode(void);
		byte encode_percussion(int p_perc, int p_repeat) const;
	};

}

#endif
