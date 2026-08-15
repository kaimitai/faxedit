#include "MMLSongCollection.h"
#include "fm/fm_constants.h"
#include "fe/fe_app_constants.h"
#include "mml_constants.h"
#include "Fraction.h"
#include <format>

fm::MMLSongCollection::MMLSongCollection(void) :
	fm::MMLSongCollection({ -12, -12, 12, 127 })
{
}

fm::MMLSongCollection::MMLSongCollection(const std::vector<int>& p_global_transpose) :
	global_transpose{ p_global_transpose }
{
}

void fm::MMLSongCollection::extract_bytecode_collection(MScriptLoader& p_loader) {

	for (std::size_t i{ 0 }; i < p_loader.get_song_count(); ++i) {
		auto song{ extract_bytecode_song(p_loader, i) };
		fm::Fraction tempo = determine_tempo(song);

		fm::MMLSong bytesong;
		bytesong.tempo = tempo;
		bytesong.index = static_cast<int>(i + 1);

		for (std::size_t channel_idx{ 0 }; channel_idx < 4; ++channel_idx) {
			fm::MMLChannel bytechannel(tempo);

			bytechannel.channel_type = c::CHANNEL_TYPES[channel_idx];

			auto chandata{ normalize_bytecode_channel(song.at(channel_idx)) };

			bytechannel.parse_bytecode(chandata.instrs, chandata.start, chandata.jump_targets);

			bytesong.channels.push_back(bytechannel);
		}

		songs.push_back(bytesong);
	}
}

fm::Fraction fm::MMLSongCollection::determine_tempo(
	const std::vector<BytecodeChannel>& p_song) const
{
	// 1. Build histogram of tick lengths
	std::map<int, int> lcounts;

	for (const auto& ch : p_song) {
		for (const auto& instr : ch.instrs) {
			int D = -1;

			if (instr.second.opcode_byte >= fm::c::HEX_NOTELENGTH_MIN &&
				instr.second.opcode_byte < fm::c::HEX_NOTELENGTH_END)
			{
				D = instr.second.opcode_byte - fm::c::HEX_NOTELENGTH_MIN;
			}
			else if (instr.second.opcode_byte == fm::c::MSCRIPT_OPCODE_SET_LENGTH) {
				D = instr.second.operand.value();
			}

			if (D >= 0)
				++lcounts[D];
		}
	}

	// Allowed tempo range
	const double T_min = 30.0 / 4.0;
	const double T_max = 300.0 / 4.0;

	// 2. Generate candidate tempos
	std::map<fm::Fraction, int> scores;

	for (auto& [D, count] : lcounts) {
		for (const fm::Fraction& f : fm::c::ALLOWED_FRACTIONS) {

			// T = (3600 * f) / D
			fm::Fraction T = fm::Fraction(c::TICK_PER_MIN * 4, 1) * f / fm::Fraction(D, 1);

			// Filter tempo range
			double Td = T.to_double();
			if (Td < T_min || Td > T_max)
				continue;

			// Initialize score bucket
			scores.try_emplace(T, 0);
		}
	}

	// 3. Score each candidate tempo
	for (auto& [T, score] : scores) {
		for (auto& [D, count] : lcounts) {
			for (const fm::Fraction& f : fm::c::ALLOWED_FRACTIONS) {

				// expected ticks = (3600/T) * f
				fm::Fraction expected = fm::Fraction(c::TICK_PER_MIN * 4, 1) * f / T;

				if (expected.is_integer() && expected.extract_whole() == D)
					score += count;
			}
		}
	}

	// 4. Pick best tempo
	fm::Fraction bestT(120 / 4, 1); // fallback
	int bestScore = -1;

	for (auto& [T, score] : scores) {
		if (score > bestScore) {
			bestScore = score;
			bestT = T;
		}
	}

	return bestT * Fraction(4, 1);
}


// this gets rid of all ROM offsets and replaces them with instruction indexes
// for both the jump targets and entrypoing
fm::NormalizedBytecodeChannel fm::MMLSongCollection::normalize_bytecode_channel(
	const fm::BytecodeChannel& ch) const {

	std::vector<fm::MusicInstruction> instrs;
	// ROM offset to instruction index
	std::map<std::size_t, std::size_t> offsettoindex;

	// map all ROM offsets to instruction idx and store them in the result vec
	for (const auto& kv : ch.instrs) {
		std::size_t idx{ instrs.size() };
		offsettoindex[kv.first] = idx;
		instrs.push_back(kv.second);
	}

	// patch all jump targets
	for (auto& ins : instrs) {
		if (ins.jump_target.has_value())
			ins.jump_target = offsettoindex.at(ins.jump_target.value());
	}

	// recalc jump targets: ROM offset -> instruction idx
	std::set<std::size_t> jump_targets;
	for (std::size_t n : ch.jump_targets)
		jump_targets.insert(offsettoindex.at(n));

	// same for entrypoint: ROM offset -> instruction idx on return
	return fm::NormalizedBytecodeChannel(
		instrs,
		offsettoindex.at(ch.start),
		jump_targets
	);
}

