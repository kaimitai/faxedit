#include "WorldVisualizer.h"
#include "fe_constants.h"
#include "Config.h"
#include <algorithm>
#include <deque>
#include <string>

fe::WorldVisualizer::WorldVisualizer(
	const std::vector<std::vector<klib::NES_tile>>& p_complete_tilesets) :
	tilesets{ p_complete_tilesets }
{
}

fe::WorldVisualization fe::WorldVisualizer::visualize_world(const fe::Config& p_config,
	const fe::Game& game, std::size_t world_no,
	ScreenId start_screen, const fe::SpriteGUILoader& p_sprites) const {
	std::unordered_set<ScreenId> handled;
	std::unordered_map<ScreenId, ResolvedScreen> resolved;
	std::map<BuildingKey, std::size_t> building_lookup;
	std::deque<PlacementGraph> graphs;
	std::unordered_map<ScreenId, std::vector<DrawCommand>> screenDraw, buildingDraw;
	std::size_t current_door_id{ 0 };

	auto recurse = [&](auto&& self, ScreenId screen, std::size_t palette, IntPosition pos,
		PlacementGraph& graph) -> void {

			// already handled or invalid screen id
			if (handled.contains(screen) ||
				screen >= game.m_chunks.at(world_no).m_screens.size())
				return;

			// placement collision
			if (graph.occupied(pos)) {
				graphs.emplace_back();
				self(self, screen, palette, { 0, 0 }, graphs.back());
				return;
			}

			// place
			handled.insert(screen);

			graph.place(screen, pos);

			resolved[screen] = {
				.pos = pos,
				.palette = palette
			};

			const auto& scr{ game.m_chunks.at(world_no).m_screens.at(screen) };

			// traversal helper
			auto try_scroll = [&](std::optional<ScreenId> dest, IntPosition offs,
				std::optional<PaletteId> pal_override) {
					if (!dest.has_value())
						return;

					self(
						self,
						*dest,
						pal_override.value_or(palette),
						IntPosition(pos.first + offs.first, pos.second + offs.second),
						graph);
				};

			// scrolling
			try_scroll(scr.m_scroll_left, { -1,  0 }, std::nullopt);
			try_scroll(scr.m_scroll_right, { 1,  0 }, std::nullopt);
			try_scroll(scr.m_scroll_up, { 0, -1 }, std::nullopt);
			try_scroll(scr.m_scroll_down, { 0,  1 }, std::nullopt);

			// same-world transitions
			if (scr.m_interchunk_scroll) {
				const auto offs{ get_sw_trans_offset(game.m_chunks.at(world_no), screen) };

				if (offs) {
					self(
						self,
						scr.m_interchunk_scroll->m_dest_screen,
						scr.m_interchunk_scroll->m_palette_id,
						IntPosition(
							pos.first + offs->first,
							pos.second + offs->second),
						graph);
				}
			}

			// doors
			for (const auto& door : scr.m_doors) {
				if (door.m_door_type == fe::DoorType::Building) {

					const BuildingKey bld_key{
						.screen = door.m_dest_screen_id,
						.sprite_set = door.m_npc_bundle
					};

					auto it{ building_lookup.find(bld_key) };

					if (it == end(building_lookup)) {
						const auto idx{ building_lookup.size() };
						it = building_lookup.insert(std::make_pair(bld_key, idx)).first;

						// draw numeric label for the building screen
						buildingDraw[it->second].push_back(DrawCommand{
							.x = 8,	.y = 8,
							.type = DrawCommandType::Number,
							.param = it->second,
							.param_palette = 2
							});
					}

					screenDraw[screen].push_back(DrawCommand{
						.x = 16 * door.m_coords.first,
						.y = 16 * door.m_coords.second,
						.type = DrawCommandType::Number,
						.param = it->second,
						.param_palette = 2
						});

					maybe_add_door_requirement_item_draw(p_config, screenDraw, screen,
						16 * door.m_coords.first,
						16 * door.m_coords.second - 16,
						door.m_requirement);
				}
				else if (door.m_door_type == fe::DoorType::SameWorld) {

					// make a draw command for this door at position
					const auto door_id{ current_door_id++ };

					screenDraw[screen].push_back(DrawCommand{
						.x = 16 * door.m_coords.first,
						.y = 16 * door.m_coords.second,
						.type = DrawCommandType::Number,
						.param = door_id,
						.param_palette = 0
						});

					// draw the requirement gfx if it exists
					const auto& req_items{ p_config.bmap_numeric(c::ID_DOOR_REQ_ITEM_GFX) };

					if (req_items.contains(door.m_requirement)) {
						screenDraw[screen].push_back(DrawCommand{
							.x = 16 * door.m_coords.first,
							.y = 16 * door.m_coords.second - 16,
							.type = DrawCommandType::Item,
							.param = req_items.at(door.m_requirement),
							.param_palette = 0
							});
					}

					maybe_add_door_requirement_item_draw(p_config, screenDraw, screen,
						16 * door.m_coords.first,
						16 * door.m_coords.second - 16,
						door.m_requirement);

					// and the label for the destination
					screenDraw[door.m_dest_screen_id].push_back(DrawCommand{
						.x = 16 * door.m_dest_coords.first,
						.y = 16 * (door.m_dest_coords.second + 1),
						.type = DrawCommandType::Number,
						.param = door_id,
						.param_palette = 1
						});

					if (!handled.contains(door.m_dest_screen_id)) {
						graphs.emplace_back();
						self(self, door.m_dest_screen_id, door.m_dest_palette_id, { 0, 0 }, graphs.back());
					}
				}
			}
		};

	graphs.emplace_back();

	// let's weed out unreachable screens immediately
	const auto l_refs{ game.get_referenced_screens(world_no) };
	for (std::size_t i{ 0 }; i < game.m_chunks.at(world_no).m_screens.size(); ++i)
		if (!l_refs.contains(static_cast<byte>(i)))
			handled.insert(i);

	// initial graph
	recurse(recurse, start_screen, game.get_default_palette_no(world_no, start_screen),
		{ 0, 0 }, graphs.back());

	// unhandled screens
	for (ScreenId s{ 0 }; s < game.m_chunks.at(world_no).m_screens.size(); ++s) {

		if (handled.contains(s))
			continue;

		graphs.emplace_back();

		recurse(
			recurse,
			s,
			game.get_default_palette_no(world_no, s),
			{ 0, 0 },
			graphs.back());
	}

	// flatten disjoint graphs
	std::vector<std::vector<std::optional<ScreenId>>> final_layout;
	std::vector<std::size_t> graph_breaks;

	// determine max width
	std::size_t max_width{ 0 };
	for (const auto& graph : graphs) {
		auto grid{ graph.make_grid() };
		if (!grid.empty())
			max_width = std::max(max_width, grid.front().size());
	}

	max_width = std::max(
		max_width,
		std::min<std::size_t>(BUILDING_GRAPH_WIDTH, building_lookup.size()));

	for (const auto& graph : graphs) {

		auto grid{ graph.make_grid() };

		for (auto& row : grid) {
			row.resize(max_width, std::nullopt);
			final_layout.push_back(std::move(row));
		}

		// separator row
		if (!grid.empty())
			graph_breaks.push_back(final_layout.size());
	}

	// add building screens
	if (!building_lookup.empty()) {
		std::vector<std::optional<ScreenId>> row;

		for (std::size_t idx{ 0 }; idx < building_lookup.size(); ++idx) {

			row.push_back(static_cast<ScreenId>(
				BUILDING_SCREEN_IDX_OFFSET + idx));

			if (row.size() == BUILDING_GRAPH_WIDTH) {
				row.resize(max_width, std::nullopt);
				final_layout.push_back(std::move(row));
				row.clear();
			}
		}

		if (!row.empty()) {
			row.resize(max_width, std::nullopt);
			final_layout.push_back(std::move(row));
		}
	}

	// render tilemaps
	std::unordered_map<ScreenId, Tilemap> tilemaps;
	for (const auto& [screen, info] : resolved)
		tilemaps[screen] = render_screen(game, world_no, screen, info.palette, p_sprites);

	// render building screens
	for (const auto& kv : building_lookup)
		tilemaps[kv.second + BUILDING_SCREEN_IDX_OFFSET] =
		render_screen(game, BUILDING_WORLD_IDX, kv.first.screen,
			game.get_default_palette_no(BUILDING_WORLD_IDX, kv.first.screen),
			game.m_npc_bundles.at(kv.first.sprite_set), p_sprites);

	const auto apply_draw_commands = [&](const auto& draw_map, bool building_idx) -> void {
		for (const auto& kv : draw_map) {

			std::size_t screen_id{ building_idx ?
				kv.first + BUILDING_SCREEN_IDX_OFFSET : kv.first };

			if (!tilemaps.contains(screen_id))
				continue;

			for (const auto& cmd : kv.second) {
				if (cmd.type == DrawCommandType::Number) {
					draw_number_on_tilemap(
						tilemaps.at(screen_id),
						cmd.param,
						tilesets.at(0),
						cmd.param_palette,
						cmd.x,
						cmd.y);
				}
				else if (cmd.type == DrawCommandType::Item) {
					draw_item_on_tilemap(
						tilemaps.at(screen_id),
						game,
						cmd.param,
						cmd.x,
						cmd.y);
				}
			}
		}
		};

	// apply draw commands
	apply_draw_commands(screenDraw, false);
	apply_draw_commands(buildingDraw, true);

	return {
	.layout = std::move(final_layout),
	.tilemaps = std::move(tilemaps),
	.graph_breaks = graph_breaks
	};
}

