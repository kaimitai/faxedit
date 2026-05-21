#ifndef FE_MAINWINDOW_H
#define FE_MAINWINDOW_H

#include <SDL3/SDL.h>
#include <deque>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include "gfx.h"
#include "UndoInterface.h"
#include "ClipBoardManager.h"
#include "./../fe/EditorSettings.h"
#include "./../fe/sprite/SpriteGfxSnapshotManager.h"
#include "./../fe/Config.h"
#include "./../fe/Game.h"
#include "./../fe/ROM_Manager.h"
#include "./../fi/IScriptLoader.h"
#include "./../common/imgui/imgui.h"
#include "./../common/imgui/imgui_impl_sdl3.h"
#include "./../common/imgui/imgui_impl_sdlrenderer3.h"

namespace fe {

	enum EditMode { TilemapEditMode, Sprites, Doors, Scrolling, Transitions, Other };
	enum ChrPickerMode { Default, HUD, All };
	enum GfxEditMode { WorldChr, BgGraphics, WorldPalettes, GfxPalettes, HUDAttributes, WorldChrBank, GfxChrBank };
	enum class SpriteGfxEditMode { Settings, Portraits, NPC, Player };
	enum class CinematicEditMode { Player, Ripples, Waterfall, Palette, AnimationFrames, Settings };

	struct Size4 {
		std::size_t x, y, w, h;
	};

	struct MainCache {
		// labels
		std::map<byte, std::string> m_labels_cmd_byte,
			m_labels_door_reqs, m_labels_block_props,
			m_labels_palettes, m_labels_spec_sprite_sets,
			m_labels_music, m_labels_buildings;
		std::vector<std::string> m_labels_worlds, m_labels_sprites,
			m_labels_tilesets;
		// counts
		std::size_t m_sprite_count, m_iscript_count, m_music_count,
			m_command_byte_count;
		// palettes shared between worlds and game gfx images
		std::map<std::size_t, std::string> m_shared_palettes;
		// flags
		bool m_disable_pal2_mus;
		// sprite dimensions; holding sprite size and cartesian offsets per animation frame
		std::vector<fe::SpriteAnimationGUIData> m_sprite_dims;
		// iscripts
		std::map<std::size_t, std::vector<fi::AsmToken>> m_iscripts;
		std::map<byte, byte> m_spawn_to_script_no;
	};

	struct Message {
		std::string text;
		int status; // 0=neutral, 1=good, 2=bad
	};

	class MainWindow {

		// config
		fe::Config m_config;
		// tilemap undo interface
		std::optional<fe::UndoInterface> m_undo;
		fe::SpriteGfxSnapshotManager m_sprite_snap_manager;
		// settings
		fe::EditorSettings m_settings;
		// cache
		fe::MainCache m_cache;
		// file info
		std::filesystem::path m_path;
		std::string m_filename, m_loaded_rom_path, m_region_override;

		// selectors
		// TODO: Move to separate struct, and turn suitable vars into local statics instead of class members
		std::size_t
			// world, screen
			m_sel_chunk, m_sel_screen,
			// screen objects
			m_sel_door, m_sel_sprite,
			// tilemap editor
			// select a tile or a region
			m_sel_tile_x, m_sel_tile_y,
			m_sel_tile_x2, m_sel_tile_y2,
			// metatile select editor
			m_sel_metatile,
			// npc bundles aka building parameters
			m_sel_npc_bundle, m_sel_npc_bundle_sprite,
			// selected iscript
			m_sel_iscript,
			// gfx selectors
			m_sel_gfx_ts_world,
			m_sel_gfx_ts_screen;
		// clipboard
		fe::ClipboardManager m_clip_manager;
		// rendering options
		bool m_iscript_window, m_iscript_win_set_focus,
			m_gfx_window, m_sprite_gfx_window, m_cinematic_window;

		fe::ChrTilemap m_hud_tilemap;

		// world tileset cache (for all 256 ppu tiles, not only tileset tiles)
		std::vector<std::vector<klib::NES_tile>> world_ppu_tilesets;

		fe::EditMode m_emode;
		fe::GfxEditMode m_gfx_emode;
		std::size_t m_atlas_tileset_no, m_atlas_palette_no,
			m_atlas_new_tileset_no, m_atlas_new_palette_no;
		bool m_atlas_force_update;
		std::optional<fe::Game> m_game;
		fe::gfx m_gfx;
		std::deque<fe::Message> m_messages;
		fe::ROM_Manager m_rom_manager;

		// oscillating color for selected object
		SDL_Color m_pulse_color;
		float m_pulse_time, m_anim_time;
		std::size_t m_anim_frame;

