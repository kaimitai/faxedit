#!/usr/bin/env python3
"""Rebuild the checked-in Atlas movie 6502 artifacts with cc65."""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "eoe_core/src/fe/AtlasMovieRuntime.s"
STANDALONE_DATA = ROOT / "eoe_core/src/fe/AtlasMovieStandaloneData.inc"
STANDALONE_RELOCS = ROOT / "eoe_core/src/fe/AtlasMovieStandaloneRelocations.inc"
ENGINE_DATA = ROOT / "eoe_core/src/fe/AtlasMovieEngineData.inc"
RUNTIME_CONTRACT_TEST = ROOT / "eoe_core/tests/AtlasMovieRuntimeContract.py"

LINK_BASE = 0x8000
CORE_BASE = 0xA708
CORE_LIMIT = 0xAA83
TAIL_BASE = 0xAD91


def integrated_source(source: str) -> str:
    result = source.replace('.segment "CODE"', '.segment "CORE"', 1)
    result = result.replace(
        "SPLASH_Y      = $068A\n",
        "SPLASH_Y      = $068A\nMV_NATIVE_RETURN = $0698\n", 1)
    result = result.replace(
        ".export AtlasDevPlayMovie, Bundle",
        ".export MovieEnginePlayIntro, MovieEnginePlayOutro\n"
        ".export MovieEnginePlayScript, Bundle", 1)
    result = result.replace(
        "AtlasDevPlayMovie:\n"
        "    jsr LoadByte\n"
        "    tax\n",
        "MovieEnginePlayIntro:\n"
        "    lda #$00\n"
        "    beq MovieEnginePlayNative\n"
        "MovieEnginePlayOutro:\n"
        "    lda #$01\n"
        "MovieEnginePlayNative:\n"
        "    tax\n"
        "    lda #$01\n"
        "    bne MovieEngineSetMode\n"
        "MovieEnginePlayScript:\n"
        "    tax\n"
        "    lda #$00\n"
        "MovieEngineSetMode:\n"
        "    sta MV_NATIVE_RETURN\n", 1)
    result = result.replace(
        "MovieExit:\n    lda MV_EXIT\n",
        "MovieExit:\n"
        "    lda MV_NATIVE_RETURN\n"
        "    beq :+\n"
        "    rts\n"
        ":\n"
        "    lda MV_EXIT\n", 1)
    result = result.replace(
        "SetupAssets:\n",
        ".segment \"TAIL\"\n\nSetupAssets:\n", 1)
    if result == source or "AtlasDevPlayMovie:" in result:
        raise RuntimeError("failed to derive the Shared engine source")
    return result


def run(command: list[str], cwd: Path) -> None:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True,
                            timeout=60, check=False)
    if result.returncode:
        raise RuntimeError(
            f"{Path(command[0]).name} failed with status {result.returncode}:\n"
            + result.stdout + result.stderr)


def tools() -> tuple[str, str]:
    ca65, ld65 = shutil.which("ca65"), shutil.which("ld65")
    if not ca65 or not ld65:
        raise RuntimeError("ca65 and ld65 are required to verify Atlas movie artifacts")
    return ca65, ld65


def assemble_standalone(source: str, base: int) -> bytes:
    ca65, ld65 = tools()
    with tempfile.TemporaryDirectory(prefix="atlas-movie-standalone-") as raw:
        stage = Path(raw)
        (stage / "runtime.s").write_text(source, encoding="utf-8")
        (stage / "runtime.cfg").write_text(
            "MEMORY {\n"
            f"  ROM: start = ${base:04X}, size = ${0xC000 - base:04X}, file = %O, fill = no;\n"
            "}\nSEGMENTS {\n  CODE: load = ROM, type = ro;\n}\n",
            encoding="utf-8")
        run([ca65, "runtime.s", "-o", "runtime.o"], stage)
        run([ld65, "-C", "runtime.cfg", "runtime.o", "-o", "runtime.bin"], stage)
        return (stage / "runtime.bin").read_bytes()


def assemble_shared(source: str) -> tuple[bytes, bytes]:
    ca65, ld65 = tools()
    with tempfile.TemporaryDirectory(prefix="atlas-movie-shared-") as raw:
        stage = Path(raw)
        (stage / "engine.s").write_text(integrated_source(source), encoding="utf-8")
        (stage / "engine.cfg").write_text(
            "MEMORY {\n"
            f"  CORE: start = ${CORE_BASE:04X}, size = ${CORE_LIMIT - CORE_BASE:04X}, file = \"core.bin\", fill = no;\n"
            f"  TAIL: start = ${TAIL_BASE:04X}, size = ${0xC000 - TAIL_BASE:04X}, file = \"tail.bin\", fill = no;\n"
            "}\nSEGMENTS {\n"
            "  CORE: load = CORE, type = ro;\n"
            "  TAIL: load = TAIL, type = ro;\n}\n",
            encoding="utf-8")
        run([ca65, "engine.s", "-o", "engine.o"], stage)
        run([ld65, "-C", "engine.cfg", "engine.o", "-o", "unused.bin"], stage)
        return (stage / "core.bin").read_bytes(), (stage / "tail.bin").read_bytes()


