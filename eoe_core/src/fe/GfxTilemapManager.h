#ifndef FE_TILEMAP_MANAGER_H
#define FE_TILEMAP_MANAGER_H

#include <array>
#include <string>
#include <vector>
#include <map>
#include "common/klib/NES_tile.h"
#include "ChrStructures.h"

using byte = unsigned char;
using GfxTilemap = std::vector<std::vector<byte>>;
using GfxAttrmap = std::vector<std::vector<byte>>;
using GfxPalette = std::vector<byte>;

namespace fe {

	// type used for chr tile banks
	struct ChrTileBank {
		std::vector<klib::NES_tile> tiles;
		klib::NESTileFormat format{ klib::NESTileFormat::TwoBPP };
	};

	// type used for canonicalization of chr banks
	struct ChrReorderResult {
		std::vector<klib::NES_tile> tiles;
		std::vector<std::size_t> idx_old_to_new;
	};

	// struct of tilemaps we store - chr data is separate
	struct GfxTilemapData {
		GfxTilemap tilemap;
		GfxAttrmap attrmap;
		GfxPalette palette;
	};

	struct GfxTilemapMetadata {
		std::string m_label, m_chr_bank_id;
		std::size_t m_gfx_key, mt_x, mt_y, mt_w, mt_h;
		std::size_t m_rom_offset_tilemap,
			m_rom_offset_attr,
			m_rom_offset_pal,
			m_chr_ppu_index;

		bool m_patch_attributes, m_patch_palette, m_fix_tile_0,
			m_add_alphanumeric;
	};

	struct IntroOutroLayout {
		std::size_t intro_offset, outro_offset, intro_chr_count, outro_chr_count,
			intro_ppu_index, outro_ppu_index;
		std::string intro_chr_bank, outro_chr_bank;
	};

	enum class ScriptFontVariant { None, LatinSmall, LatinBig, Japanese };
	enum class IntroOutroChrType { Normal, Expanded };

	class Config;

	struct GfxTilemapManager {

		std::map<std::string, ChrTileBank> chrbanks;
		std::map<std::string, std::size_t> chrbank_rom_offsets;
		std::map<std::string, GfxTilemapMetadata> metadata;
		std::map<std::string, GfxTilemapData> tilemapdata;
		ScriptFontVariant m_font_variant{ ScriptFontVariant::None };
		IntroOutroChrType m_io_chr_type{ IntroOutroChrType::Normal };

		void load_chr_bank(const std::string& p_chr_bank_id,
			const std::vector<byte>& p_rom, std::size_t p_rom_offset,
			std::size_t p_chr_tile_count, klib::NESTileFormat p_format = klib::NESTileFormat::TwoBPP);
		void load_chr_metadata(const std::string& p_gfx_id,
			const std::string& p_label, const std::string& p_chr_bank_id,
			std::size_t p_gfx_numeric_key,
			std::size_t p_mt_x, std::size_t p_mt_y,
			std::size_t p_mt_w, std::size_t p_mt_h,
			std::size_t p_rom_offset_tilemap,
			std::size_t p_rom_offset_attr, std::size_t p_rom_offset_pal,
			std::size_t p_chr_ppu_index,
			bool p_patch_attributes, bool p_patch_palette,
			bool p_fix_tile_0, bool p_add_alphanumeric);

		void load_tilemap_data_from_rom(const std::vector<byte>& p_rom, const std::string& p_gfx_id);

		void load_rom_tilemap(const std::vector<byte>& p_rom, const std::string& p_gfx_id);
		void load_rom_attribute_map(const std::vector<byte>& p_rom, const std::string& p_gfx_id);
		void load_rom_palette(const std::vector<byte>& p_rom, const std::string& p_gfx_id);

		void verify_chr_gfx_id(const std::string& p_gfx_id) const;

		std::vector<klib::NES_tile> get_complete_chr_tileset(const std::string& p_gfx_id) const;
		std::vector<fe::ChrGfxTile> get_complete_chr_tileset_w_md(const std::string& p_gfx_id,
			bool p_determine_fixed = true) const;
		std::vector<fe::ChrGfxTile> get_complete_bank_chr_tileset_w_md(const std::string& p_bank_id) const;
		bool is_bank_tile_0_fixed(const std::string& p_bank_id) const;
		IntroOutroLayout get_intro_outro_layout(const fe::Config& p_config, bool p_expanded) const;
		void assign_intro_outro_offsets(const fe::Config& p_config, bool p_expanded);

