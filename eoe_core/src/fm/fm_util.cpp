#include <format>
#include "fm_constants.h"
#include "fm/song/mml_constants.h"
#include "fm_util.h"

std::map<byte, std::string> fm::util::generate_note_meta_consts(void) {
	std::map<byte, std::string> result;

	result[c::HEX_REST] = "r";

	for (byte i{ c::HEX_NOTELENGTH_MIN }; i < c::HEX_NOTELENGTH_END; ++i)
		result[i] = std::format("l{}", i - c::HEX_NOTELENGTH_MIN);

	return result;
}

fm::MusicOpcode fm::util::decode_opcode_byte(byte b,
	const std::map<byte, fm::MusicOpcode>& p_opcodes) {
	auto iter{ p_opcodes.find(b) };
	if (iter == end(p_opcodes))
		return fm::MusicOpcode(std::string(), fm::OpcodeType::Note,
			fm::AudioArgType::None, fm::AudioFlow::Continue,
			fm::AudioArgDomain::None);
	else
		return iter->second;
}

std::string fm::util::pitch_offset_to_string(int8_t val) {
	int total = static_cast<int>(val);

	bool neg{ total < 0 };
	if (neg)
		total *= -1;

	if (total == 0)
		return "no offset";

	// Compute octave + semitone decomposition
	int octaves = total / 12;
	int semitones = total % 12;

	// Normalize semitones into 0-11
	if (semitones < 0) {
		semitones += 12;
		octaves -= 1;
	}

	// Direction string
	std::string dir;
	if (!neg)
		dir = "up";
	else
		dir = "down";

	// Build result
	std::string result = dir + " ";
	if (octaves != 0) {
		result += std::to_string(std::abs(octaves)) + " octave";
		if (std::abs(octaves) > 1) result += "s";
		if (semitones != 0) result += ", ";
	}
	if (semitones != 0) {
		result += std::to_string(semitones) + " semitone";
		if (semitones > 1) result += "s";
	}

	return result;
}

std::string fm::util::byte_to_note(byte p_val, int8_t offset) {
	int noteIndex = (static_cast<int>(p_val) + static_cast<int>(offset)) - 1;

	int octave = 2 + (noteIndex / 12);
	int semitone = noteIndex % 12;

	return std::format("{}{}", c::NOTE_NAMES.at(semitone), octave);
}

std::string fm::util::sq_control_to_string(byte val) {

	// Duty cycle (bits 7-6)
	int dutyBits = (val >> 6) & 0x3;
	std::string duty;
	switch (dutyBits) {
	case 0: duty = "12.5%"; break;
	case 1: duty = "25%"; break;
	case 2: duty = "50%"; break;
	case 3: duty = "75%"; break;
	}

	// Flags
	bool lengthHalt = (val & 0x20) != 0;
	bool constantVol = (val & 0x10) != 0;

	// Volume (bits 3-0)
	int volume = val & 0x0F;

	// Build comment string
	std::string comment = "duty=" + duty +
		", length counter halt=" + (lengthHalt ? "on" : "off") +
		", constant volume=" + (constantVol ? "on" : "off") +
		", volume=" + std::to_string(volume);

	return comment;
}

bool fm::util::is_note(const std::string& token) {
	if (token.size() < 2)
		return false;

	// Step 1: first char must be a-g
	char first = std::tolower(token[0]);
	if (first < 'a' || first > 'g')
		return false;

	// Step 2: optional accidental
	size_t i = 1;
	if (token[i] == '+' || token[i] == '-')
		++i;

	// Step 3: remainder must be digits
	if (i >= token.size())
		return false;

	for (; i < token.size(); ++i)
		if (!std::isdigit(static_cast<unsigned char>(token[i])))
			return false;

	return true;
}

