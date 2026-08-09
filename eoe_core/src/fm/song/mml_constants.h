#ifndef FM_MML_CONSTANTS_H
#define FM_MML_CONSTANTS_H

#include <map>
#include <string>
#include <vector>

namespace fm {

	enum class ChannelType { sq1, sq2, tri, noise };
	enum class MmlArgDomain { PulseDuty, PulseLen, PulseConstVol, SQEnvMode };

	namespace c {
		// number of hardware ticks per minute
		// used for tempo and note length calculations
		// yes, PAL is 3000 - but let's not worry about that for now
		constexpr int TICK_PER_MIN{ 3600 };

		constexpr char DIRECTIVE_SONG[]{ "#song" };
		// delimiter for raw tick lengths
		constexpr char RAW_DELIM{ '~' };

		inline const std::vector<std::string> CHANNEL_NAMES
		{ "#sq1", "#sq2", "#tri", "#noise" };
		inline const std::vector<fm::ChannelType> CHANNEL_TYPES
		{ fm::ChannelType::sq1, fm::ChannelType::sq2,
		fm::ChannelType::tri, fm::ChannelType::noise };

		constexpr char DIRECTIVE_SQ1[]{ "#sq1" };
		constexpr char DIRECTIVE_SQ2[]{ "#sq2" };
		constexpr char DIRECTIVE_TRIANGLE[]{ "#tri" };
		constexpr char DIRECTIVE_NOISE[]{ "#noise" };

		// in-song directives
		constexpr char DIRECTIVE_S_TITLE[]{ "#title" };
		constexpr char DIRECTIVE_S_TIMESIG[]{ "#time" };

		// in-channel directives
		constexpr char DIRECTIVE_CH_CLEF[]{ "#clef" };

		constexpr char OPCODE_START[]{ "start" };
		constexpr char OPCODE_END[]{ "end" };
		constexpr char OPCODE_RESTART[]{ "restart" };
		constexpr char OPCODE_NOP[]{ "nop" };
		constexpr char OPCODE_JSR[]{ "jsr" };
		constexpr char OPCODE_RETURN[]{ "return" };
		constexpr char OPCODE_LOOPIF[]{ "endloopif" };
		constexpr char OPCODE_ENVELOPE[]{ "envelope" };
		constexpr char OPCODE_DETUNE[]{ "detune" };
		constexpr char OPCODE_PULSE[]{ "pulse" };
		constexpr char OPCODE_EFFECT[]{ "effect" };

		constexpr char CONST_DUTY_12_5[]{ "DUTY_12_5" };
		constexpr char CONST_DUTY_25[]{ "DUTY_25" };
		constexpr char CONST_DUTY_50[]{ "DUTY_50" };
		constexpr char CONST_DUTY_75[]{ "DUTY_75" };

		constexpr char CONST_LEN_RUN[]{ "LEN_RUN" };
		constexpr char CONST_LEN_HALT[]{ "LEN_HALT" };

		constexpr char CONST_ENV_MODE[]{ "ENV_MODE" };
		constexpr char CONST_CONST_VOL[]{ "CONST_VOL" };

		constexpr char CONST_ENV_VOLUME_DECAY[]{ "ENV_VOLUME_DECAY" };
		constexpr char CONST_ENV_CURVE_HELD[]{ "ENV_CURVE_HELD" };
		constexpr char CONST_ENV_ATTACK_DECAY_CURVE[]{ "ENV_ATTACK_DECAY_CURVE" };

		inline const std::map<std::string, int> OPERAND_CONSTANTS{
			// sq pulse: duty cycle
			{CONST_DUTY_12_5, 0},
			{CONST_DUTY_25, 1},
			{CONST_DUTY_50, 2},
			{CONST_DUTY_75, 3},
			// sq pulse: length counter halt
			{CONST_LEN_RUN, 0},
			{CONST_LEN_HALT, 1},
			// sq pulse: const vol / envelope mode
			{CONST_ENV_MODE, 0},
			{CONST_CONST_VOL, 1},
			// sq envelope modes
			{CONST_ENV_VOLUME_DECAY, 0},
			{CONST_ENV_CURVE_HELD, 1},
			{CONST_ENV_ATTACK_DECAY_CURVE, 2}
		};

		inline const std::map<MmlArgDomain, std::map<int, std::string>> OPERAND_CONSTANT_BY_ARG_DOMAIN{
			{MmlArgDomain::PulseDuty, {
				{0, CONST_DUTY_12_5},
				{1, CONST_DUTY_25},
				{2, CONST_DUTY_50},
				{3, CONST_DUTY_75}
				}
			},
			{MmlArgDomain::PulseLen, {
				{0, CONST_LEN_RUN},
				{1, CONST_LEN_HALT}
				}
			},
			{MmlArgDomain::PulseConstVol, {
				{0, CONST_ENV_MODE},
				{1, CONST_CONST_VOL}
				}
			},
			{MmlArgDomain::SQEnvMode, {
				{0, CONST_ENV_VOLUME_DECAY},
				{1, CONST_ENV_CURVE_HELD},
				{2, CONST_ENV_ATTACK_DECAY_CURVE}
				}
			}
		};

	}

}

#endif
