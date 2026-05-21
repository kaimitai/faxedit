#ifndef FE_SPRITE_GFX_SNAPSHOT_MANAGER_H
#define FE_SPRITE_GFX_SNAPSHOT_MANAGER_H

#include <map>
#include <deque>
#include <utility>
#include <vector>
#include "SpriteGfxManager.h"
#include "./../ChrStructures.h"

using ChrBank = std::vector<klib::NES_tile>;

namespace fe {

	struct SpriteGfxSnapshot {
		std::vector<std::size_t> bank_ids;
		std::vector<ChrBank> banks;
		std::vector<std::size_t> frame_ids;
		std::vector<fe::SpriteAnimationFrame> frames;
	};

	class SpriteGfxSnapshotManager {

		std::map<std::size_t, std::deque<SpriteGfxSnapshot>> history;
		void add_entry(std::size_t p_coll_id, const SpriteGfxSnapshot& snapshot);

	public:
		void reset(void);
		bool has_snapshot(std::size_t p_coll_id) const;
		std::pair<std::size_t, std::size_t> query_snapshot(std::size_t p_coll_id) const;
		std::pair<std::size_t, std::size_t> restore_snapshot(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id);
		std::pair<std::size_t, std::size_t> restore_flat_snapshot(ChrBank& p_bank,
			std::vector<fe::SpriteAnimationFrame>& p_frames, std::size_t p_coll_id);
		void apply_chr_import(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			std::size_t p_bank_id, const ChrBank& p_bank);
		void apply_flat_chr_import(std::size_t p_coll_id, std::size_t p_bank_id, const ChrBank& p_bank,
			ChrBank& bank_ref);
		void apply_bmp_import(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			const fe::SpriteImportResult& impres, const fe::ChrBankImpact& impact);
		void apply_flat_bmp_import(std::size_t p_coll_id,
			const ChrBank& p_bank, const std::vector<fe::SpriteAnimationFrame>& p_frames,
			ChrBank& bank_ref, std::vector<fe::SpriteAnimationFrame>& p_frame_ref);
		void add_snapshot(const fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			const fe::ChrBankImpact& impact);

		void add_flat_snapshot(std::size_t p_coll_id, const ChrBank& p_bank,
			const std::vector<fe::SpriteAnimationFrame>& p_frames);
	};

}

#endif
