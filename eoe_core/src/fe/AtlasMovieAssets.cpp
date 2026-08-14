#include "AtlasMovieAssets.h"
#include "AtlasMovieLayout.h"
#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <stdexcept>

namespace fe::atlas_movie {

	std::size_t rom_offset(byte p_bank, std::uint16_t p_cpu) {
		if (p_bank == 15 && p_cpu >= 0x8000 && p_cpu < 0xc000)
			p_cpu = static_cast<std::uint16_t>(p_cpu + 0x4000);
		return layout::file_offset(p_bank, p_cpu);
	}

	std::vector<byte> asset_bytes(const std::vector<byte>& p_rom,
		const AtlasMovieAsset& p_asset) {
		const auto offset{ rom_offset(p_asset.bank, p_asset.cpu) };
		if (offset > p_rom.size() || p_asset.bytes > p_rom.size() - offset)
			throw std::runtime_error("Movie asset extends beyond the ROM");
		return { p_rom.begin() + offset, p_rom.begin() + offset + p_asset.bytes };
	}

	const AtlasMovieAsset* find_asset(const AtlasMovie& p_movie,
		AtlasMovieAssetKind p_kind) {
		const auto found{ std::ranges::find_if(p_movie.assets,
			[p_kind](const auto& asset) { return asset.kind == p_kind; }) };
		return found == p_movie.assets.end() ? nullptr : &*found;
	}

	const AtlasMovieImport* find_import(const AtlasMovie& p_movie,
		AtlasMovieImportKind p_kind) {
		const auto found{ std::ranges::find_if(p_movie.imports,
			[p_kind](const auto& imported) { return imported.kind == p_kind; }) };
		return found == p_movie.imports.end() ? nullptr : &*found;
	}

	void replace_import(AtlasMovie& p_movie, AtlasMovieImport p_import) {
		p_movie.imports.erase(std::remove_if(p_movie.imports.begin(),
			p_movie.imports.end(), [&](const auto& prior) {
				return prior.kind == p_import.kind;
			}), p_movie.imports.end());
		p_movie.imports.push_back(std::move(p_import));
	}

	std::vector<klib::NES_tile> decode_chr(const std::vector<byte>& p_bytes) {
		std::vector<klib::NES_tile> result;
		for (std::size_t offset{ 0 }; offset + 16 <= p_bytes.size(); offset += 16)
			result.emplace_back(p_bytes, offset);
		return result;
	}

	std::vector<byte> encode_chr(const std::vector<klib::NES_tile>& p_tiles) {
		std::vector<byte> result;
		for (const auto& tile : p_tiles) {
			const auto bytes{ tile.to_bytes() };
			result.insert(result.end(), bytes.begin(), bytes.end());
		}
		return result;
	}

	namespace {

		const char* asset_kind_name(AtlasMovieAssetKind p_kind) {
			switch (p_kind) {
			case AtlasMovieAssetKind::SpriteChr: return "sprite CHR";
			case AtlasMovieAssetKind::BackgroundChr: return "background CHR";
			case AtlasMovieAssetKind::Nametable: return "nametable";
			case AtlasMovieAssetKind::Palette: return "palette";
			}
			return "asset";
		}

		bool import_replaces_asset(const AtlasMovie& p_movie,
			AtlasMovieAssetKind p_kind) {
			AtlasMovieImportKind imported_kind{};
			switch (p_kind) {
			case AtlasMovieAssetKind::BackgroundChr:
				imported_kind = AtlasMovieImportKind::BackgroundChr; break;
			case AtlasMovieAssetKind::Nametable:
				imported_kind = AtlasMovieImportKind::Nametable; break;
			case AtlasMovieAssetKind::Palette:
				imported_kind = AtlasMovieImportKind::Palette; break;
			case AtlasMovieAssetKind::SpriteChr:
				return false;
			}
			return find_import(p_movie, imported_kind) != nullptr;
		}

