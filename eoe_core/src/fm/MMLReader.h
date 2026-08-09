#ifndef FM_MML_READER_H
#define FM_MML_READER_H

#include <map>
#include <set>
#include <string>
#include <vector>
#include "MusicOpcode.h"
#include "fe/Config.h"

using byte = unsigned char;

namespace fm {

	class MMLReader {

		std::vector<fm::MusicInstruction> m_instructions;
		std::map<std::size_t, std::size_t> m_ptr_table;
		std::map<byte, fm::MusicOpcode> m_opcodes;
		std::map<std::string, std::string> m_defines;

		bool is_entrypoint(const std::string& p_token) const;
		std::pair<std::size_t, std::size_t> parse_entrypoint(const std::string& p_token) const;

		bool is_label_definition(const std::string& p_token);
		std::string get_label(const std::string& p_token);

	public:
		MMLReader(const fe::Config& p_config);
		void read_mml_file(const std::string& p_filename,
			const fe::Config& p_config);
		std::vector<byte> get_bytes(void) const;
	};

}

#endif
