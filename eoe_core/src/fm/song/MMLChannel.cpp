#include "MMLChannel.h"
#include "fm/fm_constants.h"
#include "fm/fm_util.h"
#include "mml_constants.h"
#include "LilyPond.h"
#include <cmath>
#include <format>
#include <numeric>
#include <stack>
#include <stdexcept>
#include <tuple>

using byte = unsigned char;

fm::MMLChannel::MMLChannel(fm::Fraction p_song_tempo) :
	song_tempo{ p_song_tempo }
{
	reset_vm();
}

int fm::MMLChannel::get_song_transpose(void) const {
	// return the first song transposition, if any
	for (const auto& ev : events) {
		if (std::holds_alternative<SongTransposeEvent>(ev)) {
			auto& ste = std::get<SongTransposeEvent>(ev);
			return ste.semitones;
		}
	}

	// no song transpositions
	return 0;
}

std::size_t fm::MMLChannel::get_start_index(void) const {
	// return the first start, if any (should be at most one)
	for (std::size_t i{ 0 }; i < events.size(); ++i) {
		if (std::holds_alternative<StartEvent>(events[i])) {
			return i;
		}
	}

	// no explicit start
	return 0;
}

std::size_t fm::MMLChannel::get_label_addr(const std::string& label) {
	for (std::size_t i{ 0 }; i < events.size(); ++i) {
		if (std::holds_alternative<LabelEvent>(events[i])) {
			auto& labe = std::get<LabelEvent>(events[i]);
			if (labe.name == label)
				return i;
		}
	}

	throw std::runtime_error(std::format("Could not find label with name {}", label));
}

void fm::MMLChannel::reset_vm(bool point_at_start) {
	vm.loop_count = 0;
	vm.loop_iters = 0;
	vm.jsr_addr.reset();
	vm.push_addr.reset();
	vm.loop_addr.reset();
	vm.loop_end_addr.reset();
	vm.st.octave = 4;
	vm.st.ticklength = 1;
	vm.st.default_length = { std::nullopt, 1, 0 };
	vm.st.pitchoffset = 0;
	vm.st.volume = 15;
	vm.st.sq_duty_cycle = 2;
	vm.tempo = song_tempo;

	if (point_at_start) {
		vm.pc = get_start_index();
	}
	else
		vm.pc = 0;
}

byte fm::MMLChannel::encode_percussion(int p_perc, int p_repeat) const {
	int l_repeat{ p_repeat == 256 ? 0 : p_repeat };

	byte result{ static_cast<byte>(p_perc * 16 + l_repeat) };
	if (result == c::HEX_REST || result >= c::HEX_NOTELENGTH_MIN)
		throw std::runtime_error("Percussion byte out of range");
	else
		return result;
}

void fm::MMLChannel::emit_set_length_bytecode_if_necessary(
	std::vector<fm::MusicInstruction>& instrs,
	const fm::Duration& p_duration,
	bool no_vm_advance) {
	int final_ticks = advance_vm_ticks(p_duration, no_vm_advance);

	if (final_ticks > 255)
		throw std::runtime_error("Tick-length exceeds 255");

	if (no_vm_advance || (final_ticks != vm.st.ticklength)) {
		vm.st.ticklength = final_ticks;

		constexpr byte HEX_NOTELENGTH_MAX{ c::HEX_NOTELENGTH_END - c::HEX_NOTELENGTH_MIN };

		if (final_ticks > 109) {
			fm::MusicInstruction sli{ c::MSCRIPT_OPCODE_SET_LENGTH,
			static_cast<byte>(final_ticks) };
			instrs.push_back(sli);
		}
		else {
			fm::MusicInstruction sli(c::HEX_NOTELENGTH_MIN + static_cast<byte>(final_ticks));
			instrs.push_back(sli);
		}
	}

}

int fm::MMLChannel::advance_vm_ticks(const fm::Duration& p_duration,
	bool no_vm_advance) {
	auto ticks{ tick_length(p_duration) };

	// we must avoid changing vm state here, but let us predict the actual length
	// of the next note or rest by making a copy of the accumulator and return
	// the expected length the next note/rest will get
	// to not waste one opcode
	if (no_vm_advance) {
		auto vm_ticks_copy{ vm.st.fraq };
		vm_ticks_copy += ticks.fraq;
		int carry = vm_ticks_copy.extract_whole();
		return ticks.whole + carry;
	}

	vm.st.fraq += ticks.fraq;
	int carry = vm.st.fraq.extract_whole();
	int final_ticks = ticks.whole + carry;

	return final_ticks;
}

// add set-length instructions if the input tick length differs from the previous
void fm::MMLChannel::emit_set_length_bytecode_if_necessary(
	std::vector<fm::MusicInstruction>& instrs,
	std::optional<int> length, int dots, std::optional<int> raw,
	bool no_vm_advance) {
	auto dur{ resolve_duration(length, dots, raw) };
	emit_set_length_bytecode_if_necessary(instrs, dur, no_vm_advance);
}

std::string fm::MMLChannel::volume_to_marker(int v) const {
	if (v >= 14) return "\\fff";
	if (v >= 12) return "\\ff";
	if (v >= 10) return "\\f";
	if (v >= 8)  return "\\mf";
	if (v >= 6)  return "\\mp";
	if (v >= 4)  return "\\p";
	if (v >= 2)  return "\\pp";
	return "\\ppp";
}

bool fm::MMLChannel::is_empty(void) const {
	// start and end
	return events.size() <= 2;
}