		// rom patching
		void patch_chr_banks(std::vector<byte>& p_rom) const;
		void patch_tilemap_data(std::vector<byte>& p_rom) const;
		std::vector<byte> serialize_attr_table(const std::vector<std::vector<byte>>& p_attr_table) const;

	public:
		GfxTilemapManager(void) = default;
		void initialize(const fe::Config& p_config,
			const std::vector<byte>& p_rom);
		std::size_t get_gfx_numeric_key(const std::string& p_gfx_id) const;
		std::string get_label(const std::string& p_gfx_id) const;

		void apply_expanded_intro_outro_chr(const fe::Config& p_config);
		void apply_canonicalization(const std::string& p_bank_id, const fe::ChrReorderResult& result);

		// rom patching
		void patch_rom(std::vector<byte>& p_rom) const;

		// xml procedures
		void set_chr_bank(const std::string& p_chr_bank_id,
			const std::vector<klib::NES_tile>& p_tiles);
		void set_chr_bank_format(const std::string& p_chr_bank_id,
			klib::NESTileFormat p_format);
		void set_tilemap(const std::string& p_gfx_id,
			const std::vector<std::vector<byte>>& p_tilemap);
		void set_attribute_table(const std::string& p_gfx_id,
			const std::vector<std::vector<byte>>& p_attribute_table);
		void set_palette(const std::string& p_gfx_id,
			const std::vector<byte>& p_palette);

		// gfx import and export helpers
		void commit_import(const std::string& p_gfx_id, const fe::ChrTilemap& p_result);
		std::vector<byte>& get_bg_palette(const std::string& p_gfx_id);

		// convert to the type of tilemap object the gfx renderer requires
		fe::ChrTilemap get_chrtilemap(const std::string& p_gfx_id) const;

		// helpers
		bool is_palette_dynamic(const std::string& p_gfx_id) const;
		bool is_attr_table_dynamic(const std::string& p_gfx_id) const;
		std::string get_tilemap_chr_bank_id(const std::string& p_gfx_id) const;
	};

	namespace c {
		// chr banks not tied to a particular tilemap
		constexpr char CHR_BANK_HUD[]{ "chr_hud" };
		constexpr std::size_t CHR_BANK_HUD_ALPHA_START{ 0x10 };
		constexpr std::size_t CHR_BANK_HUD_NUMERIC_START{ 0x30 };
		constexpr char CHR_BANK_FONT[]{ "chr_font" };
		// dynamic editable chr banks, counts implicit from vector sizes (xml load) or config (ROM load)
		constexpr char CHR_BANK_INTRO_OUTRO[]{ "chr_intro_outro" };
		constexpr char CHR_BANK_INTRO[]{ "chr_intro" };
		constexpr char CHR_BANK_OUTRO[]{ "chr_outro" };
		constexpr char CHR_BANK_TITLE[]{ "chr_title" };
		constexpr char CHR_BANK_ITEMS[]{ "chr_items" };
		// config constants
		constexpr char ID_SCRIPT_FONT_CHR_OFFSET[]{ "script_font_chr_offset" };
		constexpr char ID_SCRIPT_FONT_CHR_COUNT[]{ "script_font_chr_count" };
		constexpr char ID_SCRIPT_FONT_VARIANT[]{ "script_font_variant" };

		constexpr char CHR_GFX_LABEL_TITLE[]{ "Title Screen" };
		constexpr char CHR_GFX_LABEL_INTRO[]{ "Intro Animation Screen" };
		constexpr char CHR_GFX_LABEL_OUTRO[]{ "Outro Animation Screen" };
		constexpr char CHR_GFX_LABEL_ITEMS[]{ "Item Graphics (bg)" };
		constexpr std::size_t CHR_GFX_NUM_ID_TITLE{ 900 };
		constexpr std::size_t CHR_GFX_NUM_ID_INTRO{ CHR_GFX_NUM_ID_TITLE + 1 };
		constexpr std::size_t CHR_GFX_NUM_ID_OUTRO{ CHR_GFX_NUM_ID_INTRO + 1 };
		constexpr std::size_t CHR_GFX_NUM_ID_ITEMS{ CHR_GFX_NUM_ID_OUTRO + 1 };
	}

}

#endif