byte fm::util::note_to_byte(const std::string& token, int8_t offset) {
	char base = std::tolower(token[0]);
	int semitone; switch (base) {
	case 'c': semitone = 0; break;
	case 'd': semitone = 2; break;
	case 'e': semitone = 4; break;
	case 'f': semitone = 5; break;
	case 'g': semitone = 7; break;
	case 'a': semitone = 9; break;
	case 'b': semitone = 11; break;
	default: throw std::runtime_error("Invalid note letter");
	}

	size_t i = 1;
	if (i < token.size() && (token[i] == '+' || token[i] == '-')) {
		if (token[i] == '+')
			semitone += 1;
		else semitone -= 1;
		++i;
	}

	// Normalize semitone into 0-11 range
	if (semitone < 0)
		semitone += 12;
	if (semitone > 11)
		semitone -= 12;

	// Step 3: parse octave (assume valid digits)
	int octave = std::stoi(token.substr(i));

	// Step 4: compute raw index (C2 = $01)
	int noteIndex = (octave - 2) * 12 + semitone;
	int rawIndex = noteIndex + 1;

	// Step 5: apply channel offset
	int finalByte = rawIndex + static_cast<int>(offset);

	// Step 6: range check
	if (finalByte < c::HEX_NOTE_MIN)
		throw std::runtime_error("Note index underflow after transpose");

	if (finalByte >= c::HEX_NOTE_END) {
		throw std::runtime_error("Note index overflow after transpose");
	}

	return static_cast<uint8_t>(finalByte);
}

std::pair<int, int> fm::util::note_string_to_pitch(const std::string& s) {
	// s is something like "c", "c+", "f-", "g+8.", etc.
	// We only care about the first 1-2 chars.

	char letter = s[0];
	int base = 0;

	switch (letter) {
	case 'c': base = 0;  break;
	case 'd': base = 2;  break;
	case 'e': base = 4;  break;
	case 'f': base = 5;  break;
	case 'g': base = 7;  break;
	case 'a': base = 9;  break;
	case 'b': base = 11; break;
	}

	// accidental
	if (s.size() > 1) {
		if (s[1] == '+') base += 1;
		else if (s[1] == '-') base -= 1;
	}

	int octave_delta{ 0 };
	if (base < 0) {
		base = 11;
		octave_delta = -1;
	}
	else if (base >= 12) {
		base = 0;
		octave_delta = 1;
	}

	return std::make_pair(base, octave_delta);
}

int fm::util::mml_constant_to_int(const std::string& name) {
	std::string upper_const{ klib::str::to_upper(name) };

	if (c::OPERAND_CONSTANTS.contains(upper_const))
		return c::OPERAND_CONSTANTS.at(upper_const);
	else
		throw std::runtime_error(std::format("Constant ${} undefined", name));
}

std::string fm::util::mml_arg_to_string(fm::MmlArgDomain p_domain,
	int p_value) {

	const auto iter{ c::OPERAND_CONSTANT_BY_ARG_DOMAIN.find(p_domain) };
	if (iter == end(c::OPERAND_CONSTANT_BY_ARG_DOMAIN))
		return std::to_string(p_value);
	else {
		const auto jter{ iter->second.find(p_value) };
		if (jter == end(iter->second))
			return std::to_string(p_value);
		else
			return std::format("${}", jter->second);
	}

}

std::size_t fm::util::get_music_count(const fe::Config& p_config, const std::vector<byte>& p_rom) {
	auto musicptr{ p_config.pointer(c::ID_MUSIC_PTR) };
	std::size_t result{ 0 };
	std::size_t lowest_target{ 0x10000 };

	// read ptr by ptr until the cursor gets "close" to an address referenced by a ptr
	// the original ROM data has a stray 0xff between ptr table and music data
	for (std::size_t i{ 0 }; ; i += 2) {
		std::size_t next_ptr_addr{ (musicptr.first - musicptr.second) + (i + 2) };
		if (next_ptr_addr > lowest_target + 1)
			break;

		++result;
		std::size_t ptr_addr{ static_cast<std::size_t>(p_rom.at(musicptr.first + i)) +
			256 * static_cast<std::size_t>(p_rom.at(musicptr.first + i + 1)) };
		lowest_target = std::min(lowest_target, ptr_addr);
	}

	return result / 4;
}