		SpriteAnimationFrame decode_metasprite_record(
			const std::vector<byte>& p_data, std::size_t& p_cursor) {
			if (p_cursor + 4 > p_data.size())
				throw std::runtime_error("Metasprite header is truncated");
			SpriteAnimationFrame frame;
			frame.offset_x = static_cast<std::int8_t>(p_data[p_cursor++]);
			frame.offset_y = static_cast<std::int8_t>(p_data[p_cursor++]);
			const byte width{ p_data[p_cursor++] }, height{ p_data[p_cursor++] };
			if (!width || !height
				|| width > AtlasMovieBundleCodec::METASPRITE_MAX_WIDTH
				|| height > AtlasMovieBundleCodec::METASPRITE_MAX_HEIGHT)
				throw std::runtime_error("Metasprite dimensions are invalid");
			for (byte y{ 0 }; y < height; ++y) {
				std::vector<std::optional<SpriteFrameTile>> row;
				for (byte x{ 0 }; x < width; ++x) {
					if (p_cursor >= p_data.size())
						throw std::runtime_error("Metasprite is truncated");
					const byte tile{ p_data[p_cursor++] };
					if (tile == 0xff) row.push_back(std::nullopt);
					else {
						if (p_cursor >= p_data.size())
							throw std::runtime_error("Metasprite attribute is truncated");
						const byte attr{ p_data[p_cursor++] };
						row.push_back(SpriteFrameTile{ tile,
							static_cast<byte>(attr & 3),
							static_cast<bool>(attr & 0x80),
							static_cast<bool>(attr & 0x40) });
					}
				}
				frame.tilemap.push_back(std::move(row));
			}
			return frame;
		}

	}

	std::vector<AtlasMovieRomSpan> movie_rom_source_spans(
		const std::vector<byte>& p_rom, const AtlasMovieBundle& p_bundle) {
		std::vector<AtlasMovieRomSpan> result;
		auto append = [&](std::size_t offset, std::size_t bytes,
			std::string label) {
			if (offset > p_rom.size() || bytes > p_rom.size() - offset)
				throw std::runtime_error(label + " extends beyond the ROM");
			result.push_back({ offset, bytes, std::move(label) });
		};

		for (const auto& movie : p_bundle.movies) {
			for (const auto& asset : movie.assets) {
				// Replacement imports live inside FMB itself. Sprite imports are
				// additions, so the base sprite-CHR source remains ROM-owned.
				if (import_replaces_asset(movie, asset.kind)) continue;
				append(rom_offset(asset.bank, asset.cpu), asset.bytes,
					movie.id + " " + asset_kind_name(asset.kind));
			}

			if (find_import(movie, AtlasMovieImportKind::MetaspriteLibrary))
				continue;
			const auto lo_offset{ rom_offset(movie.metasprite_bank,
				movie.metasprite_pointer_lo) };
			const auto hi_offset{ rom_offset(movie.metasprite_bank,
				movie.metasprite_pointer_hi) };
			append(lo_offset, movie.metasprite_count,
				movie.id + " metasprite low-pointer table");
			append(hi_offset, movie.metasprite_count,
				movie.id + " metasprite high-pointer table");
			for (std::size_t frame{}; frame < movie.metasprite_count; ++frame) {
				const auto cpu{ static_cast<std::uint16_t>(p_rom.at(lo_offset + frame)
					| p_rom.at(hi_offset + frame) << 8) };
				std::size_t cursor{ rom_offset(movie.metasprite_bank, cpu) };
				const auto start{ cursor };
				(void)decode_metasprite_record(p_rom, cursor);
				append(start, cursor - start, movie.id + " metasprite frame "
					+ std::to_string(frame));
			}
		}
		return result;
	}

