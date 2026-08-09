#ifndef FM_MMLEVENT_H
#define FM_MMLEVENT_H

#include <optional>
#include <string>
#include <variant>
#include "Fraction.h"

namespace fm {

	struct NoteEvent {
		int pitch = 0; // semitone from C (0 = C, 1 = C#, ..., 11 = B)
		std::optional<int> length, raw;
		int dots;
		bool tie_to_next = false;
	};
	struct PercussionEvent {
		int perc_no = 0;
		int repeat = 0;
	};
	struct RestEvent {
		std::optional<int> length, raw;
		int dots;
	};
	struct LengthEvent {
		std::optional<int> length, raw;
		int dots;
	};
	struct TempoSetEvent {
		fm::Fraction tempo;
	};
	struct OctaveShiftEvent {
		int amount; // +1 or -1
	};
	struct OctaveSetEvent {
		int octave; // absolute octave
	};
	struct VolumeSetEvent {
		int volume; // 0 (max) - 15 (silent)
	};
	struct DirectiveEvent {
		std::string name;     // e.g. "tempo"
		std::string argument; // e.g. "150"
	};
	struct LabelEvent {
		std::string name;
	};
	struct IdentifierEvent {
		std::string name; // used for jsr, macros, inline asm, etc.
	};
	struct PushAddrEvent {};
	struct PopAddrEvent {};
	struct BeginLoopEvent {
		int iterations;
	};
	struct EndLoopEvent {};
	struct JSREvent {
		std::string label_name;
	};
	struct ReturnEvent {};
	struct StartEvent {};
	struct EndEvent {};
	struct RestartEvent {};
	struct NOPEvent {};
	struct LoopIfEvent {
		int iterations;
	};
	struct EnvelopeEvent {
		int value;
	};
	struct SongTransposeEvent {
		int semitones;
	};
	struct ChannelTransposeEvent {
		int semitones;
	};
	struct DetuneEvent {
		int value;
	};
	struct EffectEvent {
		int value;
	};
	struct PulseEvent {
		int duty_cycle, length_counter, constant_volume, volume_period;
	};


	using MmlEvent = std::variant<
		NoteEvent,
		PercussionEvent,
		RestEvent,
		LengthEvent,
		TempoSetEvent,
		OctaveShiftEvent,
		OctaveSetEvent,
		VolumeSetEvent,
		DirectiveEvent,
		LabelEvent,
		IdentifierEvent,
		PushAddrEvent,
		PopAddrEvent,
		BeginLoopEvent,
		LoopIfEvent,
		EndLoopEvent,
		JSREvent,
		ReturnEvent,
		StartEvent,
		EndEvent,
		RestartEvent,
		NOPEvent,
		EnvelopeEvent,
		SongTransposeEvent,
		ChannelTransposeEvent,
		DetuneEvent,
		PulseEvent,
		EffectEvent
	>;
}

#endif