GfxTilemap fe::WorldVisualizer::render_screen(const fe::Game& p_game,
	std::size_t p_world_no,
	std::size_t p_screen_no,
	std::size_t p_palette_no,
	const fe::SpriteGUILoader& p_sprites) const {
	return render_screen(p_game, p_world_no, p_screen_no, p_palette_no,
		p_game.m_chunks.at(p_world_no).m_screens.at(p_screen_no).m_sprite_set,
		p_sprites);
}

GfxTilemap fe::WorldVisualizer::render_screen(const fe::Game& p_game, std::size_t p_world_no,
	std::size_t p_screen_no,
	std::size_t p_palette_no,
	const fe::Sprite_set& p_sprite_set,
	const fe::SpriteGUILoader& p_sprites) const {

	GfxTilemap tilemap(13 * 16, std::vector<byte>(16 * 16, 0x0));
	const auto& world{ p_game.m_chunks.at(p_world_no) };
	const auto& screen{ world.m_screens.at(p_screen_no) };

	draw_screen_on_tilemap(tilemap, screen,
		world.m_metatiles,
		tilesets.at(p_game.get_default_tileset_no(p_world_no, p_screen_no)),
		p_game.m_palettes.at(p_palette_no)
	);

	for (const auto& sprite : p_sprite_set.m_sprites) {
		const auto& frames{ p_sprites.animations.at(sprite.m_id) };
		std::size_t frame_id{ frames.size() - 1 };

		draw_animation_frame_on_tilemap(tilemap,
			frames.at(frame_id),
			static_cast<int>(16 * sprite.m_x), static_cast<int>(16 * sprite.m_y),
			p_sprites.banks.at(p_sprites.npc_to_bank_idx.at(sprite.m_id)),
			p_game.m_palettes.at(28));
	}

	return tilemap;
}

