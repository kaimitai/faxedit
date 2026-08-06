/*
 This class takes in data of different types, some ranges of free space, and allocates the data across all the free space
 with a greedy algorithm, and by deduplicating sub-data across all data types, and generates ptr tables and data sections
*/

#ifndef FE_GODALLOCATOR_H
#define FE_GODALLOCATOR_H

#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

using byte = unsigned char;
using Pointer = std::pair<std::size_t, std::size_t>;
using Bucket = std::vector<byte>;
using PtrData = std::vector<byte>;
using PtrTableData = std::vector<Bucket>;
using BucketLocation = std::pair<std::size_t, std::size_t>;
using PtrLocation = std::pair<std::size_t, std::size_t>;
using SpaceRange = std::pair<std::size_t, std::size_t>;

namespace fe {

	struct PtrPairHash {
		std::size_t operator()(const PtrLocation& p) const noexcept {
			std::size_t h1 = std::hash<std::size_t>{}(p.first);
			std::size_t h2 = std::hash<std::size_t>{}(p.second);
			return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
		}
	};

	struct BucketPlacement {
		std::size_t bucket_idx;   // which bucket
		std::size_t offset;       // where inside that bucket
		std::size_t appended;     // how many bytes to append
	};

	struct RomWrite {
		std::size_t rom_offset;
		std::vector<byte> data;
	};

	struct AllocationResult {
		std::vector<RomWrite> ptr_table_writes;
		std::vector<RomWrite> bucket_writes;
	};

	enum class AllocState { NoInitialize, Initialized, AllocGood, AllocBad };

	class GodAllocator {

		AllocState allocState;

		std::unordered_map<PtrLocation, BucketPlacement, PtrPairHash> m_placed_items;
		std::vector<PtrTableData> m_data;
		std::vector<Bucket> m_buckets;
		std::vector<std::size_t> m_bucket_caps;

		std::optional<BucketLocation> get_bucket_fit(const PtrData& p_data, std::size_t p_bucket_idx) const;
		std::optional<PtrLocation> get_biggest_unplaced_data_idx(void) const;

	public:
		GodAllocator(void);
		void initialize(const std::vector<std::size_t>& p_bucket_sizes, const std::vector<PtrTableData>& p_data);
		void allocate(void);
		std::vector<std::vector<byte>> get_buckets(void) const;
		std::vector<std::vector<PtrLocation>> get_ptr_tables(void) const;
		fe::AllocState get_alloc_state(void) const;

		std::optional<fe::AllocationResult> init_and_allocate(const std::vector<Pointer>& p_pointers,
			const std::vector<PtrTableData>&, const std::vector<SpaceRange>& p_free_ranges,
			bool append_ffs = false);

		std::optional<std::size_t> init_allocate_and_patch_single_table(std::size_t p_ptr_table_rom_offset,
			std::size_t p_ptr_table_rom_zero_addr,
			const PtrTableData& p_data, std::vector<byte>& p_rom);
		std::optional<std::size_t> init_allocate_and_patch_single_table(Pointer p_pointer,
			const PtrTableData& p_data, std::vector<byte>& p_rom);
		
	};

}

#endif