	void reject_movie_source_overlaps(const std::vector<byte>& p_rom,
		const AtlasMovieBundle& p_bundle,
		const std::vector<AtlasMovieRomSpan>& p_writes,
		const std::string& p_owner) {
		const auto sources{ movie_rom_source_spans(p_rom, p_bundle) };
		for (const auto& write : p_writes) {
			if (!write.bytes) continue;
			if (write.offset > p_rom.size()
				|| write.bytes > p_rom.size() - write.offset)
				throw std::runtime_error(p_owner + " write extends beyond the ROM");
			for (const auto& source : sources) {
				const bool overlap{ write.offset < source.offset + source.bytes
					&& source.offset < write.offset + write.bytes };
				if (overlap)
					throw std::runtime_error(p_owner + " would overwrite " + source.label);
			}
		}
	}

	void validate_movie_oam_budget(const std::vector<byte>& p_rom,
		const AtlasMovieBundle& p_bundle) {
		for (const auto& movie : p_bundle.movies) {
			const auto frames{ decode_movie_frames(p_rom, movie) };
			std::vector<std::size_t> frame_cells;
			frame_cells.reserve(frames.size());
			for (const auto& frame : frames) {
				std::size_t cells{};
				for (const auto& row : frame.tilemap)
					cells += static_cast<std::size_t>(std::count_if(
						row.begin(), row.end(), [](const auto& cell) {
							return cell.has_value();
						}));
				if (cells > 64)
					throw std::runtime_error("Movie " + movie.id
						+ " contains a metasprite frame over the NES 64-sprite OAM limit");
				frame_cells.push_back(cells);
			}

			std::vector<std::size_t> track_maximums;
			track_maximums.reserve(movie.tracks.size());
			for (const auto& track : movie.tracks) {
				std::size_t maximum{};
				auto include = [&](byte frame) {
					maximum = std::max(maximum, frame_cells.at(frame));
				};
				if (track.kind == AtlasMovieTrackKind::Path)
					for (const auto& stage : track.stage_frames)
						for (const auto frame : stage) include(frame);
				else if (track.kind == AtlasMovieTrackKind::Cyclic)
					for (const auto frame : track.visible_frames) include(frame);
				else
					for (const auto frame : track.toggle_frames) include(frame);
				track_maximums.push_back(maximum);
			}

			for (std::size_t phase_index{};
				phase_index < movie.phases.size(); ++phase_index) {
				std::size_t cells{};
				for (std::size_t track{}; track < track_maximums.size(); ++track)
					if (movie.phases[phase_index].draw_mask & (1u << track))
						cells += track_maximums[track];
				if (cells > 64)
					throw std::runtime_error("Movie " + movie.id + " phase "
						+ std::to_string(phase_index + 1) + " may draw "
						+ std::to_string(cells)
						+ " sprites, exceeding the NES 64-sprite OAM limit");
			}
		}
	}

	std::optional<std::size_t> movie_background_tile(
		const AtlasMovie& p_movie, byte p_nametable_tile) {
		const auto* asset{ find_asset(p_movie,
			AtlasMovieAssetKind::BackgroundChr) };
		if (!asset || asset->destination < 0x1000
			|| asset->destination % 16 || asset->bytes % 16)
			return std::nullopt;
		const std::size_t first{
			static_cast<std::size_t>(asset->destination - 0x1000) / 16 };
		const std::size_t count{ static_cast<std::size_t>(asset->bytes) / 16 };
		if (p_nametable_tile < first
			|| static_cast<std::size_t>(p_nametable_tile) >= first + count)
			return std::nullopt;
		return static_cast<std::size_t>(p_nametable_tile) - first;
	}

