#ifndef FM_MMLWRITER_H
#define FM_MMLWRITER_H

#include "MScriptLoader.h"
#include "fe/Config.h"

#include <string>
#include <vector>
#include <cstdint>

using byte = unsigned char;

namespace fm {

	class MMLWriter {

		std::map<byte, std::string> m_note_meta_defines;
		// defines per argument domain (when applicable)
		std::map<fm::AudioArgDomain, std::map<byte, std::string>> m_defines;

		std::string argument_to_string(fm::AudioArgDomain p_domain,
			byte p_opcode_byte) const;

	public:
		MMLWriter(const fe::Config& p_config);
		void generate_mml_file(const std::string& p_filename,
			const std::map<std::size_t, fm::MusicInstruction>& p_instructions,
			const std::map<byte, fm::MusicOpcode>& p_opcodes,
			const std::vector<std::size_t>& p_entrypoints,
			const std::set<std::size_t>& p_jump_targets,
			const std::vector<int8_t>& p_chan_pitch_offsets,
			bool p_emit_notes) const;

		void add_defines_subsection(const std::string& p_subheader,
			fm::AudioArgDomain p_domain,
			std::string& p_output) const;
	};

}

#endif