fm::BytecodeChannel fm::MMLSongCollection::extract_bytecode_channel(fm::MScriptLoader& p_loader, std::size_t p_song_no,
	std::size_t p_channel_no) {
	p_loader.parse_channel(p_song_no, p_channel_no);
	return fm::BytecodeChannel(p_loader.get_channel_offset(p_song_no, p_channel_no),
		p_loader.m_instrs, p_loader.m_jump_targets);
}

std::vector<fm::BytecodeChannel> fm::MMLSongCollection::extract_bytecode_song(MScriptLoader& p_loader,
	std::size_t p_song_no) {
	std::vector<fm::BytecodeChannel> result;

	for (std::size_t i{ 0 }; i < 4; ++i)
		result.push_back(extract_bytecode_channel(p_loader, p_song_no, i));

	return result;
}

std::string fm::MMLSongCollection::to_string(void) const {
	std::string result{
		std::format(" ; Faxanadu mml (music macro language) file extracted by {} {}\n ; {}\n\n", fe::c::APP_NAME, fe::c::APP_VERSION, fe::c::APP_URL)
	};

	for (std::size_t i{ 0 }; i < global_transpose.size(); ++i)
		result += std::format(" ; global transpose for channel {}: {} semitones\n",
			c::CHANNEL_LABELS[i], global_transpose[i]);
	result += "\n";

	for (const auto& song : songs) {
		result += song.to_string() + "\n";
	}

	return result;
}

std::vector<byte> fm::MMLSongCollection::to_bytecode(const fe::Config& p_config) {
	std::vector<byte> result;
	std::vector<std::size_t> ptr_table;
	std::size_t instr_offset{ 0 };
	std::vector<fm::MusicInstruction> instrs;

	for (std::size_t i{ 0 }; i < songs.size(); ++i)
		for (std::size_t c{ 0 }; c < songs[i].channels.size(); ++c) {
			try {
				auto c_instrs{ songs[i].channels[c].to_bytecode() };
				ptr_table.push_back(c_instrs.entrypt_idx + instr_offset);

				for (auto& instr : c_instrs.instrs) {
					if (instr.jump_target.has_value())
						instr.jump_target = instr.jump_target.value() + instr_offset;
					instrs.push_back(instr);
				}

				instr_offset += c_instrs.instrs.size();
			}
			catch (const std::runtime_error& ex) {
				throw std::runtime_error(std::format("Error when encoding song {} channel {}: {}", i + 1, c + 1, ex.what()));
			}
			catch (const std::exception& ex) {
				throw std::runtime_error(std::format("Error when encoding song {} channel {}: {}", i + 1, c + 1, ex.what()));
			}
			catch (...) {
				throw std::runtime_error(std::format("Unknown error when encoding song {} channel {}", i + 1, c + 1));
			}
		}

	// set byte offsets for all instructions
	std::size_t byte_offset{ 0 };
	for (std::size_t i{ 0 }; i < instrs.size(); ++i) {
		instrs[i].byte_offset = byte_offset;
		byte_offset += instrs[i].size();
	}

	// the instructions and jump targets are relative to 0
	// next we set all byte offsets relative to the bank
	auto music_ptr{ p_config.pointer(c::ID_MUSIC_PTR) };

	// diff between our 0-addr @ start of music data and ptr zero addr
	const uint16_t PTR_DELTA{ static_cast<uint16_t>(music_ptr.first + 2 * ptr_table.size()
		- music_ptr.second) };

	// add this delta to all jump targets and then make our ptr table entries
	for (auto& instr : instrs)
		instr.byte_offset = instr.byte_offset.value() + PTR_DELTA;

	// update all jump targets now that all byte offsets are known
	// turn instr index into byte offsets
	for (std::size_t i{ 0 }; i < instrs.size(); ++i) {
		if (instrs[i].jump_target.has_value())
			instrs[i].jump_target = instrs.at(instrs[i].jump_target.value()).byte_offset.value();
	}

	// update ptr table values in the same way
	for (std::size_t& n : ptr_table)
		n = instrs.at(n).byte_offset.value();

	// emit bytes - ptr table
	for (std::size_t n : ptr_table) {
		result.push_back(static_cast<byte>(n % 256));
		result.push_back(static_cast<byte>(n / 256));
	}
	// emit bytes - instructions
	for (const auto& instr : instrs) {
		auto bytes{ instr.get_bytes() };
		result.insert(end(result), begin(bytes), end(bytes));
	}

	return result;
}

