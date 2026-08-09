#include "MMLWriter.h"
#include <format>
#include <utility>
#include "common/klib/Kfile.h"
#include "fm_constants.h"
#include "fi/cli/application_constants.h"
#include "fm_util.h"
#include "MusicOpcode.h"

fm::MMLWriter::MMLWriter(const fe::Config& p_config) {

	// prepare our defines
	m_note_meta_defines = fm::util::generate_note_meta_consts();

	m_defines.insert(std::make_pair(fm::AudioArgDomain::Envelope,
		p_config.bmap(c::ID_DEFINES_ENVELOPE)));
}

void fm::MMLWriter::generate_mml_file(const std::string& p_filename,
	const std::map<std::size_t, fm::MusicInstruction>& p_instructions,
	const std::map<byte, fm::MusicOpcode>& p_opcodes,
	const std::vector<std::size_t>& p_entrypoints,
	const std::set<std::size_t>& p_jump_targets,
	const std::vector<int8_t>& p_chan_pitch_offsets,
	bool p_emit_notes) const {

	const auto& get_next_label = [](std::size_t p_offset,
		int p_lastentry, int& p_lastlabel,
		std::map<std::size_t, std::string>& p_labels) -> std::string {
			auto labiter{ p_labels.find(p_offset) };
			if (labiter != end(p_labels))
				return labiter->second;

			std::string tmp_label{ std::format("@mscript_{:03}_{:02}",
				p_lastentry, p_lastlabel++) };
			p_labels[p_offset] = tmp_label;
			return tmp_label;
		};

	// make a map from offset to vector of entrypoints (tune and channel no combo)
	std::map<std::size_t, std::vector<std::pair<std::size_t, std::size_t>>> l_eps;
	for (std::size_t i{ 0 }; i < p_entrypoints.size(); i += 4)
		for (std::size_t j{ 0 }; j < 4; ++j)
			l_eps[p_entrypoints.at(i + j)].push_back(
				std::make_pair(i / 4, j)
			);

	std::string af{
	std::format(" ; MScript asm file extracted by by {} v{}\n ; {}\n\n",
		fi::appc::APP_NAME, fi::appc::APP_VERSION, fi::appc::APP_URL)
	};

	// inform about channel pitch offsets
	af += std::format(" ; {}\n", std::string(40, '='));
	for (std::size_t i{ 0 }; i < 3 && i < p_chan_pitch_offsets.size(); ++i) {
		af += std::format(" ; {} pitch offset: {}\n",
			c::CHANNEL_LABELS.at(i),
			fm::util::pitch_offset_to_string(p_chan_pitch_offsets[i])
		);
	}
	af += std::format(" ; {}\n\n", std::string(40, '='));

	af += std::format("{}\n ; rest and length constants\n", c::SECTION_DEFINES);

	for (const auto& kv : m_note_meta_defines)
		af += std::format("define {} ${:02x}\n",
			kv.second, kv.first);

	add_defines_subsection("envelope", fm::AudioArgDomain::Envelope, af);

	af += std::format("\n{}", c::SECTION_MSCRIPT);

	int lastentry{ 0 }, lastlabel{ 0 };
	std::map<std::size_t, std::string> l_labels;
	bool note_last{ false };
	std::size_t last_chan{ 0 };

	for (const auto& kv : p_instructions) {
		const fm::MusicInstruction& instr{ kv.second };
		std::size_t offset{ kv.first };

		// are we at an entrypoint? output it
		auto ep{ l_eps.find(offset) };
		if (ep != end(l_eps)) {
			af += std::format("\n\n ; Channel entrypoint\n{}", c::ID_MSCRIPT_ENTRYPOINT);
			for (std::size_t i{ 0 }; i < ep->second.size(); ++i) {
				af += std::format(" {}.{}", ep->second[i].first + 1,
					c::CHANNEL_LABELS.at(ep->second[i].second));
				lastentry = static_cast<int>(ep->second[i].first * 4 + ep->second[i].second);
				lastlabel = 0;
			}
			af += "\n";

			// if the last ptr table entry we see is the noise channel,
			// mark it so we don't emit note names here
			last_chan = ep->second.back().second;
			note_last = false;
		}

		// emit jump labels at this offset
		if (p_jump_targets.find(offset) != end(p_jump_targets)) {
			af += std::format("\n{}:\n",
				get_next_label(offset, lastentry, lastlabel, l_labels));

			note_last = false;
		}

		const auto op{ fm::util::decode_opcode_byte(instr.opcode_byte, p_opcodes) };

		if (op.m_opcodetype == fm::OpcodeType::Note) {
			byte noteval{ instr.opcode_byte };
			std::string noteoutput;

			// we have an actual note, emit the value
			if (noteval >= c::HEX_NOTE_MIN &&
				noteval < c::HEX_NOTE_END) {
				// emit the note hex byte for noise, or for all notes
				// if the emit notes-flag is disabled
				if (last_chan == 3 || !p_emit_notes)
					noteoutput = std::format("${:02x}", noteval);
				else
					noteoutput = util::byte_to_note(noteval, 0);
			}
			else {
				// we have a rest or note length, take it from defines
				// if it exists there
				if (m_note_meta_defines.contains(noteval))
					noteoutput = std::format("{}", m_note_meta_defines.at(noteval));
				else
					noteoutput = std::format("${:02x}", noteval);
			}

			if (note_last)
				af += std::format(" {}", noteoutput);
			else
				af += std::format("\n {}", noteoutput);

			note_last = true;
		}
		else {
			af += std::format("\n {}", op.m_mnemonic);

			if (op.m_argtype == fm::AudioArgType::Byte)
				af += std::format(" {}",
					argument_to_string(op.m_arg_domain,
						instr.operand.value())
				);

			if (op.m_flow == fm::AudioFlow::Jump)
				af += std::format(" {}",
					get_next_label(instr.jump_target.value(),
						lastentry, lastlabel, l_labels)
				);

			note_last = false;
		}
	}

	klib::file::write_string_to_file(af, p_filename);
}


std::string fm::MMLWriter::argument_to_string(fm::AudioArgDomain p_domain,
	byte p_opcode_byte) const {
	if (p_domain == fm::AudioArgDomain::PitchOffset) {
		return std::format("${:02x} ; {}", p_opcode_byte,
			fm::util::pitch_offset_to_string(static_cast<int8_t>(p_opcode_byte)));
	}
	else if (p_domain == fm::AudioArgDomain::SQControl)
		return std::format("0b{:08b} ; {}", p_opcode_byte,
			fm::util::sq_control_to_string(p_opcode_byte));
	else if (m_defines.contains(p_domain) &&
		m_defines.at(p_domain).contains(p_opcode_byte)) {
		return std::format("{}", m_defines.at(p_domain).at(p_opcode_byte));
	}
	else
		return std::format("{}", p_opcode_byte);
}

void fm::MMLWriter::add_defines_subsection(const std::string& p_subheader,
	fm::AudioArgDomain p_domain,
	std::string& p_output) const {
	if (m_defines.contains(p_domain) &&
		!m_defines.at(p_domain).empty()) {
		p_output += std::format(" ; {} defines \n", p_subheader);
		for (const auto& kv : m_defines.at(p_domain)) {
			p_output += std::format("define {} ${:02x}\n",
				kv.second, kv.first);
		}
	}
}