void fe::WorldVisualizer::draw_pixel_on_tilemap(GfxTilemap& p_tilemap,
	const GfxPalette& p_palette,
	std::size_t p_sub_palette,
	byte p_sub_palette_index,
	std::size_t p_pixel_x, std::size_t p_pixel_y) const {
	p_tilemap.at(p_pixel_y).at(p_pixel_x) = p_palette.at(p_sub_palette * 4 + p_sub_palette_index);
}

void fe::WorldVisualizer::draw_chr_tile_on_tilemap(
	GfxTilemap& p_tilemap,
	const GfxPalette& p_palette,
	std::size_t p_sub_palette,
	const klib::NES_tile& p_chr_tile,
	int p_pixel_x, int p_pixel_y,
	bool p_h_flip,
	bool p_v_flip,
	bool p_sprite) const
{
	const int width{ static_cast<int>(p_tilemap.front().size()) };
	const int height{ static_cast<int>(p_tilemap.size()) };

	for (int y{ 0 }; y < 8; ++y) {
		for (int x{ 0 }; x < 8; ++x) {

			const int dst_x{ p_pixel_x + x };
			const int dst_y{ p_pixel_y + y };

			// clipped
			if (dst_x < 0 || dst_y < 0 ||
				dst_x >= width || dst_y >= height) {
				continue;
			}

			const auto src_x{ p_h_flip ? (7 - x) : x };
			const auto src_y{ p_v_flip ? (7 - y) : y };
			const auto pixel{ p_chr_tile.get_color(src_x, src_y) };

			// sprite palette index 0 is transparent
			if (p_sprite && pixel == 0)
				continue;

			draw_pixel_on_tilemap(
				p_tilemap,
				p_palette,
				p_sub_palette,
				pixel,
				static_cast<std::size_t>(dst_x),
				static_cast<std::size_t>(dst_y)
			);
		}
	}
}

