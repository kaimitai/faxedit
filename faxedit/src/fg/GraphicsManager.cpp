#include "GraphicsManager.h"
#include "ObjectAnimation.h"

void fg::GraphicsManager::extract_graphics_collection(fg::GraphicsCollection& p_coll,
	const std::vector<byte>& p_rom,
	std::size_t p_portrait_count,
	std::size_t p_portrait_anim_frame_count,
	std::size_t p_portrait_chr_offset,
	std::size_t p_portrait_chr_count,
	const std::vector<std::size_t>& p_lookup_table_offsets,
	const std::vector<std::size_t>& p_anim_frame_offsets,
	std::size_t p_palette_offset,
	std::size_t p_frames_per_object,
	const std::vector<std::size_t>& p_lookup_table_sizes) {

	p_coll = fg::GraphicsCollection();
	p_coll.m_palette = extract_palette(p_rom, p_palette_offset);
	p_coll.m_nes_tiles = extract_nes_tiles(p_rom, p_portrait_chr_offset, p_portrait_chr_count);

	for (std::size_t i{ 0 }; i < p_portrait_count; ++i) {
		fg::ObjectAnimation l_objanim;

		l_objanim.m_tile_lookup = extract_lookup_table_ff_delim(p_rom,
			p_lookup_table_offsets.at(i),
			i < p_lookup_table_sizes.size() ? p_lookup_table_sizes[i] : 255
		);

		for (std::size_t j{ 0 }; j < p_frames_per_object; ++j)
			l_objanim.m_frames.push_back(fg::AnimationFrame(
				p_rom,
				p_anim_frame_offsets.at(i * p_frames_per_object + j)
			));

		p_coll.m_obj_anims.push_back(l_objanim);
	}

}

std::vector<klib::NES_tile> fg::GraphicsManager::extract_nes_tiles(const std::vector<byte>& p_rom,
	std::size_t p_offset, std::size_t p_tile_count) const {

	std::vector<klib::NES_tile> result;
	for (std::size_t i{ 0 }; i < p_tile_count; ++i)
		result.push_back(klib::NES_tile(p_rom, p_offset + 16 * i));

	return result;
}

std::vector<std::vector<byte>> fg::GraphicsManager::extract_palette(const std::vector<byte>& p_rom,
	std::size_t p_offset) const {
	std::vector<std::vector<byte>> result;

	for (std::size_t i{ 0 }; i < 4; ++i) {
		std::vector<byte> subpal;
		for (std::size_t j{ 0 }; j < 4; ++j)
			subpal.push_back(p_rom.at(p_offset + 4 * i + j));
		result.push_back(subpal);
	}

	return result;
}

std::vector<std::size_t> fg::GraphicsManager::extract_lookup_table_ff_delim(
	const std::vector<byte>& p_rom,
	std::size_t p_offset,
	std::size_t p_max_size) const {

	std::vector<std::size_t> result;

	for (std::size_t i{ 0 }; p_rom.at(p_offset + i) != 0xff &&
		result.size() < p_max_size; ++i)
		result.push_back(p_rom.at(p_offset + i));

	return result;
}