fm::ChannelBytecodeExport fm::MMLChannel::to_bytecode(void) {
	std::vector<fm::MusicInstruction> instrs;

	// label to instruction index the label points to
	std::map<std::string, std::size_t> label_idx;
	// label to all instr indexes jumping to it
	std::map<std::string, std::vector<std::size_t>> jump_targets;

	reset_vm();

	for (; vm.pc < events.size(); ++vm.pc) {
		const auto& ev{ events[vm.pc] };

		// musical note
		if (std::holds_alternative<NoteEvent>(ev)) {
			auto tickresult{ resolve_tie_chain() };

			emit_set_length_bytecode_if_necessary(instrs,
				tickresult.total);
			byte note_byte{ static_cast<byte>(note_index(vm.st.octave,
				tickresult.pitch)) };

			fm::MusicInstruction notei(note_byte);
			instrs.push_back(notei);
		}
		// percussion note
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& perce = std::get<PercussionEvent>(ev);

			// check the tick length and emit length event if needed
			emit_set_length_bytecode_if_necessary(instrs);

			fm::MusicInstruction perci(encode_percussion(perce.perc_no, perce.repeat));
			instrs.push_back(perci);
		}
		// rest
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& reste{ std::get<RestEvent>(ev) };

			emit_set_length_bytecode_if_necessary(instrs, reste.length, reste.dots, reste.raw);
			fm::MusicInstruction resti(c::HEX_REST);
			instrs.push_back(resti);
		}
		// length - an abstraction that also exists in Faxanadu's music engine
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& lene{ std::get<LengthEvent>(ev) };
			if (lene.raw.has_value()) {
				vm.st.default_length = { std::nullopt, lene.raw.value(), 0 };
			}
			else if (lene.length.has_value()) {
				vm.st.default_length = { lene.length.value(), std::nullopt, lene.dots };
			}
			else
				throw std::runtime_error("Malformed length event");

			emit_set_length_bytecode_if_necessary(instrs, lene.length, lene.dots, lene.raw, true);
		}
		// music opcodes
		else if (std::holds_alternative<SongTransposeEvent>(ev)) {
			auto& ste{ std::get<SongTransposeEvent>(ev) };
			fm::MusicInstruction sti(c::MSCRIPT_OPCODE_GLOBAL_TRANSPOSE,
				int_to_byte(ste.semitones));
			instrs.push_back(sti);
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte{ std::get<ChannelTransposeEvent>(ev) };
			fm::MusicInstruction cti(c::MSCRIPT_OPCODE_CHANNEL_TRANSPOSE,
				int_to_byte(cte.semitones));
			instrs.push_back(cti);
		}
		else if (std::holds_alternative<DetuneEvent>(ev)) {
			auto& dete{ std::get<DetuneEvent>(ev) };
			fm::MusicInstruction deti(c::MSCRIPT_OPCODE_SQ2_DETUNE, int_to_byte(dete.value));
			instrs.push_back(deti);
		}
		else if (std::holds_alternative<EffectEvent>(ev)) {
			auto& effe{ std::get<EffectEvent>(ev) };
			fm::MusicInstruction effi(c::MSCRIPT_OPCODE_SQ_PITCH_EFFECT,
				int_to_byte(effe.value));
			instrs.push_back(effi);
		}
		else if (std::holds_alternative<EnvelopeEvent>(ev)) {
			auto& enve{ std::get<EnvelopeEvent>(ev) };
			fm::MusicInstruction envi(c::MSCRIPT_OPCODE_SQ_ENVELOPE,
				int_to_byte(enve.value));
			instrs.push_back(envi);
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev)) {
			auto& vse{ std::get<VolumeSetEvent>(ev) };
			fm::MusicInstruction vsi(c::MSCRIPT_OPCODE_VOLUME,
				int_to_volume_byte(vse.volume));
			instrs.push_back(vsi);
		}
		else if (std::holds_alternative<PulseEvent>(ev)) {
			auto& pulsee{ std::get<PulseEvent>(ev) };
			fm::MusicInstruction pulsei(c::MSCRIPT_OPCODE_SQ_CONTROL,
				ints_to_sq_control(pulsee.duty_cycle, pulsee.length_counter,
					pulsee.constant_volume, pulsee.volume_period));
			instrs.push_back(pulsei);
		}
		// control flow
		else if (std::holds_alternative<LabelEvent>(ev)) {
			auto& labe = std::get<LabelEvent>(ev);
			if (label_idx.contains(labe.name))
				throw std::runtime_error(std::format("Label redefinition: {}", labe.name));
			else
				label_idx.insert(std::make_pair(labe.name, instrs.size()));
		}
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& jsre = std::get<JSREvent>(ev);
			jump_targets[jsre.label_name].push_back(instrs.size());
			// will add jump target at the end
			fm::MusicInstruction jsri(c::MSCRIPT_OPCODE_JSR);
			instrs.push_back(jsri);
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			fm::MusicInstruction reti(c::MSCRIPT_OPCODE_RETURN);
			instrs.push_back(reti);
		}
		else if (std::holds_alternative<RestartEvent>(ev)) {
			fm::MusicInstruction resti(c::MSCRIPT_OPCODE_RESTART);
			instrs.push_back(resti);
		}
		else if (std::holds_alternative<NOPEvent>(ev)) {
			fm::MusicInstruction nopi(c::MSCRIPT_OPCODE_NOP);
			instrs.push_back(nopi);
		}
		else if (std::holds_alternative<EndEvent>(ev)) {
			fm::MusicInstruction endi(c::MSCRIPT_OPCODE_END);
			instrs.push_back(endi);
		}
		// loops
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			fm::MusicInstruction pushaddri(c::MSCRIPT_OPCODE_PUSHADDR);
			instrs.push_back(pushaddri);
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			fm::MusicInstruction popaddri(c::MSCRIPT_OPCODE_POPADDR);
			instrs.push_back(popaddri);
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& bloope = std::get<BeginLoopEvent>(ev);
			fm::MusicInstruction bloopi(c::MSCRIPT_OPCODE_BEGIN_LOOP, static_cast<byte>(bloope.iterations));
			instrs.push_back(bloopi);
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			fm::MusicInstruction lii(c::MSCRIPT_OPCODE_LOOPIF,
				static_cast<byte>(lie.iterations));
			instrs.push_back(lii);
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			fm::MusicInstruction eloopi(c::MSCRIPT_OPCODE_END_LOOP);
			instrs.push_back(eloopi);
		}
		// mml abstractions
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& ose = std::get<OctaveSetEvent>(ev);
			vm.st.octave = ose.octave;
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& oshe = std::get<OctaveShiftEvent>(ev);
			vm.st.octave += oshe.amount;
		}
		else if (std::holds_alternative<TempoSetEvent>(ev)) {
			auto& tse = std::get<TempoSetEvent>(ev);
			vm.tempo = tse.tempo;
		}

	}

	// patch jumps - jumps to label @label get the instr idx of @label as target
	for (const auto& kv : label_idx)
		for (const auto& n : jump_targets[kv.first])
			instrs[n].jump_target = kv.second;

	return fm::ChannelBytecodeExport{ instrs, get_start_index() };
}

