#ifndef FE_SPRITEFRAMECOLLECTION_H
#define FE_SPRITEFRAMECOLLECTION_H

#include "./../../common/klib/NES_tile.h"
#include "SpriteAnimationFrame.h"
#include <vector>

using byte = unsigned char;
using ChrBank = std::vector<klib::NES_tile>;

namespace fe {

	struct SpriteFrame {
		fe::SpriteAnimationFrame frame;
		std::vector<std::size_t> chrbanks;
	};

	struct SpriteFrameCollection {
		std::vector<SpriteFrame> frames;
		std::vector<ChrBank> banks;

		void add_chr_bank(const ChrBank& p_bank, bool make_full_ppu = false);
		void expand_last_bank(void);
		void expand_bank_if_last(std::size_t p_bank_id);
		void add_frame(const fe::SpriteAnimationFrame& p_frame);
		void add_frames(const std::vector<fe::SpriteAnimationFrame>& p_frames);
		void add_frame_chr_bank_index(std::size_t p_frame_idx, std::size_t p_chr_bank_idx);

		ChrBank get_chr_bank(std::size_t p_bank_no, bool make_sparse_ppu_if_last = false) const;
		static void expand_bank(ChrBank& p_bank);
	};

}

#endif