std::vector<smf::MidiFile> fm::MMLSongCollection::to_midi(void) {
	std::vector<smf::MidiFile> result;

	for (auto& song : songs)
		result.push_back(song.to_midi(global_transpose));

	return result;
}

std::vector<std::string> fm::MMLSongCollection::to_lilypond(bool p_incl_percussion) {
	std::vector<std::string> result;

	for (auto& song : songs)
		result.push_back(song.to_lilypond(global_transpose,
			p_incl_percussion));

	return result;
}

void fm::MMLSongCollection::sort(void) {
	std::vector<fm::MMLSong> sorted_songs;

	const auto get_song_w_index = [*this](int p_index) -> std::size_t {
		for (std::size_t i{ 0 }; i < songs.size(); ++i)
			if (songs[i].index == p_index)
				return i;
		throw std::runtime_error(std::format("Missing song with index {}", p_index));
		};

	int max_song_no{ static_cast<int>(songs.size()) };

	for (int i{ 1 }; i <= max_song_no; ++i) {
		auto l_song{ songs[get_song_w_index(i)] };
		l_song.sort();
		sorted_songs.push_back(l_song);
	}

	songs = sorted_songs;
}

std::string fm::MMLSongCollection::integral_notes(void) const {
	std::vector<std::map<fm::Fraction, int>> int_tix;

	for (int qnote_ticks{ 1 }; qnote_ticks <= 255; ++qnote_ticks) {
		// 0-OK, 1-fractional ticks, 2-out of bounds
		std::map<fm::Fraction, int> legals;

		fm::Fraction wnote(qnote_ticks * 4, 1);

		for (const auto& frac : c::ALLOWED_FRACTIONS) {
			fm::Fraction chk{ wnote * frac };

			int fres{ 0 };
			if (!chk.is_integer())
				fres = 1;
			else {
				int ww{ chk.extract_whole() };
				if (ww <= 0 || ww >= 256)
					fres = 2;
			}

			legals.insert(std::make_pair(frac, fres));
		}

		int_tix.push_back(legals);
	}

	std::string result{
		"| tempo (q/min) | T (ticks/q) | coverage | 1 | 2 | 4 | 8 | 16 | 32 | 1. | 2. | 4. | 8. | 16. | 3 | 6 | 12 | 24 |\n"
		"| :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |\n"
	};

	const auto f2r = [&int_tix](std::size_t p_idx, int p_num, int p_dem) -> int {
		return int_tix[p_idx].at(fm::Fraction(p_num, p_dem));
		};

	const auto f2s = [&int_tix](std::size_t p_idx, int p_num, int p_dem) -> std::string {
		std::string l_result;

		int status{ int_tix[p_idx].at(fm::Fraction(p_num, p_dem)) };

		if (status == 0)
			l_result = " Y ";
		else if (status == 1)
			l_result = " N ";
		else
			l_result = " X ";

		return l_result;
		};

	for (std::size_t i{ 0 }; i < int_tix.size(); ++i) {

		int qtix{ static_cast<int>(i + 1) };
		const auto& mp{ int_tix[i] };

		fm::Fraction bpm{ fm::Fraction(3600,1) / fm::Fraction(qtix,1) };
		int coverage{ 0 };

		for (const auto& kv : mp)
			if (kv.second == 0)
				++coverage;

		if (coverage < 11 || f2r(i, 1, 1) != 0)
			continue;

		result += "| ";
		result += bpm.to_tempo_string() + " | ";
		result += std::to_string(i + 1) + " | ";
		result += std::to_string(coverage) + " | ";

		result += f2s(i, 1, 1) + " | ";
		result += f2s(i, 1, 2) + " | ";
		result += f2s(i, 1, 4) + " | ";
		result += f2s(i, 1, 8) + " | ";
		result += f2s(i, 1, 16) + " | ";
		result += f2s(i, 1, 32) + " | ";

		result += f2s(i, 3, 2) + " | ";
		result += f2s(i, 3, 4) + " | ";
		result += f2s(i, 3, 8) + " | ";
		result += f2s(i, 3, 16) + " | ";
		result += f2s(i, 3, 32) + " | ";

		result += f2s(i, 1, 3) + " | ";
		result += f2s(i, 1, 6) + " | ";
		result += f2s(i, 1, 12) + " | ";
		result += f2s(i, 1, 24) + " | ";

		result += "\n";
	}

	return result;
}