	std::vector<SpriteAnimationFrame> decode_movie_frames(
		const std::vector<byte>& p_rom, const AtlasMovie& p_movie) {
		std::vector<SpriteAnimationFrame> result;
		if (const auto* imported{ find_import(
			p_movie, AtlasMovieImportKind::MetaspriteLibrary) }) {
			std::size_t cursor{ static_cast<std::size_t>(imported->aux) * 2 };
			for (byte i{ 0 }; i < imported->aux; ++i)
				result.push_back(decode_metasprite_record(imported->data, cursor));
			if (cursor != imported->data.size())
				throw std::runtime_error("Imported metasprites have trailing bytes");
			return result;
		}
		const auto lo_offset{ rom_offset(
			p_movie.metasprite_bank, p_movie.metasprite_pointer_lo) };
		const auto hi_offset{ rom_offset(
			p_movie.metasprite_bank, p_movie.metasprite_pointer_hi) };
		if (lo_offset + p_movie.metasprite_count > p_rom.size()
			|| hi_offset + p_movie.metasprite_count > p_rom.size())
			throw std::runtime_error("Metasprite pointer table extends beyond the ROM");
		for (std::size_t i{ 0 }; i < p_movie.metasprite_count; ++i) {
			const auto cpu{ static_cast<std::uint16_t>(p_rom[lo_offset + i]
				| (p_rom[hi_offset + i] << 8)) };
			std::size_t cursor{ rom_offset(p_movie.metasprite_bank, cpu) };
			result.push_back(decode_metasprite_record(p_rom, cursor));
		}
		return result;
	}

	std::vector<byte> encode_metasprite_library(
		const std::vector<SpriteAnimationFrame>& p_frames) {
		if (p_frames.empty() || p_frames.size() > 255)
			throw std::runtime_error("A movie needs 1..255 metasprite frames");
		std::vector<byte> result(p_frames.size() * 2, 0);
		for (const auto& frame : p_frames) {
			if (!frame.w() || !frame.h()
				|| frame.w() > AtlasMovieBundleCodec::METASPRITE_MAX_WIDTH
				|| frame.h() > AtlasMovieBundleCodec::METASPRITE_MAX_HEIGHT)
				throw std::runtime_error(
					"Gameplay sprite has unsupported frame dimensions");
			result.insert(result.end(), {
				static_cast<byte>(frame.offset_x), static_cast<byte>(frame.offset_y),
				static_cast<byte>(frame.w()), static_cast<byte>(frame.h()) });
			std::size_t visible_cells{};
			for (const auto& row : frame.tilemap)
				for (const auto& cell : row) {
					if (!cell) result.push_back(0xff);
					else {
						++visible_cells;
						result.insert(result.end(), {
							static_cast<byte>(cell->index),
							static_cast<byte>((cell->sub_palette & 3)
								| (cell->h_flip ? 0x40 : 0)
								| (cell->v_flip ? 0x80 : 0)) });
					}
				}
			if (visible_cells > 64)
				throw std::runtime_error(
					"Gameplay sprite frame exceeds the NES 64-sprite OAM limit");
		}
		return result;
	}

	std::vector<byte> resolved_asset_bytes(const std::vector<byte>& p_rom,
		const AtlasMovie& p_movie, AtlasMovieAssetKind p_kind) {
		AtlasMovieImportKind import_kind{};
		switch (p_kind) {
		case AtlasMovieAssetKind::BackgroundChr:
			import_kind = AtlasMovieImportKind::BackgroundChr; break;
		case AtlasMovieAssetKind::Nametable:
			import_kind = AtlasMovieImportKind::Nametable; break;
		case AtlasMovieAssetKind::Palette:
			import_kind = AtlasMovieImportKind::Palette; break;
		default: {
			const auto* asset{ find_asset(p_movie, p_kind) };
			if (!asset) throw std::runtime_error("Movie asset is missing");
			return asset_bytes(p_rom, *asset);
		}
		}
		if (const auto* imported{ find_import(p_movie, import_kind) })
			return imported->data;
		const auto* asset{ find_asset(p_movie, p_kind) };
		if (!asset) throw std::runtime_error("Movie asset is missing");
		return asset_bytes(p_rom, *asset);
	}