fm::TickResult fm::MMLChannel::tick_length(const fm::Duration& dur) const {
	// RAW TICKS: bypass musical math entirely
	if (dur.is_raw()) {
		int ticks = dur.raw.value();

		if (ticks < 1 || ticks > 255)
			throw std::runtime_error(
				std::format("Tick count out of bounds ({})", ticks));

		return fm::TickResult{
			ticks,
			Fraction(0, 1)   // no fractional leftover
		};
	}

	// MUSICAL DURATION
	Fraction qnote = Fraction(c::TICK_PER_MIN * vm.tempo.get_den(), vm.tempo.get_num());
	Fraction total = qnote * (dur.musical * Fraction(4, 1));

	int whole = total.extract_whole();

	if (whole < 1 || whole > 255)
		throw std::runtime_error(
			std::format("Tick count out of bounds ({})", whole));

	return fm::TickResult{
		whole,
		total   // leftover fractional ticks
	};
}


int fm::MMLChannel::note_index(int octave, int pitch) const {
	int absnote = octave * 12 + pitch;

	if (absnote < 1 || absnote >= 0x80)
		throw std::runtime_error(
			std::format("Note index out of range ({})", absnote));

	return absnote - 23; // because C2 (24) should map to 1
}

std::pair<int, int> fm::MMLChannel::split_note(byte p_note_no) const {
	int note_no{ static_cast<int>(p_note_no) - 1 };
	int octave{ 2 + (note_no / 12) };
	int pitch{ note_no % 12 };

	return std::make_pair(pitch, octave);
}

std::string fm::MMLChannel::note_index_to_str(int p_idx) const {
	const std::vector<std::string> pitchnames{
		"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"
	};

	int absidx{ p_idx + 23 };

	return std::format("{}{}", pitchnames[absidx % 12], absidx / 12);
}

std::string fm::MMLChannel::note_no_to_str(int p_note_no) const {
	const std::vector<std::string> pitchnames{
	"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"
	};

	if (p_note_no < 0 || p_note_no >= 12)
		throw std::runtime_error(
			std::format("Invalid note number: {} (values must be in the range 0-11)",
				p_note_no));
	else
		return pitchnames[static_cast<std::size_t>(p_note_no)];
}

fm::Duration fm::MMLChannel::resolve_duration(std::optional<int> length,
	int dots, std::optional<int> raw) const {
	if (raw)
		return fm::Duration::from_raw_ticks(raw.value());
	else if (length.has_value() || vm.st.default_length.length.has_value()) {
		// add the default length-dots if the note did not have an explicit musical length
		int base = length.has_value() ? length.value() : vm.st.default_length.length.value();
		int final_dots{ length ? dots : dots + vm.st.default_length.dots };
		return Duration::from_length_and_dots(base, final_dots);
	}
	else if (vm.st.default_length.raw.has_value())
		return fm::Duration::from_raw_ticks(vm.st.default_length.raw.value());
	else
		throw std::runtime_error("No note-length available");
}

fm::TieResult fm::MMLChannel::resolve_tie_chain(void) {
	NoteEvent neve{ std::get<NoteEvent>(events.at(vm.pc)) };

	int pitch = neve.pitch;
	Duration total{ resolve_duration(neve.length, neve.dots, neve.raw) };

	// single note - allow raw ticks
	if (!neve.tie_to_next)
		return fm::TieResult(pitch, total);

	while (neve.tie_to_next) {
		++vm.pc;

		if (vm.pc >= events.size() ||
			!std::holds_alternative<NoteEvent>(events.at(vm.pc)))
			throw std::runtime_error("Note tied to non-note");

		neve = std::get<NoteEvent>(events.at(vm.pc));

		if (pitch != neve.pitch)
			throw std::runtime_error("Tied notes have different pitches");
		else if (neve.raw.has_value())
			throw std::runtime_error("Tied note has raw length - not allowed");

		total = total + resolve_duration(neve.length, neve.dots, neve.raw);
	}

	return { pitch, total };
}

byte fm::MMLChannel::ints_to_sq_control(int duty, int envLoop,
	int constVol, int volume) const {
	return static_cast<byte>((duty << 6) | (envLoop << 5) | (constVol << 4) | (volume & 0x0F));
}

byte fm::MMLChannel::int_to_volume_byte(int p_volume) const {
	if (p_volume >= 15)
		return static_cast<byte>(0);
	else if (p_volume < 0)
		return static_cast<byte>(15);
	else
		return static_cast<byte>(15 - p_volume);
}

byte fm::MMLChannel::int_to_byte(int n) const {
	char r{ static_cast<char>(n) };
	return static_cast<byte>(r);
}

int fm::MMLChannel::byte_to_int(byte b) const {
	char r{ static_cast<char>(b) };
	return static_cast<int>(r);
}

int fm::MMLChannel::byte_to_volume(byte b) const {
	if (b >= 15)
		return 0;
	else
		return static_cast<int>(15 - b);
}

