#include "GodAllocator.h"
#include <stdexcept>

fe::GodAllocator::GodAllocator(void) :
	allocState{ fe::AllocState::NoInitialize }
{
}

std::optional<std::size_t> fe::GodAllocator::init_allocate_and_patch_single_table(std::size_t p_ptr_table_rom_offset,
	std::size_t p_ptr_table_rom_zero_addr,
	const PtrTableData& p_data, std::vector<byte>& p_rom) {
	return init_allocate_and_patch_single_table(std::make_pair(p_ptr_table_rom_offset, p_ptr_table_rom_zero_addr),
		p_data, p_rom);
}

std::optional<std::size_t> fe::GodAllocator::init_allocate_and_patch_single_table(Pointer p_pointer,
	const PtrTableData& p_data, std::vector<byte>& p_rom) {
	std::pair<std::size_t, std::size_t> free_range{
		std::make_pair(
			p_pointer.first + 2 * p_data.size(),
			p_pointer.second + 0x4000)
	};

	if (free_range.first > free_range.second)
		return std::nullopt;

	auto allocres{ init_and_allocate(
		{p_pointer},
		{p_data},
		{free_range}
	) };

	if (allocres) {
		std::size_t total_byte_size{ 0 };

		for (const auto& ptrtable : allocres->ptr_table_writes) {
			std::copy(begin(ptrtable.data), end(ptrtable.data), begin(p_rom) + ptrtable.rom_offset);
			total_byte_size += ptrtable.data.size();
		}
		for (const auto& ptrdata : allocres->bucket_writes) {
			std::copy(begin(ptrdata.data), end(ptrdata.data), begin(p_rom) + ptrdata.rom_offset);
			total_byte_size += ptrdata.data.size();
		}
		return p_pointer.first + total_byte_size;
	}
	else
		return std::nullopt;
}

std::optional<fe::AllocationResult> fe::GodAllocator::init_and_allocate(const std::vector<Pointer>& p_pointers,
	const std::vector<PtrTableData>& p_ptr_data, const std::vector<SpaceRange>& p_free_ranges,
	bool append_ffs) {

	std::vector<std::size_t> bucket_sizes;
	for (const auto& freerange : p_free_ranges)
		bucket_sizes.push_back(freerange.second - freerange.first);

	initialize(bucket_sizes, p_ptr_data);
	allocate();

	if (allocState == fe::AllocState::AllocBad)
		return std::nullopt;

	// generate all ptr tables
	auto ptrtables{ get_ptr_tables() };

	std::vector<RomWrite> out_ptr_tables, out_data;

	for (std::size_t i{ 0 }; i < ptrtables.size(); ++i) {
		const auto& ptrtable{ ptrtables[i] };

		std::vector<byte> ptr_cpu_values;
		std::size_t ptr_rom_offset{ p_pointers.at(i).first };

		// ptr are currently (bucket idx, byte offset in bucket)
		for (std::size_t j{ 0 }; j < ptrtable.size(); ++j) {
			// get ROM offset of ptr location
			std::size_t ptr_rom_offset{ p_free_ranges.at(ptrtable[j].first).first + ptrtable[j].second };
			std::size_t ptr_cpu_offset{ ptr_rom_offset - p_pointers.at(i).second };

			ptr_cpu_values.push_back(static_cast<byte>(ptr_cpu_offset % 256));
			ptr_cpu_values.push_back(static_cast<byte>(ptr_cpu_offset / 256));
		}

		out_ptr_tables.push_back(fe::RomWrite(ptr_rom_offset, ptr_cpu_values));
	}

	for (std::size_t i{ 0 }; i < m_buckets.size(); ++i) {
		std::size_t bucket_rom_offset{ p_free_ranges.at(i).first };

		auto bucketbytes{ m_buckets[i] };
		// optionally apped with 0xff to the capacity so it is too see free space remaining
		if (append_ffs)
			for (std::size_t cap{ 0 }; cap < m_bucket_caps[i]; ++cap)
				bucketbytes.push_back(0xff);

		out_data.push_back(fe::RomWrite(bucket_rom_offset, bucketbytes));
	}

	return fe::AllocationResult(out_ptr_tables, out_data);
}

void fe::GodAllocator::initialize(const std::vector<std::size_t>& p_bucket_sizes,
	const std::vector<PtrTableData>& p_data) {
	m_data = p_data;
	m_bucket_caps = p_bucket_sizes;
	m_buckets = std::vector<Bucket>(p_bucket_sizes.size());
	m_placed_items.clear();
	allocState = fe::AllocState::Initialized;
}

std::optional<PtrLocation> fe::GodAllocator::get_biggest_unplaced_data_idx(void) const {
	std::optional<PtrLocation> result{ std::nullopt };
	std::optional<std::size_t> biggestsize;

	for (std::size_t datatype{ 0 }; datatype < m_data.size(); ++datatype)
		for (std::size_t ptrindex{ 0 }; ptrindex < m_data[datatype].size(); ++ptrindex) {
			if (!m_placed_items.contains(std::make_pair(datatype, ptrindex))) {
				if (!biggestsize || biggestsize.value() < m_data[datatype][ptrindex].size()) {
					biggestsize = m_data[datatype][ptrindex].size();
					result = std::make_pair(datatype, ptrindex);
				}
			}
		}

	return result;
}