def transformed(blob: bytes, base: int, target: int,
                words: tuple[int, ...], splits: tuple[tuple[int, int], ...]) -> bytes:
    result = bytearray(blob)
    delta = target - base
    for offset in words:
        value = (result[offset] | result[offset + 1] << 8) + delta
        result[offset:offset + 2] = (value & 0xFFFF).to_bytes(2, "little")
    for low, high in splits:
        value = (result[low] | result[high] << 8) + delta
        result[low], result[high] = value & 0xFF, (value >> 8) & 0xFF
    return bytes(result)


def derive_relocations(source: str) -> tuple[bytes, tuple[int, ...],
                                               tuple[tuple[int, int], ...]]:
    bases = (
        LINK_BASE, 0x8001, 0x807f, 0x80ff, 0x8100, 0x8111,
        0x81fe, 0x8200, 0x8fff, 0x9000, 0x9f80, 0xa000,
        0xafff, 0xb000, 0xb7ff, 0xb831, 0xb832,
    )
    builds = tuple(assemble_standalone(source, base) for base in bases)
    if len({len(blob) for blob in builds}) != 1:
        raise RuntimeError("runtime length changes with its link address")
    candidates: list[int] = []
    for offset in range(len(builds[0]) - 1):
        values = tuple(blob[offset] | blob[offset + 1] << 8 for blob in builds)
        if all((values[i] - values[0]) & 0xFFFF == bases[i] - bases[0]
               for i in range(1, len(bases))):
            candidates.append(offset)
    words: list[int] = []
    cursor = -2
    for candidate in candidates:
        if candidate >= cursor + 2:
            words.append(candidate)
            cursor = candidate
    covered = {index for offset in words for index in (offset, offset + 1)}
    changed = {offset for offset in range(len(builds[0]))
               if any(blob[offset] != builds[0][offset] for blob in builds[1:])}
    remaining = sorted(changed - covered)
    splits: list[tuple[int, int]] = []
    while remaining:
        first = remaining.pop(0)
        match: int | None = None
        pair: tuple[int, int] | None = None
        for other in remaining:
            for low, high in ((first, other), (other, first)):
                values = tuple(blob[low] | blob[high] << 8 for blob in builds)
                if all((values[i] - values[0]) & 0xFFFF == bases[i] - bases[0]
                       for i in range(1, len(bases))):
                    match, pair = other, (low, high)
                    break
            if pair:
                break
        if match is None or pair is None:
            raise RuntimeError("unclassified one-byte runtime relocation")
        remaining.remove(match)
        splits.append(pair)
    word_tuple, split_tuple = tuple(words), tuple(splits)
    for base, blob in zip(bases, builds):
        if transformed(builds[0], LINK_BASE, base, word_tuple, split_tuple) != blob:
            raise RuntimeError("derived relocation table does not reproduce linked image")
    return builds[0], word_tuple, split_tuple


def byte_rows(data: bytes) -> str:
    return "\n".join("\t" + ", ".join(f"0x{value:02x}" for value in data[i:i + 16]) + ","
                     for i in range(0, len(data), 16))


def artifacts() -> dict[Path, str]:
    source = SOURCE.read_text(encoding="utf-8")
    run([sys.executable, str(RUNTIME_CONTRACT_TEST)], ROOT)
    standalone, words, splits = derive_relocations(source)
    core, tail = assemble_shared(source)
    if len(standalone) != 1998 or len(core) != 782 or len(tail) != 1235:
        raise RuntimeError(
            f"unexpected runtime sizes: standalone={len(standalone)}, core={len(core)}, tail={len(tail)}")
    standalone_text = (
        "// Generated by util/generate_atlas_movie_runtime.py from AtlasMovieRuntime.s.\n"
        f"// SHA-256: {hashlib.sha256(standalone).hexdigest()}\n"
        + byte_rows(standalone) + "\n")
    reloc_text = (
        "// Generated by util/generate_atlas_movie_runtime.py.\n"
        f"constexpr std::array<std::size_t, {len(words)}> GENERATED_ABSOLUTE_RELOCATIONS{{\n"
        "\t" + ", ".join(map(str, words)) + "\n};\n"
        f"constexpr std::array<std::array<std::size_t, 2>, {len(splits)}> GENERATED_SPLIT_RELOCATIONS{{{{\n"
        + "\n".join(f"\t{{ {low}, {high} }}," for low, high in splits)
        + "\n}};\n")
    engine_text = (
        "// Generated by util/generate_atlas_movie_runtime.py from AtlasMovieRuntime.s.\n"
        f"constexpr std::array<byte, {len(core)}> GENERATED_CORE{{\n"
        + byte_rows(core) + "\n};\n"
        f"constexpr std::array<byte, {len(tail)}> GENERATED_TAIL{{\n"
        + byte_rows(tail) + "\n};\n")
    return {
        STANDALONE_DATA: standalone_text,
        STANDALONE_RELOCS: reloc_text,
        ENGINE_DATA: engine_text,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true",
                        help="replace checked-in artifacts instead of checking them")
    args = parser.parse_args()
    generated = artifacts()
    if args.write:
        for path, content in generated.items():
            path.write_text(content, encoding="utf-8")
            print(f"wrote {path.relative_to(ROOT)}")
        return 0
    stale = []
    for path, content in generated.items():
        if not path.exists() or path.read_text(encoding="utf-8") != content:
            stale.append(str(path.relative_to(ROOT)))
    if stale:
        raise RuntimeError("stale generated Atlas movie artifacts: " + ", ".join(stale))
    print("Atlas movie 6502 artifacts reproduce exactly")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
