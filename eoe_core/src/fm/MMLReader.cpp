#include "MMLReader.h"
#include "fm_constants.h"
#include "common/klib/Kstring.h"
#include "fm_util.h"
#include <format>
#include <stdexcept>

fm::MMLReader::MMLReader(const fe::Config& p_config) {
	m_opcodes = parse_opcode_map(p_config.bmap(c::ID_MSCRIPT_OPCODES));
}

// we employ the same strategy as for iscript, but this is less complex
// we still need to resolve jump targets and ptr table entries however
void fm::MMLReader::read_mscript_asm(const std::vector<std::string>& p_asm,
	const fe::Config& p_config) {
	auto music_ptr{ p_config.pointer(c::ID_MUSIC_PTR) };

	std::vector<std::string> tokens;

	// local scope, we will discard most of this
	{
		bool l_defines{ false };
		std::vector<std::string> mscript_lines;

		auto l_lines{ p_asm };

		for (const auto& line : l_lines) {
			auto stripline = klib::str::trim(klib::str::strip_comment(line));
			if (stripline.empty())
				continue;

			if (stripline == c::SECTION_DEFINES)
				l_defines = true;
			else if (stripline == c::SECTION_MSCRIPT)
				l_defines = false;
			else {
				if (l_defines) {
					const auto kv{ klib::str::parse_define(stripline) };
					if (m_defines.contains(kv.first))
						throw std::runtime_error("Define redefinition: " + kv.first);
					else
						m_defines.insert(std::make_pair(kv.first,
							kv.second));
				}
				else {
					mscript_lines.push_back(stripline);
				}

			}

		}

		// resolve all tokens already
		for (const auto& line : mscript_lines) {
			auto linetokens{ klib::str::split_whitespace(line) };
			for (const auto& token : linetokens) {
				std::string finaltoken{ klib::str::trim(token) };
				if (m_defines.contains(finaltoken))
					finaltoken = m_defines[finaltoken];
				tokens.push_back(klib::str::to_lower(finaltoken));
			}
		}
	}

	// make an opcode mnemonic reverse lookup
	std::map<std::string, byte> mnemonics;
	for (const auto& kv : m_opcodes) {
		mnemonics.insert(std::make_pair(
			klib::str::to_lower(kv.second.m_mnemonic), kv.first
		));
	}

	// clear out our data
	m_ptr_table.clear();
	m_instructions.clear();

	// prepare structures we need for bookkeeping

	// map string label to instruction index
	std::map<std::string, std::size_t> label_to_instr_idx;
	// map from ptr table index to instruction index
	std::map<std::size_t, std::size_t> ptr_to_instr_index;
	// map from label to all instruction indexes having it as target
	std::map<std::string, std::set<std::size_t>> jump_labels;
	// byte offset relative to the start of data
	std::size_t offset{ 0 };

	// we now have all our tokens whitespace-trimmed, and can get to work
	for (std::size_t tokenno{ 0 }; tokenno < tokens.size();) {
		std::string token = tokens[tokenno];

		if (token == klib::str::to_lower(c::ID_MSCRIPT_ENTRYPOINT)) {
			while (++tokenno < tokens.size() && is_entrypoint(tokens[tokenno])) {
				const auto& entrypttoken = tokens[tokenno];
				auto entrypt = parse_entrypoint(entrypttoken);
				std::size_t entryidx{ 4 * entrypt.first + entrypt.second };
				if (ptr_to_instr_index.contains(entryidx))
					throw std::runtime_error(std::format("Multiple definitions for entrypoint {}", entrypttoken));
				ptr_to_instr_index.insert({ entryidx, m_instructions.size() });
			}
		}
		else if (is_label_definition(token)) {
			std::string label{ get_label(token) };

			if (label_to_instr_idx.contains(label))
				throw std::runtime_error(std::format("Label redefinition: {}", label));
			else
				label_to_instr_idx.insert(std::make_pair(
					label,
					m_instructions.size()
				));
			++tokenno;
		}
		else if (tokenno < tokens.size()) {
			byte opcodebyte{ 0 };

			if (mnemonics.contains(token)) {
				opcodebyte = mnemonics[token];
			}
			else if (util::is_note(token)) {
				opcodebyte = util::note_to_byte(token, 0);
			}
			else
			{
				try {
					opcodebyte = static_cast<byte>(
						klib::str::parse_numeric(token)
						);
				}
				catch (...) {
					throw std::runtime_error("Could not parse token " + token);
				}
			}

			const auto opcode{ fm::util::decode_opcode_byte(opcodebyte, m_opcodes) };

			std::optional<byte> arg;

			if (opcode.m_argtype == fm::AudioArgType::Byte) {
				arg = klib::str::parse_numeric(tokens.at(++tokenno));
			}
			else if (opcode.m_argtype == fm::AudioArgType::Address) {
				jump_labels[tokens.at(++tokenno)].insert(m_instructions.size());
			}

			m_instructions.push_back(fm::MusicInstruction(
				opcodebyte,
				arg,
				std::nullopt,
				offset
			));

			offset += opcode.size();
			++tokenno;
		}
	}

	// diff between our 0-addr @ start of music data and ptr zero addr
	const uint16_t PTR_DELTA{ static_cast<uint16_t>(music_ptr.first + 2 * ptr_to_instr_index.size()
		- music_ptr.second) };

	for (const auto& kv : jump_labels) {
		std::size_t jump_instr{ label_to_instr_idx.at(kv.first) };
		std::size_t jump_offset{ m_instructions.at(jump_instr).byte_offset.value() };

		for (const auto instrind : kv.second)
			m_instructions.at(instrind).jump_target = jump_offset + PTR_DELTA;
	}

	// similarly for ptr table entries
	for (const auto& kv : ptr_to_instr_index) {
		m_ptr_table[kv.first] = m_instructions.at(kv.second).byte_offset.value() + PTR_DELTA;
	}

	// final sanity check
	if (m_ptr_table.size() % 4 != 0) {
		throw std::runtime_error("Four channels must be defined for each song");
	}
	else {
		for (std::size_t i{ 0 }; i < m_ptr_table.size(); ++i)
			if (!m_ptr_table.contains(i))
				throw std::runtime_error(
					std::format("Channel {} not defined for song {}",
						c::CHANNEL_LABELS.at((i % 4) + 1), i / 4)
				);
	}
}

