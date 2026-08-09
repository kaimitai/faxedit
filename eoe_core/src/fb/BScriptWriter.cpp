#include "BScriptWriter.h"
#include "common/klib/Kfile.h"
#include "common/klib/Kstring.h"
#include "fi/cli/application_constants.h"
#include "fb_constants.h"
#include <cstdint>
#include <format>
#include <map>
#include <vector>

fb::BScriptWriter::BScriptWriter(const fe::Config& p_config) {
	sprite_labels = p_config.bmap(c::ID_SPRITE_LABELS);

	for (const auto& kv : c::STR_ARGDOMAIN)
		arg2str.insert(std::make_pair(kv.second, kv.first));

	defines.insert(std::make_pair(fb::ArgDomain::Action, p_config.bmap(c::ID_BSCRIPT_DEF_ACTIONS)));
	defines.insert(std::make_pair(fb::ArgDomain::HopMode, p_config.bmap(c::ID_BSCRIPT_DEF_HOPMODES)));
	defines.insert(std::make_pair(fb::ArgDomain::Direction, p_config.bmap(c::ID_BSCRIPT_DEF_DIRECTIONS)));

	// special handling for RAM address defines since they are 16bit consts
	const auto ramdefs{ p_config.bmap(c::ID_BSCRIPT_DEF_RAM) };
	for (const auto& kv : ramdefs) {
		const auto valuepair{ klib::str::split_string(kv.second, ':') };
		if (valuepair.size() == 2) {
			std::string key{ klib::str::trim(valuepair.at(0)) };
			std::size_t val{ static_cast<std::size_t>(klib::str::parse_numeric(klib::str::trim(valuepair.at(1)))) };

			ram_defines.insert(std::make_pair(val, key));
		}
	}
}

std::string fb::BScriptWriter::get_label_name(std::size_t p_ptr_table_idx, std::size_t p_address) const {
	static std::map<std::size_t, std::string> ls_addr_to_label;
	static std::map<std::size_t, int> ls_local_counter;

	if (ls_addr_to_label.contains(p_address))
		return ls_addr_to_label.at(p_address);
	else {
		++ls_local_counter[p_ptr_table_idx];
		std::string label{ std::format("@label_{}_{}", p_ptr_table_idx, ls_local_counter[p_ptr_table_idx]) };
		ls_addr_to_label.insert(std::make_pair(p_address, label));
		return label;
	}

}

void fb::BScriptWriter::write_asm(const std::string& p_filename,
	const fb::BScriptLoader& loader) const {
	bool rg2_marked{ false };

	std::string af{
		std::format(" ; BScript asm file extracted by {} v{}\n ; {}\n\n",
			fi::appc::APP_NAME, fi::appc::APP_VERSION, fi::appc::APP_URL)
	};

	add_defines(af);
	af += std::format("\n{}", c::SECTION_BSCRIPT);

	const auto& instrs{ loader.m_instrs };
	const auto& jump_targets{ loader.m_jump_targets };
	const auto& entrypoints{ loader.m_ptr_table };

	std::map<std::size_t, std::vector<std::size_t>> offset_to_eps;
	for (std::size_t i{ 0 }; i < entrypoints.size(); ++i)
		offset_to_eps[entrypoints[i]].push_back(i);

	std::size_t ptr_table_index{ 0 };
	for (const auto& kv : instrs) {
		// emit info on region 2 code start if applicable
		if (!rg2_marked && kv.first >= loader.m_rg2_rom_offset) {
			af += "\n\n ; ***** Region 2 code start *****\n";
			rg2_marked = true;
		}

		// emit entrypoints if any
		auto epiter{ offset_to_eps.find(kv.first) };
		if (epiter != end(offset_to_eps)) {
			af += "\n";
			for (std::size_t epidx : epiter->second) {
				af += std::format("\n.entrypoint {}", epidx);
				byte spr_no{ static_cast<byte>(epidx) };
				if (sprite_labels.contains(spr_no))
					af += std::format(" ; {}", sprite_labels.at(spr_no));
				ptr_table_index = spr_no;
			}
		}

		// emit labels if any
		if (jump_targets.contains(kv.first))
			af += std::format("\n{}:", get_label_name(ptr_table_index, kv.first));

		// emit the actual opcode
		const auto& opcode{ kv.second.behavior_byte ?
		loader.behavior_ops.at(kv.second.behavior_byte.value()) :
		loader.opcodes.at(kv.second.opcode_byte) };

		af += "\n  " + opcode.mnemonic;

		for (std::size_t i{ 0 }; i < opcode.args.size(); ++i) {
			emit_operand(af, kv.second.operands.at(i).data_value.value(),
				opcode.args[i].domain, ptr_table_index);
		}
	}

	klib::file::write_string_to_file(af, p_filename);
}

void fb::BScriptWriter::emit_operand(std::string& p_asm, std::size_t p_value,
	fb::ArgDomain domain, std::size_t p_ptr_table_idx) const {

	// unconditional jump targets
	if (domain == fb::ArgDomain::Addr)
		p_asm += std::format(" addr={}", get_label_name(p_ptr_table_idx, p_value));
	// branched jump targets
	else if (domain == fb::ArgDomain::TrueAddr)
		p_asm += std::format(" true={}", get_label_name(p_ptr_table_idx, p_value));
	else if (domain == fb::ArgDomain::FalseAddr)
		p_asm += std::format(" false={}", get_label_name(p_ptr_table_idx, p_value));
	else if (domain == fb::ArgDomain::RAM) {
		if (ram_defines.contains(p_value)) {
			p_asm += std::format(" ram={}", ram_defines.at(p_value));
		}
		else {
			p_asm += std::format(" ram=0x{:04x}", p_value);
		}
	}
	else if (domain == fb::ArgDomain::Zero) {
		if (p_value != 0)
			p_asm += std::format(" zero=${:02x}", p_value);
	}
	else if (domain == fb::ArgDomain::SignedByte) {
		p_asm += std::format(" value={}", static_cast<int8_t>(p_value));
	}
	else if (arg2str.contains(domain)) {
		p_asm += std::format(" {}={}", arg2str.at(domain),
			operand_value(p_value, domain)
		);
	}
	else
		throw std::runtime_error("Unknown argument type");
}

std::string fb::BScriptWriter::operand_value(std::size_t p_value,
	fb::ArgDomain domain) const {
	byte l_value{ static_cast<byte>(p_value) };

	if (defines.contains(domain) && defines.at(domain).contains(l_value))
		return defines.at(domain).at(l_value);
	else
		return std::format("{}", p_value);
}

void fb::BScriptWriter::add_defines(std::string& p_asm) const {
	p_asm += std::format("{}\n", c::SECTION_DEFINES);

	add_defines(p_asm, "direction", fb::ArgDomain::Direction);
	add_defines(p_asm, "action", fb::ArgDomain::Action);
	add_defines(p_asm, "hop mode", fb::ArgDomain::HopMode);

	// special handling for RAM defines
	if (!ram_defines.empty()) {
		p_asm += " ; RAM address defines\n";
		for (const auto& kv : ram_defines)
			p_asm += std::format("define {} ${:04x}\n", kv.second, kv.first);
	}
}

void fb::BScriptWriter::add_defines(std::string& p_asm, const std::string& p_type,
	fb::ArgDomain p_domain) const {
	if (defines.contains(p_domain) && !defines.at(p_domain).empty()) {
		p_asm += std::format(" ; {} defines\n", p_type);
		for (const auto& kv : defines.at(p_domain))
			p_asm += std::format("define {} ${:02x}\n", kv.second, kv.first);
	}
}