void fe::WorldVisualizer::draw_metatile_on_tilemap(GfxTilemap& p_tilemap,
	const fe::Metatile& p_metatile,
	const std::vector<klib::NES_tile>& p_tiles,
	const GfxPalette& p_palette,
	std::size_t p_mt_x, std::size_t p_mt_y) const {

	for (std::size_t y{ 0 }; y < 2; ++y)
		for (std::size_t x{ 0 }; x < 2; ++x)
			draw_chr_tile_on_tilemap(p_tilemap,
				p_palette,
				p_metatile.get_palette_attribute(p_mt_x, p_mt_y),
				p_tiles.at(p_metatile.m_tilemap.at(y).at(x)),
				static_cast<int>(16 * p_mt_x + 8 * x),
				static_cast<int>(16 * p_mt_y + 8 * y));
}

void fe::WorldVisualizer::draw_screen_on_tilemap(GfxTilemap& p_tilemap,
	const fe::Screen& p_screen,
	const std::vector<fe::Metatile>& p_metatiles,
	const std::vector<klib::NES_tile>& p_tiles,
	const GfxPalette& p_palette) const {
	for (std::size_t y{ 0 }; y < 13; ++y)
		for (std::size_t x{ 0 }; x < 16; ++x)
			draw_metatile_on_tilemap(p_tilemap,
				p_metatiles.at(p_screen.get_mt_at_pos(x, y)),
				p_tiles, p_palette, x, y);
}

void fe::WorldVisualizer::draw_animation_frame_on_tilemap(GfxTilemap& p_tilemap,
	const fe::SpriteAnimationFrame& p_frame,
	int p_pixel_x, int p_pixel_y,
	const std::vector<klib::NES_tile>& p_tiles,
	const GfxPalette& p_palette) const {
	const int base_x{ p_pixel_x + p_frame.offset_x };
	const int base_y{ p_pixel_y + p_frame.offset_y };

	for (std::size_t y{ 0 }; y < p_frame.tilemap.size(); ++y) {

		for (std::size_t x{ 0 };
			x < p_frame.tilemap.at(y).size();
			++x)
		{
			const auto& opt_tile{
				p_frame.tilemap.at(y).at(x)
			};

			if (!opt_tile.has_value())
				continue;

			const auto& tile{ *opt_tile };

			draw_chr_tile_on_tilemap(
				p_tilemap,
				p_palette,
				tile.sub_palette,
				p_tiles.at(tile.index),
				base_x + static_cast<int>(x) * 8,
				base_y + static_cast<int>(y) * 8,
				tile.h_flip,
				tile.v_flip,
				true
			);
		}
	}
}

bool fe::WorldVisualizer::PlacementGraph::occupied(IntPosition p) const {
	return placements.contains(p);
}

ScreenId fe::WorldVisualizer::PlacementGraph::at(IntPosition p) const {
	return placements.at(p);
}

void fe::WorldVisualizer::PlacementGraph::place(ScreenId s, IntPosition p) {
	placements[p] = s;
}

std::vector<std::vector<std::optional<ScreenId>>>
fe::WorldVisualizer::PlacementGraph::make_grid(void) const {
	if (placements.empty())
		return {};

	// bounds
	int min_x{ placements.begin()->first.first };
	int max_x{ placements.begin()->first.first };

	int min_y{ placements.begin()->first.second };
	int max_y{ placements.begin()->first.second };

	for (const auto& [pos, screen] : placements) {

		min_x = std::min(min_x, pos.first);
		max_x = std::max(max_x, pos.first);

		min_y = std::min(min_y, pos.second);
		max_y = std::max(max_y, pos.second);
	}

	// allocate
	const int width{ max_x - min_x + 1 };
	const int height{ max_y - min_y + 1 };

	std::vector<std::vector<std::optional<ScreenId>>> grid(
		height,
		std::vector<std::optional<ScreenId>>(
			width,
			std::nullopt));

	// populate
	for (const auto& [pos, screen] : placements) {

		const int gx{ pos.first - min_x };
		const int gy{ pos.second - min_y };

		grid[gy][gx] = screen;
	}

	return grid;
}