bool fm::MMLReader::is_label_definition(const std::string& p_token) {
	return !p_token.empty() &&
		p_token.back() == ':';
}

std::string fm::MMLReader::get_label(const std::string& p_token) {
	return p_token.substr(0, p_token.size() - 1);
}

bool fm::MMLReader::is_entrypoint(const std::string& p_token) const {
	auto split{ klib::str::split_string(p_token, '.') };
	if (split.size() != 2)
		return false;
	else {
		try {
			int dummy{ klib::str::parse_numeric(split[0]) };

			for (const auto& s : c::CHANNEL_LABELS)
				if (klib::str::to_lower(s) == split[1])
					return true;
			return false;
		}
		catch (...) {
			return false;
		}
	}
}

std::pair<std::size_t, std::size_t> fm::MMLReader::parse_entrypoint(const std::string& p_token) const {
	auto split{ klib::str::split_string(p_token, '.') };

	std::size_t l_chan_no{ 0 };
	for (std::size_t i{ 0 }; i < c::CHANNEL_LABELS.size(); ++i)
		if (klib::str::to_lower(c::CHANNEL_LABELS[i]) == split[1]) {
			l_chan_no = i;
			break;
		}

	int l_song_no{ klib::str::parse_numeric(split[0]) };
	if (l_song_no < 1)
		throw std::runtime_error(std::format("Invalid song no {}", l_song_no));

	return std::make_pair(
		static_cast<std::size_t>(l_song_no - 1),
		l_chan_no);
}

std::vector<byte> fm::MMLReader::get_bytes(void) const {
	std::vector<byte> result;

	for (std::size_t i{ 0 }; i < m_ptr_table.size(); ++i) {
		result.push_back(static_cast<byte>(m_ptr_table.at(i) % 256));
		result.push_back(static_cast<byte>(m_ptr_table.at(i) / 256));
	}
	for (const auto& instr : m_instructions) {
		auto bytes{ instr.get_bytes() };
		result.insert(end(result), begin(bytes), end(bytes));
	}

	return result;
}
