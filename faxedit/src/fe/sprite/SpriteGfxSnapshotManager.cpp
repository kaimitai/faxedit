#include "SpriteGfxSnapshotManager.h"

bool fe::SpriteGfxSnapshotManager::has_snapshot(std::size_t p_coll_id) const {
	auto iter{ history.find(p_coll_id) };
	if (iter != end(history) && !iter->second.empty())
		return true;
	else
		return false;
}

std::pair<std::size_t, std::size_t> fe::SpriteGfxSnapshotManager::query_snapshot(std::size_t p_coll_id) const {
	if (!has_snapshot(p_coll_id))
		return std::make_pair(0, 0);
	else {
		const auto& stacktop{ history.at(p_coll_id).back() };
		return std::make_pair(stacktop.bank_ids.size(), stacktop.frame_ids.size());
	}
}

std::pair<std::size_t, std::size_t> fe::SpriteGfxSnapshotManager::restore_snapshot(fe::SpriteFrameCollection& p_coll,
	std::size_t p_coll_id) {
	if (!has_snapshot(p_coll_id))
		return std::make_pair(0, 0);

	fe::SpriteGfxSnapshot snap{ std::move(history[p_coll_id].back()) };
	history[p_coll_id].pop_back();

	auto result{ std::make_pair(snap.bank_ids.size(), snap.frame_ids.size()) };

	for (std::size_t i{ 0 }; i < snap.bank_ids.size(); ++i)
		p_coll.banks.at(snap.bank_ids[i]) = snap.banks[i];
	for (std::size_t i{ 0 }; i < snap.frame_ids.size(); ++i)
		p_coll.frames.at(snap.frame_ids[i]).frame = snap.frames[i];

	return result;
}

void fe::SpriteGfxSnapshotManager::apply_chr_import(fe::SpriteFrameCollection& p_coll,
	std::size_t p_coll_id, std::size_t p_bank_id, const ChrBank& p_bank) {

	fe::SpriteGfxSnapshot snap{
		.bank_ids = {p_bank_id},
		.banks = {p_coll.banks.at(p_bank_id)}
	};

	p_coll.banks.at(p_bank_id) = p_bank;

	add_entry(p_coll_id, snap);
}

void fe::SpriteGfxSnapshotManager::apply_bmp_import(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
	const fe::SpriteImportResult& impres, const fe::ChrBankImpact& impact) {

	std::vector<std::size_t> bank_idxs, frame_idxs;
	std::vector<ChrBank> banks;
	std::vector<fe::SpriteAnimationFrame> frames;

	for (std::size_t l_bank_id : impact.chr_bank_indexes) {
		bank_idxs.push_back(l_bank_id);
		banks.push_back(p_coll.banks.at(l_bank_id));
		p_coll.banks.at(l_bank_id) = impres.tiles;
	}
	for (std::size_t i{ 0 }; i < impres.frames.size(); ++i) {
		frame_idxs.push_back(impact.frame_indexes.at(i));
		frames.push_back(p_coll.frames.at(impact.frame_indexes.at(i)).frame);
		p_coll.frames.at(impact.frame_indexes.at(i)).frame.tilemap = impres.frames[i].tilemap;
	}

	add_entry(p_coll_id, fe::SpriteGfxSnapshot{
	.bank_ids = bank_idxs,
	.banks = banks,
	.frame_ids = frame_idxs,
	.frames = frames
		});
}

void fe::SpriteGfxSnapshotManager::add_snapshot(const fe::SpriteFrameCollection& p_coll,
	std::size_t p_coll_id, const fe::ChrBankImpact& impact) {

	std::vector<std::size_t> frame_idxs{ impact.frame_indexes };
	std::vector<fe::SpriteAnimationFrame> frames;
	for (std::size_t idx : frame_idxs)
		frames.push_back(p_coll.frames.at(idx).frame);

	std::vector<std::size_t> bank_idxs(begin(impact.chr_bank_indexes), end(impact.chr_bank_indexes));
	std::vector<ChrBank> banks;
	for (std::size_t idx : bank_idxs)
		banks.push_back(p_coll.banks.at(idx));

	add_entry(p_coll_id, fe::SpriteGfxSnapshot{
		.bank_ids = bank_idxs,
		.banks = banks,
		.frame_ids = frame_idxs,
		.frames = frames
		});
}

void fe::SpriteGfxSnapshotManager::add_entry(std::size_t p_coll_id, const SpriteGfxSnapshot& snapshot) {

	history[p_coll_id].push_back(snapshot);

	if (history[p_coll_id].size() > 50)
		history[p_coll_id].pop_front();
}