std::string fm::MMLChannel::to_string(void) const {
	std::string result{ std::format("{} {{\n", channel_type_to_string()) };

	// pretty-printing helper
	bool last_was_newline{ true };
	auto emit = [&](const std::string& s) {
		if (s.empty())
			return;

		std::size_t i{ 0 };

		// Handle a *leading* newline specially (to avoid duplicates)
		if (s[0] == '\n') {
			if (!last_was_newline) {
				result.push_back('\n');
				last_was_newline = true;
			}
			// Skip this leading newline in the rest of the processing
			i = 1;
		}

		// Emit the remainder (if any)
		if (i < s.size()) {
			result.append(s.substr(i));
			// Now update the flag based on the *last emitted character*
			last_was_newline = (s.back() == '\n');
		}
		};


	std::optional<int> loopcnt{ std::nullopt };

	for (const auto& ev : events) {

		if (std::holds_alternative<NoteEvent>(ev)) {
			auto& note = std::get<NoteEvent>(ev);

			emit(note_no_to_str(note.pitch));
			if (note.raw.has_value())
				emit(std::format("{}{}", c::RAW_DELIM, note.raw.value()));
			else {
				if (note.length.has_value()) {
					emit(std::format("{}", note.length.value()));
				}

				for (int i{ 0 }; i < note.dots; ++i)
					emit(".");
			}
			emit(note.tie_to_next ? "&" : " ");
		}
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& pee = std::get<PercussionEvent>(ev);

			emit(std::format("p{}", pee.perc_no));
			if (pee.repeat != 1)
				emit(std::format("*{}", pee.repeat));

			emit(" ");
		}
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& r = std::get<RestEvent>(ev);

			emit("r");

			if (r.raw.has_value())
				emit(std::format("{}{}", c::RAW_DELIM, r.raw.value()));
			else {
				if (r.length.has_value()) {
					emit(std::format("{}", r.length.value()));
				}

				for (int i{ 0 }; i < r.dots; ++i)
					emit(".");
			}
			emit(" ");
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& os = std::get<OctaveShiftEvent>(ev);
			if (os.amount > 0)
				emit("> ");
			else
				emit("< ");
		}
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& os = std::get<OctaveSetEvent>(ev);
			emit(std::format("o{} ", os.octave));
		}
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& le = std::get<LengthEvent>(ev);

			if (le.raw.has_value()) {
				emit(std::format("l{}{}", c::RAW_DELIM, le.raw.value()));
			}
			else if (le.length.has_value()) {
				emit(std::format("l{}", le.length.value()));
				for (int i{ 0 }; i < le.dots; ++i)
					emit(".");
			}

			emit(" ");
		}
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			emit("[");
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			emit("]");
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& bl = std::get<BeginLoopEvent>(ev);
			if (loopcnt.has_value())
				throw std::runtime_error("Begin-Loop event starts before the previous loop has ended");
			loopcnt = bl.iterations;
			emit("[");
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			if (!loopcnt.has_value())
				throw std::runtime_error("End-Loop event does not have a matching Begin-Loop event");

			emit(std::format("]{} ", loopcnt.value()));
			loopcnt.reset();
		}
		else if (std::holds_alternative<StartEvent>(ev))
			emit(std::format("\n!{}\n", c::OPCODE_START));
		else if (std::holds_alternative<EndEvent>(ev))
			emit(std::format("\n!{}\n", c::OPCODE_END));
		else if (std::holds_alternative<RestartEvent>(ev))
			emit(std::format("\n!{}\n", c::OPCODE_RESTART));
		else if (std::holds_alternative<SongTransposeEvent>(ev)) {
			auto& ste = std::get<SongTransposeEvent>(ev);
			emit(std::format("S_{} ", ste.semitones));
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev)) {
			auto& vse = std::get<VolumeSetEvent>(ev);
			emit(std::format("v{} ", vse.volume));
		}
		else if (std::holds_alternative<PulseEvent>(ev)) {
			auto& pe = std::get<PulseEvent>(ev);
			emit(std::format("\n!{} {} {} {} {}\n", c::OPCODE_PULSE,
				fm::util::mml_arg_to_string(fm::MmlArgDomain::PulseDuty, pe.duty_cycle),
				fm::util::mml_arg_to_string(fm::MmlArgDomain::PulseLen, pe.length_counter),
				fm::util::mml_arg_to_string(fm::MmlArgDomain::PulseConstVol, pe.constant_volume),
				pe.volume_period
			));
		}
		else if (std::holds_alternative<EnvelopeEvent>(ev)) {
			auto& enve = std::get<EnvelopeEvent>(ev);
			emit(std::format("\n!{} {}\n", c::OPCODE_ENVELOPE,
				fm::util::mml_arg_to_string(fm::MmlArgDomain::SQEnvMode, enve.value))
			);
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte = std::get<ChannelTransposeEvent>(ev);
			emit(std::format("_{} ", cte.semitones));
		}
		else if (std::holds_alternative<DetuneEvent>(ev)) {
			auto& dte = std::get<DetuneEvent>(ev);
			emit(std::format("\n!{} {}\n", c::OPCODE_DETUNE, dte.value));
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			emit(std::format("\n!{} {}\n", c::OPCODE_LOOPIF,
				lie.iterations));
		}
		else if (std::holds_alternative<EffectEvent>(ev)) {
			auto& effe = std::get<EffectEvent>(ev);
			emit(std::format("\n!{} {}\n", c::OPCODE_EFFECT,
				effe.value));
		}
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& jsre = std::get<JSREvent>(ev);
			emit(std::format("\n!{} {}\n", c::OPCODE_JSR,
				jsre.label_name));
		}
		else if (std::holds_alternative<LabelEvent>(ev)) {
			auto& labe = std::get<LabelEvent>(ev);
			emit(std::format("\n{}:\n", labe.name));
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			emit(std::format("\n!{}\n", c::OPCODE_RETURN));
		}
		else if (std::holds_alternative<NOPEvent>(ev)) {
			emit(std::format("\n!{}\n", c::OPCODE_NOP));
		}
		else if (std::holds_alternative<TempoSetEvent>(ev)) {
			const auto& tse = std::get<TempoSetEvent>(ev);
			emit(std::format("t{} ", tse.tempo.to_tempo_string()));
		}
		else {
			std::visit([&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				throw std::runtime_error(std::format("Unhandled event type {}", typeid(T).name()));
				},
				ev);
		}
	}

	emit("\n}");

	return result;
}