		// application exit handler variables
		bool m_exit_app_requested, m_exit_app_granted;

		void imgui_text(const std::string& p_str) const;
		void regenerate_atlas_if_needed(SDL_Renderer* p_rnd);
		void load_rom(SDL_Renderer* p_rnd, const std::string& p_filepath,
			const std::string& p_region = std::string());
		std::pair<std::string, std::string> get_config_file_paths(void) const;
		int load_external_rom_data(const std::vector<byte>& p_bytes, bool p_initial);
		void cache_config_variables(void);

		std::string get_ips_path(void) const;
		std::string get_xml_path(void) const;
		std::string get_nes_path(void) const;
		std::string get_filepath(const std::string& p_ext, bool p_add_out = false) const;
		std::string get_settings_xml_path(void) const;

		std::string get_description(byte p_index, const std::map<byte, std::string>& p_map) const;
		std::string get_description(byte p_index, const std::vector<std::string>& p_vec) const;
		std::string get_sprite_label(std::size_t p_sprite_id) const;

		void draw_metatile_info(std::size_t p_sel_chunk, std::size_t p_sel_screen,
			std::size_t p_sel_x, std::size_t p_sel_y);

		void generate_world_tilesets(void);
		void draw_control_window(SDL_Renderer* p_rnd);
		void draw_screen_tilemap_window(SDL_Renderer* p_rnd);
		void draw_metadata_window(SDL_Renderer* p_rnd);
		void draw_iscript_window(SDL_Renderer* p_rnd);
		void draw_filepicker_window(SDL_Renderer* p_rnd);
		void draw_exit_app_window(SDL_Renderer* p_rnd);
		void draw_gfx_window(SDL_Renderer* p_rnd);
		void draw_sprite_gfx_window(SDL_Renderer* p_rnd);
		void show_gfx_chr_bank_screen(SDL_Renderer* p_rnd);
		void show_world_chr_bank_screen(SDL_Renderer* p_rnd);
		void draw_cinematic_window(SDL_Renderer* p_rnd);

		void show_output_messages(void) const;

		void add_message(const std::string& p_msg, int p_status = 0, bool p_allow_repeat = false);

		std::optional<byte> show_screen_scroll_section(const std::string& p_direction,
			std::size_t p_screen_count, std::optional<byte> p_scroll_data);

		std::optional<std::pair<byte, byte>> show_position_slider(byte p_x, byte p_y);
		void show_sprite_screen(fe::Sprite_set& p_sprites, std::size_t& p_sel_sprite);
		void show_sprite_npc_bundle_screen(void);
		void show_stage_door_data(fe::Door& p_door);
		void show_stages_data(void);
		void show_screen_scroll_data(void);

		void show_mt_definition_tab(SDL_Renderer* p_rnd, fe::Chunk& p_chunk);

		void show_sprite_set_contents(std::size_t p_sprite_set);
		void show_scene(fe::Scene& p_scene, bool p_show_pos);

		bool check_patched_size(const std::string& p_data_type, std::size_t p_patch_data_size, std::size_t p_max_data_size);

		std::string get_editmode_as_string(void) const;

		Size4 get_selection_dims(void) const;
		void clipboard_copy(void);
		void clipboard_paste(bool l_update_data = true);

		void scroll_button(std::string p_button_text, std::optional<byte> p_scroll_dest);
		void scroll_left_button(const fe::Screen& p_screen);
		void scroll_right_button(const fe::Screen& p_screen);
		void scroll_up_button(const fe::Screen& p_screen);
		void scroll_down_button(const fe::Screen& p_screen);
		void enter_door_button(const fe::Screen& p_screen);
		void transition_sw_button(const fe::Screen& p_screen);
		void transition_ow_button(const fe::Screen& p_screen);

		// logical helper functions
		void set_atlas_update_values(void);

		// screen element draw functions
		void draw_sprites(SDL_Renderer* p_rnd,
			const std::vector<fe::Sprite>& p_sprites,
			std::size_t p_sel_sprite_no) const;

