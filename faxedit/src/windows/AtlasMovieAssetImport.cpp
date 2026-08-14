#include "MainWindow.h"
#include "AtlasMovieUi.h"
#include "fe/AtlasMovieAssets.h"
#include "fe/AtlasMovieEditor.h"
#include <algorithm>
#include <format>
#include <map>
#include <set>

namespace {

	using namespace fe;
	using namespace fe::atlas_movie;
	using namespace fe::atlas_movie::ui_detail;

}

void fe::MainWindow::import_atlas_game_sprite(AtlasMovieBundle& bundle,
	AtlasMovie& movie, const std::vector<SpriteAnimationFrame>& decoded_frames,
	std::size_t sprite_id, byte x, byte y) {
		if (movie.tracks.size() >= 8) throw std::runtime_error("Atlas movies support at most eight tracks");
		const auto& loader{ *m_atlas_game_sprites };
		if (sprite_id >= loader.animations.size() || loader.animations[sprite_id].empty())
			throw std::runtime_error("Selected gameplay sprite has no usable frames");
		AtlasMovie backup{ movie };
		try {
			std::set<byte> used;
			for (const auto& frame : loader.animations[sprite_id])
				for (const auto& row : frame.tilemap)
					for (const auto& cell : row) if (cell) used.insert(cell->index);
			if (used.empty()) throw std::runtime_error("Selected gameplay sprite has no CHR tiles");
			const auto& source_bank{ loader.banks.at(loader.npc_to_bank_idx.at(sprite_id)) };
			std::size_t destination{ 0 };
			if (const auto* base{ find_asset(movie, AtlasMovieAssetKind::SpriteChr) })
				destination = static_cast<std::size_t>(base->destination) + base->bytes;
			for (const auto& imported : movie.imports)
				if (imported.kind == AtlasMovieImportKind::SpriteChr)
					destination = std::max(destination,
						static_cast<std::size_t>(imported.destination) + imported.data.size());
			destination = (destination + 15) & ~std::size_t(15);
			if (destination / 16 + used.size() > 256)
				throw std::runtime_error("Not enough sprite pattern-table space for this gameplay sprite");
			std::map<byte, byte> remap;
			std::vector<klib::NES_tile> compact;
			for (const byte source : used) {
				if (source >= source_bank.size()) throw std::runtime_error("Gameplay sprite references a missing CHR tile");
				remap[source] = static_cast<byte>(destination / 16 + compact.size());
				compact.push_back(source_bank[source]);
			}
			auto imported_frames{ loader.animations[sprite_id] };
			for (auto& frame : imported_frames)
				for (auto& row : frame.tilemap)
					for (auto& cell : row) if (cell) cell->index = remap.at(cell->index);
			const std::size_t first_frame{ decoded_frames.size() };
			auto all_frames{ decoded_frames };
			all_frames.insert(all_frames.end(), imported_frames.begin(), imported_frames.end());
			if (all_frames.size() > 255) throw std::runtime_error("Imported animation would exceed 255 movie frames");
			movie.imports.push_back({ AtlasMovieImportKind::SpriteChr,
				std::format("sprite {} CHR", sprite_id), static_cast<std::uint16_t>(destination), 0, encode_chr(compact) });
			replace_import(movie, { AtlasMovieImportKind::MetaspriteLibrary,
				"combined movie frames", 0, static_cast<byte>(all_frames.size()), encode_metasprite_library(all_frames) });
			movie.metasprite_count = static_cast<byte>(all_frames.size());
			const auto palette_index{ std::min<std::size_t>(m_settings.coll_palettes.at(0), m_game->m_palettes.size() - 1) };
			auto palette{ resolved_asset_bytes(m_game->m_rom_data, movie, AtlasMovieAssetKind::Palette) };
			palette.resize(32, 0x0f);
			std::copy_n(m_game->m_palettes.at(palette_index).begin(), 16, palette.begin() + 16);
			const auto* palette_asset{ find_asset(movie, AtlasMovieAssetKind::Palette) };
			replace_import(movie, { AtlasMovieImportKind::Palette, "gameplay sprite palette",
				palette_asset ? palette_asset->destination : static_cast<std::uint16_t>(0x3f00), 0, palette });
			auto actor{ default_track(AtlasMovieTrackKind::CounterToggle, static_cast<byte>(first_frame)) };
			actor.x = x; actor.y = y;
			initialize_actor_editor(actor, movie.tracks.size(), std::format("Sprite {}", sprite_id));
			actor.toggle_frames[1] = static_cast<byte>(first_frame + std::min<std::size_t>(1, imported_frames.size() - 1));
			movie.tracks.push_back(std::move(actor));
			m_atlas_movie_sel_track = movie.tracks.size() - 1;
			const auto phase_index{ std::min(m_atlas_movie_sel_phase, movie.phases.size() - 1) };
			movie.phases[phase_index].update_mask |= static_cast<byte>(1u << m_atlas_movie_sel_track);
			movie.phases[phase_index].draw_mask |= static_cast<byte>(1u << m_atlas_movie_sel_track);
			AtlasMovieBundleCodec::validate(bundle);
			m_atlas_movie_dirty = true;
			add_message(std::format("Imported gameplay sprite {}: {} CHR tiles, {} frames",
				sprite_id, compact.size(), imported_frames.size()), 2);
		}
		catch (...) { movie = std::move(backup); throw; }
}

void fe::MainWindow::import_atlas_game_room(AtlasMovieBundle& bundle,
	AtlasMovie& movie, std::size_t world, std::size_t screen) {
		AtlasMovie backup{ movie };
		try {
			auto current_palette{ resolved_asset_bytes(m_game->m_rom_data, movie, AtlasMovieAssetKind::Palette) };
			std::vector<byte> sprite_palette(16, 0x0f);
			if (current_palette.size() >= 32)
				std::copy(current_palette.begin() + 16, current_palette.begin() + 32, sprite_palette.begin());
			const auto room{ convert_game_room(*m_game, world_ppu_tilesets, world, screen, sprite_palette) };
			const auto* chr_asset{ find_asset(movie, AtlasMovieAssetKind::BackgroundChr) };
			const auto* nt_asset{ find_asset(movie, AtlasMovieAssetKind::Nametable) };
			const auto* pal_asset{ find_asset(movie, AtlasMovieAssetKind::Palette) };
			if (!chr_asset || !nt_asset || !pal_asset) throw std::runtime_error("Movie template lacks background assets");
			replace_import(movie, { AtlasMovieImportKind::BackgroundChr,
				std::format("world {} screen {} CHR", world, screen), chr_asset->destination, 0, room.chr });
			replace_import(movie, { AtlasMovieImportKind::Nametable,
				std::format("world {} screen {} nametable", world, screen), nt_asset->destination, 0, room.nametable });
			replace_import(movie, { AtlasMovieImportKind::Palette,
				std::format("world {} screen {} palette", world, screen), pal_asset->destination, 0, room.palette });
			AtlasMovieBundleCodec::validate(bundle);
			m_atlas_movie_dirty = true;
			add_message(std::format("Imported world {} screen {} as an Atlas-owned background ({} CHR bytes)",
				world, screen, room.chr.size()), 2);
		}
		catch (...) { movie = std::move(backup); throw; }
}