	std::vector<klib::NES_tile> movie_sprite_tiles(
		const std::vector<byte>& p_rom, const AtlasMovie& p_movie) {
		std::vector<klib::NES_tile> result(256);
		const auto* base{ find_asset(p_movie, AtlasMovieAssetKind::SpriteChr) };
		if (!base) throw std::runtime_error("Movie sprite CHR is missing");
		auto insert = [&](std::uint16_t destination,
			const std::vector<byte>& bytes) {
			const auto tiles{ decode_chr(bytes) };
			const std::size_t start{ static_cast<std::size_t>(destination) / 16 };
			if (start + tiles.size() > result.size())
				throw std::runtime_error("Movie sprite CHR crosses $0FFF");
			std::copy(tiles.begin(), tiles.end(), result.begin() + start);
		};
		insert(base->destination, asset_bytes(p_rom, *base));
		for (const auto& imported : p_movie.imports)
			if (imported.kind == AtlasMovieImportKind::SpriteChr)
				insert(imported.destination, imported.data);
		return result;
	}

	ImportedRoom convert_game_room(const Game& p_game,
		const std::vector<std::vector<klib::NES_tile>>& p_tilesets,
		std::size_t p_world, std::size_t p_screen,
		const std::vector<byte>& p_sprite_palette) {
		const auto& chunk{ p_game.m_chunks.at(p_world) };
		const auto& screen{ chunk.m_screens.at(p_screen) };
		const auto tileset_no{ p_game.get_default_tileset_no(p_world, p_screen) };
		const auto palette_no{ p_game.get_default_palette_no(p_world, p_screen) };
		const auto& source_tiles{ p_tilesets.at(tileset_no) };
		std::vector<klib::NES_tile> compact;
		std::vector<byte> remap(256, 0xff);
		ImportedRoom result;
		result.nametable.assign(1024, 0);
		for (std::size_t my{ 0 }; my < 13; ++my)
			for (std::size_t mx{ 0 }; mx < 16; ++mx) {
				const auto& mt{ chunk.m_metatiles.at(
					screen.m_tilemap.at(my).at(mx)) };
				for (std::size_t ty{ 0 }; ty < 2; ++ty)
					for (std::size_t tx{ 0 }; tx < 2; ++tx) {
						const byte source{ mt.m_tilemap.at(ty).at(tx) };
						if (source >= source_tiles.size())
							throw std::runtime_error(
								"Room references a missing CHR tile");
						if (remap[source] == 0xff) {
							if (compact.size() >= 128)
								throw std::runtime_error(
									"Room uses over 128 unique tiles; Atlas background slot is full");
							remap[source] = static_cast<byte>(compact.size());
							compact.push_back(source_tiles[source]);
						}
						result.nametable[(my * 2 + ty) * 32 + mx * 2 + tx]
							= static_cast<byte>(0x80 + remap[source]);
					}
			}
		for (std::size_t ay{ 0 }; ay < 8; ++ay)
			for (std::size_t ax{ 0 }; ax < 8; ++ax) {
				byte attribute{ 0 };
				for (std::size_t qy{ 0 }; qy < 2; ++qy)
					for (std::size_t qx{ 0 }; qx < 2; ++qx) {
						const std::size_t mx{ ax * 2 + qx }, my{ ay * 2 + qy };
						byte value{ 0 };
						if (mx < 16 && my < 13) {
							const auto& mt{ chunk.m_metatiles.at(
								screen.m_tilemap.at(my).at(mx)) };
							value = static_cast<byte>(mt.m_attr_tl & 3);
						}
						attribute |= static_cast<byte>(
							value << ((qy * 2 + qx) * 2));
					}
				result.nametable[960 + ay * 8 + ax] = attribute;
			}
		result.chr = encode_chr(compact);
		result.palette = p_game.m_palettes.at(palette_no);
		result.palette.insert(result.palette.end(),
			p_sprite_palette.begin(), p_sprite_palette.end());
		result.palette.resize(32,
			result.palette.empty() ? 0x0f : result.palette.front());
		return result;
	}

}