		// helper functions for the bmp import
		void gen_read_only_chr_idx_non_building(std::size_t p_tileset_no,
			std::size_t p_world_no, std::set<std::size_t>& p_idxs) const;
		void gen_read_only_chr_idx_building(std::size_t p_tileset_no,
			std::size_t p_world_no, std::size_t p_screen_no,
			std::set<std::size_t>& p_idxs) const;
		std::set<std::size_t> gen_metatile_usage(std::size_t p_world_no,
			std::size_t p_screen_no,
			std::size_t p_total_metatile_count) const;
		void gen_fixed_building_metatiles(std::size_t p_tileset_no,
			std::set<std::size_t>& p_idxs) const;
		fe::ChrTilemap get_world_mt_tilemap(
			std::size_t p_world_no,
			std::size_t p_screen_no = 0) const;
		std::string get_bmp_path(void) const;
		std::string get_bmp_filename(std::size_t p_gfx_key) const;
		std::string get_bmp_filepath(std::size_t p_gfx_key) const;
		ImVec4 SDL_Color_to_imgui(const SDL_Color& c) const;
		bool show_palette_window(std::size_t p_pal_key, std::vector<byte>& p_palette);
		std::vector<byte> update_pal_bg_idx(std::vector<byte>& p_palette, byte p_nes_pal_idx);

		void initialize_hud_tilemap(void);
		void generate_door_req_gfx(SDL_Renderer* p_rnd);

		// gfx functions for chr logic
		std::pair<std::vector<fe::ChrGfxTile>, std::set<std::size_t>> get_complete_world_tileset_w_metadata(std::size_t p_tileset_no,
			bool p_normalize = false) const;
		std::pair<std::vector<klib::NES_tile>, std::set<std::size_t>> get_world_tileset_w_metadata(std::size_t p_tileset_no) const;
		void set_world_tileset_tiles(SDL_Renderer* p_rnd, std::size_t p_tileset_no,
			const fe::ChrReorderResult& p_result);
		void set_world_tileset_tiles(SDL_Renderer* p_rnd, std::size_t p_tileset_no,
			const std::vector<klib::NES_tile>& p_tiles);
		fe::ChrReorderResult reorder_chr_tiles(const std::vector<klib::NES_tile>& tiles,
			const std::set<std::size_t>& fixed_indexes = std::set<std::size_t>()) const;
		std::string get_chr_folder(void) const;
		std::string get_chr_file_path(const std::string& p_bank_id) const;
		void save_chr(const std::vector<klib::NES_tile>& tiles, const std::string& p_bank_id);
		std::vector<klib::NES_tile> load_chr(const std::string& p_bank_id, std::size_t p_chr_tile_count);

		// sprite gfx functions and procedures
		void show_sprite_gfx_editor(SDL_Renderer* p_rnd,
			std::size_t p_coll, fe::SpriteFrameCollection& p_collection);
		std::string get_sprite_gfx_file_prefix(std::size_t p_gfx_key) const;
		std::string get_sprite_gfx_bank_name(std::size_t p_coll_id, std::size_t p_sel_bank_id,
			std::size_t p_bank_id, std::size_t p_bank_count, std::size_t p_tile_count) const;
		void report_sprite_gfx_patch(const fe::SpriteGfxPatchResult& result);
		std::optional<std::pair<int, int>> imgui_select_tile_image(SDL_Texture* tex, float scale, int& p_sel_x, int& p_sel_y) const;

		// cinematic ui helpers
		void show_cinematic_edit_mode(bool& p_mode);
		void show_cinematic_position(fe::AnimPosition& p_pos) const;
		void show_cinematic_threshold(fe::DepthState& p_pos) const;
		void show_cinematic_velocity(fe::Velocity& p_velocity) const;

		void export_sprite_frame_bmps(const fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			std::size_t p_bank_id);
		void import_sprite_frame_bmps(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			std::size_t p_bank_id);
		void export_sprite_chr_bank(const fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			std::size_t p_bank_id);
		void import_sprite_chr_bank(fe::SpriteFrameCollection& p_coll, std::size_t p_coll_id,
			std::size_t p_bank_id);

		void update_sgfx_result_for_common_bank(fe::SpriteImportResult& import);
		void generate_editor_sprite_gfx(SDL_Renderer* p_rnd);

		// load functions
		void validate_game_data(fe::Game& p_game);
		void validate_spawn_points(fe::Game& p_game);
		void load_xml(SDL_Renderer* p_rnd);

		// save functions
		void save_xml(void);
		void patch_nes_rom(bool p_in_place, bool p_exclude_dynamic = false);
		std::optional<std::vector<byte>> patch_rom(bool p_exclude_dynamic = false);

	public:

		MainWindow(SDL_Renderer* p_rnd,
			const std::string& p_filepath = std::string(),
			const std::string& p_region = std::string());
		void generate_textures(SDL_Renderer* p_rnd);
		void generate_metatile_textures(SDL_Renderer* p_rnd,
			std::size_t p_mt_no = 256);
		void draw(SDL_Renderer* p_rnd);

		void request_exit_app(void);
		bool is_exit_app_granted(void) const;
	};

}

#endif
