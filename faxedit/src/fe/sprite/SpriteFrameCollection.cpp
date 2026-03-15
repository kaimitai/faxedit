#include "SpriteFrameCollection.h"

void fe::SpriteFrameCollection::add_chr_bank(const ChrBank& p_bank) {
	banks.push_back(p_bank);
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