std::string fm::MMLChannel::channel_type_to_string(void) const {
	if (channel_type == fm::ChannelType::sq1)
		return fm::c::CHANNEL_NAMES[0];
	else if (channel_type == fm::ChannelType::sq2)
		return fm::c::CHANNEL_NAMES[1];
	else if (channel_type == fm::ChannelType::tri)
		return fm::c::CHANNEL_NAMES[2];
	else
		return fm::c::CHANNEL_NAMES[3];
}

bool fm::MMLChannel::is_square_channel(void) const {
	return channel_type == fm::ChannelType::sq1 ||
		channel_type == fm::ChannelType::sq2;
}

// ROM bytecode to MML
void fm::MMLChannel::parse_bytecode(const std::vector<fm::MusicInstruction>& instrs,
	std::size_t entrypoint_idx, std::set<std::size_t> jump_targets) {
	const auto NLENGTHS{ calc_tick_lengths() };
	int octave{ 4 };

	const auto reset = [&octave, this]() -> void {
		if (channel_type != fm::ChannelType::noise)
			this->events.push_back(OctaveSetEvent{ octave });
		};

	for (std::size_t i{ 0 }; i < instrs.size(); ++i) {
		if (i == entrypoint_idx) {
			events.push_back(StartEvent{});
			reset();
		}
		if (jump_targets.contains(i)) {
			events.push_back(LabelEvent{ std::format("@label_{}", i) });
			reset();
		}

		const auto& instr{ instrs[i] };
		byte ob{ instr.opcode_byte };
		// length-setting byte
		if (ob >= c::HEX_NOTELENGTH_MIN &&
			ob < c::HEX_NOTELENGTH_END) {
			const auto& leninfo{ (NLENGTHS.at(static_cast<std::size_t>(ob - c::HEX_NOTELENGTH_MIN))) };

			events.push_back(LengthEvent(leninfo.length, leninfo.raw, leninfo.dots));
		}
		// length-setting opcode
		else if (ob == c::MSCRIPT_OPCODE_SET_LENGTH) {
			const auto& leninfo{ (NLENGTHS.at(static_cast<std::size_t>(instr.operand.value()))) };
			events.push_back(LengthEvent(leninfo.length, leninfo.raw, leninfo.dots));
		}
		// rest
		else if (ob == c::HEX_REST) {
			events.push_back(RestEvent{});
		}
		// note
		else if (ob > c::HEX_REST && ob < c::HEX_NOTELENGTH_MIN) {
			// notes for musical channels
			if (channel_type != fm::ChannelType::noise) {

				auto snote{ split_note(ob) };
				if (snote.second != octave) {
					octave = snote.second;
					events.push_back(OctaveSetEvent{ octave });
				}
				events.push_back(NoteEvent{ snote.first });
			}
			// percussion
			else {
				int ptype{ static_cast<int>(ob / 16) };
				int prep{ static_cast<int>(ob % 16) };

				if (prep == 0)
					prep = 256;

				events.push_back(PercussionEvent{ ptype, prep });
			}
		}
		// begin loop
		else if (ob == c::MSCRIPT_OPCODE_BEGIN_LOOP) {
			events.push_back(BeginLoopEvent{ static_cast<int>(instr.operand.value()) });
			reset();
		}
		// end loop
		else if (ob == c::MSCRIPT_OPCODE_END_LOOP) {
			events.push_back(EndLoopEvent{});
			reset();
		} // song transpose
		else if (ob == c::MSCRIPT_OPCODE_GLOBAL_TRANSPOSE) {
			events.push_back(SongTransposeEvent{ byte_to_int(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_CHANNEL_TRANSPOSE) {
			events.push_back(ChannelTransposeEvent{ byte_to_int(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_VOLUME) {
			events.push_back(VolumeSetEvent{ byte_to_volume(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ_CONTROL) {
			int ctrl{ static_cast<int>(instr.operand.value()) };
			events.push_back(PulseEvent{
				(ctrl >> 6) & 3,
				(ctrl >> 5) & 1,
				(ctrl >> 4) & 1,
				ctrl & 0x0f
				});
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ_ENVELOPE) {
			events.push_back(EnvelopeEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_LOOPIF) {
			events.push_back(LoopIfEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ2_DETUNE) {
			events.push_back(DetuneEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_END) {
			events.push_back(EndEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_SQ_PITCH_EFFECT) {
			events.push_back(EffectEvent{ static_cast<int>(instr.operand.value()) });
		}
		else if (ob == c::MSCRIPT_OPCODE_RESTART) {
			events.push_back(RestartEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_RETURN) {
			events.push_back(ReturnEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_PUSHADDR) {
			events.push_back(PushAddrEvent{});
			reset();
		}
		else if (ob == c::MSCRIPT_OPCODE_POPADDR) {
			events.push_back(PopAddrEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_NOP) {
			events.push_back(NOPEvent{});
		}
		else if (ob == c::MSCRIPT_OPCODE_JSR) {
			events.push_back(JSREvent{ std::format("@label_{}", instr.jump_target.value()) });
			reset();
		}
		else
			throw std::runtime_error(std::format("Unhandled opcode {:02x}", ob));
	}

}

// calculate all mappings from byte vals 0-255 to a length string
// we don't try to calculate tempo changes in the bytecode songs
// so this will be fixed
std::vector<fm::DefaultLength> fm::MMLChannel::calc_tick_lengths(void) const {
	std::vector<fm::DefaultLength> result;
	for (int i{ 0 }; i < 256; ++i)
		result.push_back(fm::DefaultLength(std::nullopt, i, 0));

	int qnote_ticks = (c::TICK_PER_MIN * song_tempo.get_den()) / song_tempo.get_num();
	int whole_note_ticks = 4 * qnote_ticks;

	for (int i{ 0 }; i < 256; ++i) {
		Fraction r(i, whole_note_ticks);

		if (c::ALLOWED_FRACTIONS.contains(r)) {

			// Base: 1/N
			if (r.get_num() == 1) {
				result[i] = fm::DefaultLength(r.get_den(), std::nullopt, 0);
			}

			// Dotted: 3/(2N)
			else if (r.get_num() == 3 && (r.get_den() % 2 == 0)) {
				result[i] = fm::DefaultLength(r.get_den() / 2, std::nullopt, 1);
			}

			// Dotted: 1/(3N)
			else if (r.get_num() == 1 && (r.get_den() % 3 == 0)) {
				result[i] = fm::DefaultLength(r.get_den() / 3, std::nullopt, 0);
			}
		}
	}

	return result;
}

int fm::MMLChannel::add_midi_track(smf::MidiFile& p_midi, int p_channel_no,
	int p_pitch_offset, int p_max_ticks) {
	const std::vector<int> duty_cycle_to_instr{ 80, 80, 81, 62, 64, 64, 64, 64 };
	const std::vector<int> perc_note_no{ 0, 36, 38, 42, 0, 70, 70, 0 };
	bool is_perc{ channel_type == fm::ChannelType::noise };

	p_midi.addTrack();
	int l_track_no{ p_midi.getTrackCount() - 1 };
	// int l_channel_no{ is_perc ? 9 : 0 };

	std::set<VMState> vmstates;

	int default_midi_volume{ 100 };
	if (channel_type == fm::ChannelType::tri)
		default_midi_volume = 85;
	else if (channel_type == fm::ChannelType::noise)
		default_midi_volume = 90;

	p_midi.addController(l_track_no, 0, p_channel_no, 7,
		default_midi_volume
	);

	p_midi.addTimbre(l_track_no, 0, p_channel_no,
		channel_type == fm::ChannelType::tri ? 33 :
		duty_cycle_to_instr.at(vm.st.sq_duty_cycle)
	);

	int ticks{ 1 };
	reset_vm(true);

	// TODO: Find a better solution
	int guard_count{ 0 };

	while (vm.pc < events.size()) {
		++guard_count;
		if (guard_count > 20000)
			break;

		VMState state{ vm.pc,
		vm.loop_count, vm.loop_iters, vm.st.pitchoffset, vm.st.volume, vm.st.sq_duty_cycle,
		vm.push_addr, vm.jsr_addr, vm.loop_addr, vm.loop_end_addr };

		if (vmstates.contains(state))
			break;
		else
			vmstates.insert(state);

		const auto& ev{ events[vm.pc] };

		if (std::holds_alternative<NoteEvent>(ev)) {
			auto tickresult{ resolve_tie_chain() };
			int int_ticks{ advance_vm_ticks(tickresult.total) };

			p_midi.addNoteOn(l_track_no, ticks, p_channel_no,
				12 * (vm.st.octave + 1) + tickresult.pitch + p_pitch_offset + vm.st.pitchoffset, (default_midi_volume * vm.st.volume) / 15);
			ticks += int_ticks;
			p_midi.addNoteOff(l_track_no, ticks, p_channel_no,
				12 * (vm.st.octave + 1) + tickresult.pitch + p_pitch_offset + vm.st.pitchoffset, 0);
		}
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& perce = std::get<PercussionEvent>(ev);
			int int_ticks{ advance_vm_ticks(
				resolve_duration(vm.st.default_length.length,
				vm.st.default_length.dots, vm.st.default_length.raw))
			};

			bool l_end_channel{ false };

			for (int rep{ 0 }; rep < perce.repeat; ++rep) {
				p_midi.addNoteOn(l_track_no, ticks, p_channel_no,
					perc_note_no.at(perce.perc_no), default_midi_volume);

				ticks += int_ticks;

				p_midi.addNoteOff(l_track_no, ticks, p_channel_no,
					perc_note_no.at(perce.perc_no), 0);

				if (ticks > p_max_ticks) {
					l_end_channel = true;
					break;
				}
			}

			if (l_end_channel)
				break;
		}
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& le = std::get<LengthEvent>(ev);

			if (le.raw.has_value()) {
				vm.st.default_length = { std::nullopt, le.raw.value(), 0 };
			}
			else if (le.length.has_value()) {
				vm.st.default_length = { le.length.value(), std::nullopt, le.dots };
			}
			else
				throw std::runtime_error("Malformed length event");
		}
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& re = std::get<RestEvent>(ev);
			int int_ticks{ advance_vm_ticks(resolve_duration(re.length, re.dots, re.raw)) };
			ticks += int_ticks;
		}
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& se = std::get<OctaveSetEvent>(ev);
			vm.st.octave = se.octave;
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& ssh = std::get<OctaveShiftEvent>(ev);
			vm.st.octave += ssh.amount;
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte = std::get<ChannelTransposeEvent>(ev);
			vm.st.pitchoffset = cte.semitones;
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev) &&
			is_square_channel()) {
			auto& vse = std::get<VolumeSetEvent>(ev);
			vm.st.volume = vse.volume;
		}
		else if (std::holds_alternative<PulseEvent>(ev) &&
			is_square_channel()) {
			auto& pe = std::get<PulseEvent>(ev);
			vm.st.sq_duty_cycle = pe.duty_cycle;
			p_midi.addTimbre(l_track_no, ticks, 0, duty_cycle_to_instr.at(vm.st.sq_duty_cycle));
		}
		else if (std::holds_alternative<TempoSetEvent>(ev)) {
			const auto& tse = std::get<TempoSetEvent>(ev);
			vm.tempo = tse.tempo;
		}

		// advance the VM depending on opcode
		if (std::holds_alternative<EndEvent>(ev) ||
			std::holds_alternative<RestartEvent>(ev))
			break;
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& labe = std::get<JSREvent>(ev);
			if (!vm.jsr_addr.has_value())
				vm.jsr_addr = vm.pc;
			else
				throw std::runtime_error("Invoking JSR with JSR-address already on the stack");
			vm.pc = get_label_addr(labe.label_name);
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			vm.pc = vm.jsr_addr.value() + 1;
			vm.jsr_addr.reset();
		}
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			vm.push_addr = vm.pc++;
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			vm.pc = vm.push_addr.value() + 1;
			//vm.push_addr.reset();
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& ble = std::get<BeginLoopEvent>(ev);
			vm.loop_addr = vm.pc++;
			vm.loop_iters = ble.iterations;
			vm.loop_count = 0;
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			auto& ele = std::get<EndLoopEvent>(ev);
			vm.loop_end_addr = vm.pc;
			vm.loop_count++;
			if (vm.loop_count < vm.loop_iters)
				vm.pc = vm.loop_addr.value() + 1;
			else
				++vm.pc;
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			if (vm.loop_count >= lie.iterations)
				vm.pc = vm.loop_end_addr.value() + 1;
			else
				++vm.pc;
		}
		else
			++vm.pc;
	}

	return ticks;
}

int fm::MMLChannel::add_lilypond_staff(std::string& p_lp, int p_pitch_offset,
	const std::string& p_time_sig) {
	std::string l_stafftype{ channel_type == fm::ChannelType::noise ?
	"DrumStaff \\drummode" : "Staff" };

	p_lp += std::format("\\new {} {{\n\\tempo 4 = {}\n\\time {}\n",
		l_stafftype,
		// channel_type_to_string(),
		std::round(song_tempo.to_double()),
		p_time_sig);

	if (channel_type == fm::ChannelType::tri) {
		p_lp += "\\set Staff.midiMinimumVolume  = #0.10\n"
			"\\set Staff.midiMaximumVolume = #0.80\n"
			"\\set Staff.midiInstrument = \"synth bass 1\"\n";
	}
	else if (channel_type == fm::ChannelType::noise) {
		p_lp += "\\set Staff.midiMinimumVolume  = #0.10\n"
			"\\set Staff.midiMaximumVolume = #0.80\n";
	}
	else {
		p_lp += "\\set Staff.midiMinimumVolume  = #0.20\n"
			"\\set Staff.midiMaximumVolume = #0.90";
		p_lp += fm::lp::emit_midi_instrument(vm.st.sq_duty_cycle);
	};

	if (channel_type != fm::ChannelType::noise) {
		p_lp += get_lilypond_clef();
	}

	std::set<VMState> vmstates;
	fm::Fraction bar_accum{ 0,1 };

	reset_vm(true);

	// TODO: Find a better solution
	int guard_count{ 0 };

	while (vm.pc < events.size()) {
		++guard_count;
		if (guard_count > 20000)
			break;

		VMState state{ vm.pc,
		vm.loop_count, vm.loop_iters, vm.st.pitchoffset, vm.st.volume, vm.st.sq_duty_cycle,
		vm.push_addr, vm.jsr_addr, vm.loop_addr, vm.loop_end_addr };

		if (vmstates.contains(state))
			break;
		else
			vmstates.insert(state);

		const auto& ev{ events[vm.pc] };

		if (std::holds_alternative<NoteEvent>(ev)) {
			const auto& notee = std::get<NoteEvent>(ev);

			p_lp += " " + fm::lp::emit_note(notee.pitch + p_pitch_offset + vm.st.pitchoffset, vm.st.octave,
				to_lilypond_length(notee.length, notee.dots, notee.raw, bar_accum));

			if (notee.tie_to_next)
				p_lp += " ~ ";
		}
		else if (std::holds_alternative<PercussionEvent>(ev)) {
			auto& perce = std::get<PercussionEvent>(ev);

			for (int rep{ 0 }; rep < perce.repeat; ++rep)
				p_lp += " " + fm::lp::emit_percussion(perce.perc_no,
					to_lilypond_length(vm.st.default_length.length,
						vm.st.default_length.dots,
						vm.st.default_length.raw, bar_accum));
		}
		else if (std::holds_alternative<LengthEvent>(ev)) {
			auto& le = std::get<LengthEvent>(ev);

			if (le.raw.has_value()) {
				vm.st.default_length = { std::nullopt, le.raw.value(), 0 };
			}
			else if (le.length.has_value()) {
				vm.st.default_length = { le.length.value(), std::nullopt, le.dots };
			}
			else
				throw std::runtime_error("Malformed length event");
		}
		else if (std::holds_alternative<RestEvent>(ev)) {
			auto& re = std::get<RestEvent>(ev);
			p_lp += " " + fm::lp::emit_rest(to_lilypond_length(re.length, re.dots, re.raw, bar_accum));
		}
		else if (std::holds_alternative<OctaveSetEvent>(ev)) {
			auto& se = std::get<OctaveSetEvent>(ev);
			vm.st.octave = se.octave;
		}
		else if (std::holds_alternative<OctaveShiftEvent>(ev)) {
			auto& ssh = std::get<OctaveShiftEvent>(ev);
			vm.st.octave += ssh.amount;
		}
		else if (std::holds_alternative<ChannelTransposeEvent>(ev)) {
			auto& cte = std::get<ChannelTransposeEvent>(ev);
			vm.st.pitchoffset = cte.semitones;
		}
		else if (std::holds_alternative<VolumeSetEvent>(ev) &&
			is_square_channel()) {
			auto& vse = std::get<VolumeSetEvent>(ev);
			vm.st.volume = vse.volume;
			state.volume = vm.st.volume;
		}
		else if (std::holds_alternative<PulseEvent>(ev) &&
			is_square_channel()) {
			auto& pe = std::get<PulseEvent>(ev);

			if (vm.st.sq_duty_cycle != pe.duty_cycle) {
				vm.st.sq_duty_cycle = pe.duty_cycle;
				p_lp += fm::lp::emit_midi_instrument(vm.st.sq_duty_cycle);
			}
		}
		else if (std::holds_alternative<TempoSetEvent>(ev)) {
			const auto& tse = std::get<TempoSetEvent>(ev);
			vm.tempo = tse.tempo;
			p_lp += std::format("\n\\tempo 4 = {}\n",
				std::round(vm.tempo.to_double()));
		}

		// advance the VM depending on opcode
		if (std::holds_alternative<EndEvent>(ev) ||
			std::holds_alternative<RestartEvent>(ev))
			break;
		else if (std::holds_alternative<JSREvent>(ev)) {
			auto& labe = std::get<JSREvent>(ev);
			if (!vm.jsr_addr.has_value())
				vm.jsr_addr = vm.pc;
			else
				throw std::runtime_error("Invoking JSR with JSR-address already on the stack");
			vm.pc = get_label_addr(labe.label_name);
		}
		else if (std::holds_alternative<ReturnEvent>(ev)) {
			vm.pc = vm.jsr_addr.value() + 1;
			vm.jsr_addr.reset();
		}
		else if (std::holds_alternative<PushAddrEvent>(ev)) {
			vm.push_addr = vm.pc++;
		}
		else if (std::holds_alternative<PopAddrEvent>(ev)) {
			vm.pc = vm.push_addr.value() + 1;
			//vm.push_addr.reset();
		}
		else if (std::holds_alternative<BeginLoopEvent>(ev)) {
			auto& ble = std::get<BeginLoopEvent>(ev);
			vm.loop_addr = vm.pc++;
			vm.loop_iters = ble.iterations;
			vm.loop_count = 0;
		}
		else if (std::holds_alternative<EndLoopEvent>(ev)) {
			auto& ele = std::get<EndLoopEvent>(ev);
			vm.loop_end_addr = vm.pc;
			vm.loop_count++;
			if (vm.loop_count < vm.loop_iters)
				vm.pc = vm.loop_addr.value() + 1;
			else
				++vm.pc;
		}
		else if (std::holds_alternative<LoopIfEvent>(ev)) {
			auto& lie = std::get<LoopIfEvent>(ev);
			if (vm.loop_count >= lie.iterations)
				vm.pc = vm.loop_end_addr.value() + 1;
			else
				++vm.pc;
		}
		else
			++vm.pc;


		int dummy{ bar_accum.extract_whole() };
		if (dummy > 0)
			p_lp += "\n\\allowBreak\n";
	}

	p_lp += "\n}\n";
	return 0;
}

std::string fm::MMLChannel::get_lilypond_clef(void) const {
	return std::format("\\clef \"{}\"\n", m_clef.empty() ? "treble" : m_clef);
}

std::string fm::MMLChannel::to_lilypond_length(std::optional<int> p_length,
	int p_dots, std::optional<int> p_raw, fm::Fraction& p_accumulator) const {
	fm::Fraction l_lp_frac{ 0,1 };

	if (p_raw) {
		auto wnote_ticks{ fm::Fraction(4 * c::TICK_PER_MIN,1) / vm.tempo };
		l_lp_frac = fm::Fraction(p_raw.value(), 1) / wnote_ticks;

		p_accumulator += l_lp_frac;
	}
	else if (p_length.has_value() || vm.st.default_length.length.has_value()) {
		// add the default length-dots if the note did not have an explicit musical length
		int base = p_length.has_value() ? p_length.value() : vm.st.default_length.length.value();
		int final_dots{ p_length ? p_dots : p_dots + vm.st.default_length.dots };

		// numerator = 2^(dots+1) - 1
		int num = (1 << (final_dots + 1)) - 1;
		// denominator = base_len * 2^dots
		int den = base * (1 << final_dots);

		l_lp_frac = fm::Fraction(num, den);

		p_accumulator += fm::Fraction(num, den);

		// check if den is pow2, if so return string with len and dots
		auto is_pow2 = [](int x) {
			return x > 0 && (x & (x - 1)) == 0;
			};

		if (is_pow2(base)) {
			// Emit normal LilyPond duration: "8", "16.", "32.."
			std::string out = std::to_string(base);
			out.append(final_dots, '.');
			return out;
		}
	}
	else if (vm.st.default_length.raw.has_value()) {
		auto wnote_ticks{ fm::Fraction(4 * c::TICK_PER_MIN,1) / vm.tempo };
		l_lp_frac = fm::Fraction(vm.st.default_length.raw.value(), 1) / wnote_ticks;

		p_accumulator += l_lp_frac;
	}
	else
		throw std::runtime_error("Note length could not be determined");

	// base not a power of 2, return raw fraction
	return std::format("1*{}/{}", l_lp_frac.get_num(), l_lp_frac.get_den());
}

bool fm::VMState::operator<(const fm::VMState& rhs) const {
	return std::tie(pc, loop_count, loop_iters, pitch_offset, volume, duty_cycle,
		push_addr, jsr_addr, loop_addr, loop_end_addr) <
		std::tie(rhs.pc, rhs.loop_count, rhs.loop_iters, rhs.pitch_offset, rhs.volume, rhs.duty_cycle,
			rhs.push_addr, rhs.jsr_addr, rhs.loop_addr, rhs.loop_end_addr);
}
