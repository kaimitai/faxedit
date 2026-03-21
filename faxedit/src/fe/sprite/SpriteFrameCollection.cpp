#include "SpriteFrameCollection.h"
#include "fe_sprite_constants.h"

void fe::SpriteFrameCollection::add_chr_bank(const ChrBank& p_bank, bool make_full_ppu) {
	banks.push_back(p_bank);
	if (make_full_ppu)
		expand_last_bank();
}

void fe::SpriteFrameCollection::expand_last_bank(void) {
	ChrBank l_bank{ std::vector<klib::NES_tile>(256, klib::NES_tile()) };
	auto& lastbank{ banks.back() };

	for (std::size_t i{ 0 }; i < lastbank.size(); ++i)
		l_bank.at(c::PPU_COMMON_TILE_START + i) = lastbank[i];

	lastbank = l_bank;
}

void fe::SpriteFrameCollection::expand_bank_if_last(std::size_t p_bank_id) {
	if (p_bank_id == banks.size() - 1)
		expand_last_bank();
}

void fe::SpriteFrameCollection::add_frame(const fe::SpriteAnimationFrame& p_frame) {
	frames.push_back({ p_frame });
}

void fe::SpriteFrameCollection::add_frames(const std::vector<fe::SpriteAnimationFrame>& p_frames) {
	for (const auto& frame : p_frames)
		frames.push_back({ frame });
}

void fe::SpriteFrameCollection::add_frame_chr_bank_index(std::size_t p_frame_idx,
	std::size_t p_chr_bank_idx) {
	frames.at(p_frame_idx).chrbanks.push_back(p_chr_bank_idx);
}

ChrBank fe::SpriteFrameCollection::get_chr_bank(std::size_t p_bank_no,
	bool make_sparse_ppu_if_last) const {

	if (make_sparse_ppu_if_last && p_bank_no == banks.size() - 1) {
		ChrBank result;
		for (std::size_t i{ 0 }; i < c::PPU_COMMON_TILE_COUNT; ++i)
			result.push_back(banks.at(p_bank_no).at(c::PPU_COMMON_TILE_START + i));
		return result;
	}
	else {
		return banks.at(p_bank_no);
	}
}