// logical helpers
std::optional<IntPosition> fe::WorldVisualizer::get_sw_trans_offset(const fe::Chunk& p_world,
	std::size_t p_screen_id) const {
	if (p_screen_id >= p_world.m_screens.size())
		return std::nullopt;

	const auto& screen{ p_world.m_screens[p_screen_id] };

	if (!screen.m_interchunk_scroll)
		return std::nullopt;
	else {
		for (std::size_t y{ 0 }; y < screen.m_tilemap.size(); ++y)
			for (std::size_t x{ 0 }; x < screen.m_tilemap[y].size(); ++x) {
				const auto& metatile{ p_world.m_metatiles.at(screen.m_tilemap[y][x]) };

				if (metatile.m_block_property == 0x09 || metatile.m_block_property == 0x0a) {
					if (x == 0)
						return IntPosition(-1, 0);
					else if (x == 15)
						return IntPosition(1, 0);
					else if (y == 0)
						return IntPosition(0, -1);
					else if (y == 12)
						return IntPosition(0, 1);
				}
			}
	}

	return std::nullopt;
}

// draw-command helpers
void fe::WorldVisualizer::draw_number_on_tilemap(GfxTilemap& p_tilemap, std::size_t p_number,
	const std::vector<klib::NES_tile>& p_alphanumeric, byte p_palette,
	int x, int y) const {
	const auto str{ std::to_string(p_number) };

	for (std::size_t i{ 0 }; i < str.size(); ++i) {

		const char c{ str[i] };

		if (c < '0' || c > '9')
			continue;

		const std::size_t chr_idx{ 0x30 + static_cast<std::size_t>(c - '0') };

		draw_chr_tile_on_tilemap(
			p_tilemap,
			DRAW_COMMAND_PALETTES.at(p_palette),
			0,
			p_alphanumeric.at(chr_idx),
			x + static_cast<int>(8 * i),
			y,
			false,
			false,
			false
		);
	}
}

void fe::WorldVisualizer::draw_item_on_tilemap(GfxTilemap& p_tilemap, const fe::Game& p_game,
	std::size_t p_item, int x, int y) const {

	const auto& itemgfx{ p_game.m_gfx_manager.get_chrtilemap(c::CHR_GFX_ID_ITEMS) };

	if (p_item >= itemgfx.m_tilemap.size())
		return;

	const auto& row{ itemgfx.m_tilemap.at(p_item) };

	for (std::size_t mt_x{ 0 }; mt_x < row.size(); ++mt_x) {

		if (!row.at(mt_x).has_value())
			continue;

		const auto& metatile{ row.at(mt_x).value() };

		for (std::size_t ty{ 0 }; ty < 2; ++ty) {
			for (std::size_t tx{ 0 }; tx < 2; ++tx) {

				const auto chr_idx{ metatile.m_idxs.at(ty * 2 + tx) };

				draw_chr_tile_on_tilemap(
					p_tilemap,
					itemgfx.m_palette.at(0),
					0,
					itemgfx.m_tiles.at(chr_idx),
					x + static_cast<int>(mt_x * 16 + tx * 8),
					y + static_cast<int>(ty * 8),
					false,
					false,
					true
				);
			}
		}
	}
}

void fe::WorldVisualizer::maybe_add_door_requirement_item_draw(
	const fe::Config& p_config,
	std::unordered_map<ScreenId, std::vector<DrawCommand>>& p_draw_map,
	ScreenId p_screen, int p_x, int p_y, byte p_requirement) const {

	const auto& req_items{ p_config.bmap_numeric(c::ID_DOOR_REQ_ITEM_GFX) };

	if (!req_items.contains(p_requirement))
		return;

	p_draw_map[p_screen].push_back(DrawCommand{
		.x = p_x,
		.y = p_y,
		.type = DrawCommandType::Item,
		.param = req_items.at(p_requirement),
		.param_palette = 0
		});
}
