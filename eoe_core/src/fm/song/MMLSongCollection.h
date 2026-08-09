#ifndef MML_SONGCOLLECTION_H
#define MML_SONGCOLLECTION_H

#include "MMLSong.h"
#include "fm/MScriptLoader.h"
#include "fm/MusicOpcode.h"
#include "fe/Config.h"
#include "common/midifile/MidiFile.h"
#include "Fraction.h"
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fm {

	// ROM offsets
	struct BytecodeChannel {
		std::size_t start;
		std::map<std::size_t, fm::MusicInstruction> instrs;
		std::set<std::size_t> jump_targets;
	};

	// instruction offsets
	struct NormalizedBytecodeChannel {
		std::vector<fm::MusicInstruction> instrs;
		std::size_t start;
		std::set<std::size_t> jump_targets;
	};

	struct MMLSongCollection {
		std::vector<int> global_transpose;

		std::vector<fm::MMLSong> songs;

		void extract_bytecode_collection(MScriptLoader& p_loader);

		MMLSongCollection(void);
		MMLSongCollection(const std::vector<int>& p_global_transpose);
		std::string to_string(void) const;

		std::vector<byte> to_bytecode(const fe::Config& p_config);
		std::vector<smf::MidiFile> to_midi(void);
		std::vector<std::string> to_lilypond(bool p_incl_percussion);
		void sort(void);

		// calcs
		std::string integral_notes(void) const;

	private:
		// turn bytecode into mml via an MML loader
		fm::NormalizedBytecodeChannel normalize_bytecode_channel(
			const fm::BytecodeChannel& ch) const;
		BytecodeChannel extract_bytecode_channel(MScriptLoader& p_loader, std::size_t p_song_no,
			std::size_t p_channel_no);
		std::vector<BytecodeChannel> extract_bytecode_song(MScriptLoader& p_loader, std::size_t p_song_no);

		fm::Fraction determine_tempo(const std::vector<BytecodeChannel>& p_song) const;
	};

}

#endif
