#ifndef FM_CONSTANTS_H
#define FM_CONSTANTS_H

#include <map>
#include <string>
#include <vector>
#include <set>
#include "fm/song/Fraction.h"

using byte = unsigned char;

namespace fm {

	namespace c {

		constexpr char ID_MUSIC_PTR[]{ "music_ptr" };
		constexpr char ID_MUSIC_DATA_END[]{ "music_data_end" };
		constexpr char ID_MSCRIPT_OPCODES[]{ "mscript_opcodes" };
		constexpr char ID_CHAN_PITCH_OFFSET[]{ "music_channel_pitch_offset" };

		constexpr char ID_DEFINES_ENVELOPE[]{ "mscript_defines_env" };

		constexpr char ID_MSCRIPT_START_OCTAVE[]{ "music_first_octave" };
		constexpr char ID_MSCRIPT_START_NOTE[]{ "music_first_note" };
		constexpr char ID_MSCRIPT_START_BYTE[]{ "music_first_byte_value" };

		constexpr char ID_MSCRIPT_ENTRYPOINT[]{ ".song" };

		constexpr char XML_OPCODE_PARAM_MNEMONIC[]{ "Mnemonic" };
		constexpr char XML_OPCODE_PARAM_ARG[]{ "Arg" };
		constexpr char XML_OPCODE_PARAM_FLOW[]{ "Flow" };
		constexpr char XML_OPCODE_PARAM_TYPE[]{ "Type" };
		constexpr char XML_OPCODE_PARAM_DOMAIN[]{ "ArgDomain" };

		// let us hard code some opcodes for the mml converter
		// instead of making a messy config xml
		// TODO: Revisit if hacking the mScript opcodes becomes a reality
		
		constexpr byte MSCRIPT_OPCODE_SQ2_DETUNE{ 0xee };
		constexpr byte MSCRIPT_OPCODE_SQ_PITCH_EFFECT{ 0xef };
		constexpr byte MSCRIPT_OPCODE_SQ_ENVELOPE{ 0xf0 };
		constexpr byte MSCRIPT_OPCODE_VOLUME{ 0xf1 };
		constexpr byte MSCRIPT_OPCODE_SQ_CONTROL{ 0xf2 };
		constexpr byte MSCRIPT_OPCODE_SET_LENGTH{ 0xf3 };
		constexpr byte MSCRIPT_OPCODE_RESTART{ 0xf4 };
		constexpr byte MSCRIPT_OPCODE_RETURN{ 0xf5 };
		constexpr byte MSCRIPT_OPCODE_CHANNEL_TRANSPOSE{ 0xf6 };
		constexpr byte MSCRIPT_OPCODE_GLOBAL_TRANSPOSE{ 0xf7 };
		constexpr byte MSCRIPT_OPCODE_JSR{ 0xf8 };
		constexpr byte MSCRIPT_OPCODE_PUSHADDR{ 0xf9 };
		constexpr byte MSCRIPT_OPCODE_NOP{ 0xfa };
		constexpr byte MSCRIPT_OPCODE_LOOPIF{ 0xfb };
		constexpr byte MSCRIPT_OPCODE_END_LOOP{ 0xfc };
		constexpr byte MSCRIPT_OPCODE_BEGIN_LOOP{ 0xfd };
		constexpr byte MSCRIPT_OPCODE_POPADDR{ 0xfe };
		constexpr byte MSCRIPT_OPCODE_END{ 0xff };

		constexpr char SECTION_DEFINES[]{ "[defines]" };
		constexpr char SECTION_MSCRIPT[]{ "[mscript]" };

		constexpr byte HEX_REST{ 0x00 };
		constexpr byte HEX_NOTE_MIN{ 0x01 };
		constexpr byte HEX_NOTE_END{ 0x80 };
		constexpr byte HEX_NOTELENGTH_MIN{ 0x80 };
		constexpr byte HEX_NOTELENGTH_END{ 0xee };

		inline const std::vector<std::string> NOTE_NAMES{
			"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"
		};

		inline const std::vector<std::string> CHANNEL_LABELS{
			"sq1", "sq2", "tri", "noise" };

		inline const std::set<Fraction> ALLOWED_FRACTIONS = {
			// Power-of-two base lengths
			{1,1},   // whole
			{1,2},   // half
			{1,4},   // quarter
			{1,8},   // eighth
			{1,16},  // sixteenth
			{1,32},  // thirty-second

			// Triplets (common musical triplets)
			{1,3},   // quarter-triplet
			{1,6},   // eighth-triplet
			{1,12},  // sixteenth-triplet
			{1,24},  // thirty-second-triplet
			/*
			// Tuplet complements (used for scoring, not for emitting)
			{2,3},   // two-in-the-time-of-three (rare but helps Q scoring?)
			{4,3},   // four-in-the-time-of-three
			*/
			// Single-dotted (3/2N)
			{3,2},   // dotted whole
			{3,4},   // dotted half
			{3,8},   // dotted quarter
			{3,16},  // dotted eighth
			{3,32}   // dotted sixteenth
		};


	}
}

#endif