std::vector<std::vector<byte>> fe::GodAllocator::get_buckets(void) const {
	return m_buckets;
}

std::vector<std::vector<PtrLocation>> fe::GodAllocator::get_ptr_tables(void) const {
	std::vector<std::vector<PtrLocation>> result;

	for (std::size_t i{ 0 }; i < m_data.size(); ++i) {
		std::vector<PtrLocation> ptrs;

		for (std::size_t j{ 0 }; j < m_data[i].size(); ++j) {
			auto key{ std::make_pair(i, j) };
			auto iter{ m_placed_items.find(key) };

			if (iter == end(m_placed_items))
				throw std::runtime_error("Could not find key");
			else
				ptrs.push_back(std::make_pair(iter->second.bucket_idx, iter->second.offset));
		}

		result.push_back(ptrs);
	}

	return result;
}

void fe::GodAllocator::allocate(void) {
	if (allocState != fe::AllocState::Initialized)
		return;

	auto nextblock{ get_biggest_unplaced_data_idx() };

	while (nextblock) {

		std::optional<BucketPlacement> bestloc;

		for (std::size_t bucketidx{ 0 }; bucketidx < m_buckets.size(); ++bucketidx) {

			auto result{ get_bucket_fit(m_data[nextblock->first][nextblock->second], bucketidx) };

			if (result) {
				if (result->second == 0) {
					bestloc = { bucketidx, result->first, result->second };
					break;
				}
				else if (!bestloc || bestloc->appended > result->second)
					bestloc = { bucketidx, result->first, result->second };
			}
		}

		if (!bestloc) {
			allocState = fe::AllocState::AllocBad;
			return;
		}

		const auto& blockdata{ m_data[nextblock->first][nextblock->second] };
		std::size_t append_offset{ blockdata.size() - bestloc->appended };
		std::size_t place_bucket_no{ bestloc->bucket_idx };
		m_bucket_caps[place_bucket_no] -= bestloc->appended;

		m_buckets[place_bucket_no].insert(end(m_buckets[place_bucket_no]),
			begin(blockdata) + append_offset, end(blockdata));

		m_placed_items.insert(std::make_pair(*nextblock, *bestloc));

		nextblock = get_biggest_unplaced_data_idx();
	}

	allocState = fe::AllocState::AllocGood;
}

std::optional<BucketLocation> fe::GodAllocator::get_bucket_fit(const PtrData& p_data, std::size_t p_bucket_idx) const {
	const Bucket& bucket = m_buckets[p_bucket_idx];
	size_t capacity = m_bucket_caps[p_bucket_idx];  // remaining bytes we're allowed to append

	// trivial case: empty data fits anywhere, nothing will be appended
	if (p_data.empty())
		return std::make_pair(0, 0);

	const size_t bucketSize = bucket.size();
	const size_t dataSize = p_data.size();

	// Track the best candidate (smallest appended == largest usable overlap).
	size_t bestOffset = static_cast<size_t>(-1);
	size_t bestAppended = static_cast<size_t>(-1);

	// Single pass: try all alignments where data[0] would land at bucket[pos].
	// We also try pos == bucketSize to represent “no overlap, append all”.
	for (std::size_t pos{ 0 }; pos <= bucketSize; ++pos) {
		// Overlap length is bounded by how much bucket remains from pos,
		// and the entire data length.
		const size_t overlapLen =
			(pos < bucketSize)
			? std::min(bucketSize - pos, dataSize)
			: 0u; // if pos == bucketSize, there is no overlap

		// Compare the overlapping region (if any).
		bool overlapMatches = true;
		if (overlapLen > 0) {
			// Compare p_data[0..overlapLen-1] with bucket[pos..pos+overlapLen-1]
			for (size_t i = 0; i < overlapLen; ++i) {
				if (p_data[i] != bucket[pos + i]) {
					overlapMatches = false;
					break;
				}
			}
		}

		if (!overlapMatches) {
			// If overlap bytes don't match, this alignment is not viable.
			continue;
		}

		// If we get here, the overlap (possibly 0) matches.
		// Appended tail is whatever part of p_data that goes beyond the overlap.
		const size_t appended = dataSize - overlapLen;

		// Full containment (best-possible outcome): p_data fits entirely within bucket.
		if (appended == 0)
			return BucketLocation{ pos, 0 };

		// Otherwise we need to append bytes. Check capacity.
		if (appended <= capacity) {
			// Keep the smallest appended (largest usable overlap).
			if (bestAppended == static_cast<size_t>(-1) || appended < bestAppended) {
				bestAppended = appended;
				bestOffset = pos;
				// We intentionally do not early-return here,
				// because there could still be a full containment at another pos,
				// which we detect above and return immediately if found.
			}
		}
		// If appended > capacity, this alignment is infeasible; skip it.
	}

	// If we found any feasible candidate, return the best; otherwise, nullopt.
	if (bestAppended != static_cast<size_t>(-1)) {
		return BucketLocation{ bestOffset, bestAppended };
	}

	return std::nullopt;
}

fe::AllocState fe::GodAllocator::get_alloc_state(void) const {
	return allocState;
}
