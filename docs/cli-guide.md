
# Command-Line Interface

---

A command-line interface (CLI) is a program that is run by typing commands into a terminal instead of using windows, menus, and buttons. It is particularly useful for automation and for performing repeatable operations on project files.

The command-line program is named `eoe-cli` and is included with Echoes of Eolis.

On Windows, open **Command Prompt**, **PowerShell**, or **Windows Terminal**, navigate to the Echoes of Eolis directory, and run commands such as:

```text id="g1j6jw"
eoe-cli.exe xproj faxanadu.nes faxanadu.xml
```

On macOS or Linux, open a terminal and invoke the executable in the corresponding way, for example:

```text id="rj1fms"
./eoe-cli xproj faxanadu.nes faxanadu.xml
```

Running `eoe-cli` without any arguments displays the built-in command reference.

Paths can be absolute or relative to the current terminal directory. If a path contains spaces, surround it with quotes:

```text id="oj3q3f"
eoe-cli.exe xproj "C:\Faxanadu Project\faxanadu.nes" "C:\Faxanadu Project\faxanadu.xml"
```

---

## Table of Contents

* [Usage](#usage)
* [Project Builds](#project-builds)
  * [Skipping Patch Subsystems](#skipping-patch-subsystems)
* [Scripts](#scripts)
  * [IScripts](#iscripts)
  * [BScripts](#bscripts)
  * [MScripts](#mscripts)
  * [Miscellaneous Data](#miscellaneous-data)
* [MML](#mml)
* [ROM Expansion](#rom-expansion)
* [Fog CHR Remapping](#fog-chr-remapping)
* [Common Options](#common-options)
* [Configuration Overrides](#configuration-overrides)
* [Full Build Scripts](#full-build-scripts)
  * [Windows](#windows)
  * [macOS / Linux](#macos--linux)
* [Mantra Tool](#mantra-tool)
  * [Creating a Mantra](#creating-a-mantra)
  * [Decoding and Modifying a Mantra](#decoding-and-modifying-a-mantra)
  * [Options](#options)
  * [Spawn Count](#spawn-count)
  * [Available Values](#available-values)
  * [Example](#example)

---

## Usage

```text
eoe-cli <command> <input> <output> [options]
```

Most commands have both a long name and a shorter alias.

---

## Project Builds

The project commands can extract the editable game data to XML and later rebuild a ROM from it.

```text
eoe-cli xproj faxanadu.nes faxanadu.xml
eoe-cli bproj faxanadu.xml faxanadu-build.nes -s faxanadu.nes
```

`extract-project` (`xproj`) extracts project XML from a ROM.

`build-project` (`bproj`) loads project XML and patches it into a source ROM. Using a clean source ROM with `-s` is recommended, making output ROMs disposable build artifacts.

This makes the CLI suitable for reproducible and automated builds:

```text
faxanadu.nes
faxanadu.xml
eoe_config_override.xml
        │
        ▼
eoe-cli bproj faxanadu.xml build.nes -s faxanadu.nes
        │
        ▼
    build.nes
```

### Skipping Patch Subsystems

Individual parts of a project build can be omitted with `--skip` (`-skip`):

```text
eoe-cli bproj faxanadu.xml build.nes -s faxanadu.nes --skip fog,cinematics
```

Available skip codes are:

```text
bank15_data
bg_gfx
cinematics
fog
jump_on_tiles
mattock_animations
metadata
palettes
push_blocks
scenes
sprite_data
sprite_gfx
stages
tilemaps
world_chr_data
```

`--allow-cin-overflow` (`-aco`) allows cinematic data to grow into iScript region 2. By default, this causes the build to fail.

---

## Scripts

### IScripts

Interaction scripts can be disassembled and assembled directly:

```text
eoe-cli x faxanadu.nes faxanadu-iscript.asm
eoe-cli b faxanadu-iscript.asm build.nes -s faxanadu.nes
```

Commands:

```text
x, extract          Disassemble IScripts from ROM
b, build            Assemble IScripts into ROM
```

Use `--no-shop-comments` (`-p`) to omit generated shop comments during extraction.

---

### BScripts

```text
eoe-cli xb faxanadu.nes faxanadu-bscript.asm
eoe-cli bb faxanadu-bscript.asm build.nes -s faxanadu.nes
```

Commands:

```text
xb, extract-bscript
bb, build-bscript
```

### MScripts

MScript is the game's low-level music representation.

```text
eoe-cli xm faxanadu.nes faxanadu-mscript.asm
eoe-cli bm faxanadu-mscript.asm build.nes -s faxanadu.nes
```

Use `--no-notes` (`-n`) to omit note annotations from disassembly.

### Miscellaneous Data

Miscellaneous strings and constants can also be extracted and rebuilt:

```text
eoe-cli xmisc faxanadu.nes faxanadu-misc.txt
eoe-cli bmisc faxanadu-misc.txt build.nes -s faxanadu.nes
```

---

## MML

Music can be extracted to the higher-level MML format and compiled back into a ROM:

```text
eoe-cli xmml faxanadu.nes faxanadu.mml
eoe-cli bmml faxanadu.mml build.nes -s faxanadu.nes
```

MML can also be converted directly to MIDI or LilyPond:

```text
eoe-cli m2m faxanadu.mml faxanadu
eoe-cli m2l faxanadu.mml faxanadu
```

Music can be converted directly from a ROM as well:

```text
eoe-cli r2m faxanadu.nes faxanadu
eoe-cli r2l faxanadu.nes faxanadu
```

The MIDI and LilyPond output argument is used as an output file prefix.

Use `--lilypond-percussion` (`-lp`) to include a percussion staff in LilyPond output.

---

## ROM Expansion

`expand-rom` (`expand`) expands a supported **vanilla** Faxanadu ROM from 256 KiB of PRG ROM (16 banks) to 512 KiB (32 banks):

```text id="j8u2zp"
eoe-cli expand faxanadu.nes faxanadu-512.nes
```

The expanded ROM uses the SUROM variant of the MMC1 mapper. Echoes of Eolis installs the required bank-switching support and adds another 16 PRG banks. We support expaning the four vanilla regions; `us`, `us-rev-a`, `jp` and `eu`.

> **Warning:** ROM expansion is intended for vanilla ROMs. Do not use `expand` on a ROM that has already been patched or built by Echoes of Eolis unless you know exactly what you're doing. The expansion process installs new bank-switching code into space that is expected to be unused in the vanilla ROM. If that space has already been used by other patches, expansion may overwrite data and corrupt the ROM.

Expansion is normally performed **once when setting up a project**. The resulting 512 KiB ROM then becomes the clean base ROM for that project:

```text id="4pc0zw"
vanilla Faxanadu ROM
        │
        │ eoe-cli expand
        ▼
faxanadu-512.nes
        │
        │ clean project base ROM
        ▼
project builds and editing
```

Expanded ROMs are natively supported by Echoes of Eolis. The editor recognizes the expanded regional variants and can load, edit, and build projects from them normally.

The additional 16 PRG banks provide substantially more space for game data and ROM hacks. Supported Echoes of Eolis features can use this space instead of being limited to the original 256 KiB ROM.

### Expanding an Existing Project

If a project is already in progress, do **not** expand its current patched ROM directly. Instead, export the project's editable source files before moving to an expanded base ROM.

This includes:

* the project XML
* iScript sources
* bScript sources
* music sources
* miscellaneous data sources
* `eoe_config_override.xml`, if used

Then expand a clean vanilla ROM:

```text id="f84r3v"
eoe-cli expand faxanadu.nes faxanadu-512.nes
```

Use `faxanadu-512.nes` as the new clean base ROM and rebuild the project from the exported sources:

```text id="tx05ny"
project XML
scripts
music
configuration override
        │
        ▼
faxanadu-512.nes
        │
        ▼
rebuilt expanded project
```

For CLI-based projects, this follows the same build process described elsewhere in this document. For GUI-based projects, load the expanded ROM and import the previously exported project and script/music sources.

Once the project has been migrated, keep `faxanadu-512.nes` as its clean base ROM. ROMs subsequently produced by the editor or CLI should normally be treated as disposable build artifacts rather than expanded again.

---

### Advanced In-Place Migration

It is technically possible to expand an already patched ROM in place, but this relies on the configured ROM layout and should only be attempted by advanced users. We mention it for completeness' sake.

The expansion routine is 45 bytes long and is installed at the beginning of the last free-space range configured for PRG bank 15. These ranges are defined by the `bank15_free_space` configuration map in `eoe_config.xml`.

The standard `-512` region configurations already reserve these 45 bytes. For example, the normal US bank 15 range begins 45 bytes earlier than the corresponding `us-512` range.

To migrate an existing US project in place:

1. Configure the project to use the corresponding `us-512` region layout, with a configuration override.
2. Load the existing ROM and patch it with Echoes of Eolis. This causes data using the final bank 15 free-space range to be placed after the space reserved for the future bank-switching routine.
3. Reset the configuration override, and run `expand` on the patched ROM.
4. Continue using the resulting ROM as an `us-512` project.

```text id="z7ehx8"
existing US project ROM
        │
        │ patch (using us-512 layout for bank 15 free ranges)
        ▼
45-byte expansion area reserved
        │
        │ eoe-cli expand (using original bank 15 free ranges)
        ▼
expanded US-512 project ROM
```

For custom regions, the same procedure can be used by defining an appropriate final range in `bank15_free_space`, leaving at least 45 contiguous bytes at its beginning for the expansion routine.

This is an advanced migration technique that depends on implementation details of the ROM layout and configuration. Rebuilding the project from exported sources using a newly expanded vanilla ROM remains the recommended and more robust approach.

ROMs that have custom hacks and such in bank 15 free space, like randomizer ROMs, are not supported for expansion with the default configuration. In such cases you need to define your own bank 15 free range such that the first 45 bytes of the last free range are truly safe to use.

---

## Fog CHR Remapping

Faxanadu's fog animation operates on CHR tiles rather than metatiles. The original CHR-tiles for the fog animation have ppu indexes $80, $82, $84 and $86. After importing a bmp or chr-file, users may want to choose another set of 4 chr-tiles to be part of the fog animation.

This command rearranges a tileset and its corresponding metatile definitions so that four selected CHR tiles occupy the slots used by the fog animation.

`remap-fog` (`fog`) rearranges selected tileset CHR tiles into the configured fog animation slots and updates the corresponding metatile references in project XML.

```text
eoe-cli fog faxanadu.nes faxanadu-remapped.xml -s faxanadu.xml -tileset 2 -tiles $90,$a1,$e5,$85
```

This example command will remap tileset `2` so that the following swaps take place:
- $80 and $90
- $82 and $a1
- $84 and $e5
- $86 and $85

Normal world tilesets start at ppu index $80, whereas building tilesets start on later indexes like $90. It is not possible to force a well-defined fog effect for tilesets starting on ppu index $90 or later.

For this command:

* `<input>` is the ROM used for configuration and region detection.
* `-s` specifies the input project XML.
* `<output>` is the resulting project XML.
* `-tileset` selects the tileset to modify.
* `-tiles` supplies the CHR tile indexes to move.

---

## Common Options

| Option                         | Description                                                            |
| ------------------------------ | ---------------------------------------------------------------------- |
| `-r`, `--region`               | Explicitly select a ROM region instead of auto-detection               |
| `-f`, `--force`                | Allow extraction commands to overwrite existing files                  |
| `-s`, `--source-rom`           | Source file used when building instead of using the output file itself |
| `-o`, `--original-size`        | Restrict script assembly to the original ROM allocation                |
| `-p`, `--no-shop-comments`     | Disable IScript shop comments                                          |
| `-n`, `--no-notes`             | Disable MScript note annotations                                       |
| `-lp`, `--lilypond-percussion` | Add percussion to LilyPond output                                      |
| `-skip`, `--skip`              | Skip project ROM patching subsystems                                   |
| `-aco`, `--allow-cin-overflow` | Allow cinematics to grow into iScript region 2                         |

ROM regions are normally detected automatically. `--region` can be used to explicitly select any region defined by the configuration.

---

## Configuration Overrides

The CLI automatically loads `eoe_config_override.xml` when present. This allows projects to define custom opcode maps, general ROM hacks, hack parameters, and other configuration overrides just as they can when using the GUI.

For automated builds, `eoe_config_override.xml` should normally be kept alongside the other project source files in version control as it is part of the project.

---

## Full Build Scripts

For a project using XML, scripts, and MML, a small build script can rebuild the complete ROM in one step.

For example, given this project:

```text id="o6fz7g"
faxanadu.nes
faxanadu.xml
eoe_config_override.xml
faxanadu-scripts/
    faxanadu-iscript.asm
    faxanadu-bscript.asm
    faxanadu-misc.txt
    faxanadu.mml
```

The build starts with the clean `faxanadu.nes`, applies the project XML, and writes `faxanadu-build.nes`. Each following command then adds another part of the project to that output ROM:

```text id="43tjhm"
faxanadu.nes
     │
     ├─ project XML
     ▼
faxanadu-build.nes
     │
     ├─ iScripts
     ├─ bScripts
     ├─ miscellaneous data
     ├─ MML music
     ▼
complete faxanadu-build.nes
```

This means `faxanadu-build.nes` can be deleted at any time and recreated entirely from the clean ROM and project source files.

### Windows

Save the following as `build.bat` in the project directory:

```bat id="vg6bcb"
@echo off
setlocal

set "BASE=faxanadu.nes"
set "OUT=faxanadu-build.nes"
set "SCRIPTS=faxanadu-scripts"

eoe-cli bproj faxanadu.xml "%OUT%" -s "%BASE%" || exit /b 1
eoe-cli b "%SCRIPTS%\faxanadu-iscript.asm" "%OUT%" || exit /b 1
eoe-cli bb "%SCRIPTS%\faxanadu-bscript.asm" "%OUT%" || exit /b 1
eoe-cli bmisc "%SCRIPTS%\faxanadu-misc.txt" "%OUT%" || exit /b 1
eoe-cli bmml "%SCRIPTS%\faxanadu.mml" "%OUT%" || exit /b 1

echo Build complete: %OUT%
```

Run it from Command Prompt with:

```text id="z89r27"
build.bat
```

### macOS / Linux

Save the following as `build.sh` in the project directory:

```bash id="0ywpgs"
#!/usr/bin/env bash
set -e

BASE="faxanadu.nes"
OUT="faxanadu-build.nes"
SCRIPTS="faxanadu-scripts"

./eoe-cli bproj faxanadu.xml "$OUT" -s "$BASE"
./eoe-cli b "$SCRIPTS/faxanadu-iscript.asm" "$OUT"
./eoe-cli bb "$SCRIPTS/faxanadu-bscript.asm" "$OUT"
./eoe-cli bmisc "$SCRIPTS/faxanadu-misc.txt" "$OUT"
./eoe-cli bmml "$SCRIPTS/faxanadu.mml" "$OUT"

echo "Build complete: $OUT"
```

Make the script executable and run it with:

```text id="cvmdva"
chmod +x build.sh
./build.sh
```

Both examples stop immediately if any `eoe-cli` command reports an error. This prevents later build steps from continuing after an earlier part of the ROM failed to build.

The paths in these examples assume `eoe-cli` is available from the project directory. Change `EOE` to the location of the executable if it is installed elsewhere.

---

## Mantra Tool

`eoe-cli` includes a tool for decoding, modifying, and creating Faxanadu mantras (passwords) for the US version of the game. It is invoked with `m`:

```text id="52c5w8"
eoe-cli m [options]
```

### Creating a Mantra

Without an existing mantra, the tool starts with a new game state. Options can then be used to describe the desired state:

```text id="piycyg"
eoe-cli m -r hero -l dartmoor -ew dragonslayer -ea battlesuit
```

By default, the tool prints the generated mantra together with a readable summary of the encoded game state.

Use `-t` to print only the mantra, which is useful in scripts:

```text id="ym62h8"
eoe-cli m -t -r hero -l dartmoor
```

### Decoding and Modifying a Mantra

Use `-m` to start from an existing mantra:

```text id="bgrstj"
eoe-cli m -m <mantra>
```

The decoded game state and mantra will be printed.

Other options can be combined with `-m` to modify parts of an existing game state and generate a new mantra:

```text id="xljg91"
eoe-cli m -m <mantra> -l dartmoor -ew dragonslayer
```

This can be useful for inspecting passwords, creating test states, or making controlled changes to an existing save.

### Options

| Option | Meaning                                            |
| ------ | -------------------------------------------------- |
| `-m`   | Start from an existing mantra                      |
| `-t`   | Print only the resulting mantra                    |
| `-sc`  | Number of spawn locations in the game (default: 8) |
| `-r`   | Rank                                               |
| `-l`   | Location                                           |
| `-ew`  | Equipped weapon                                    |
| `-ea`  | Equipped armor                                     |
| `-es`  | Equipped shield                                    |
| `-em`  | Equipped magic                                     |
| `-ei`  | Equipped item                                      |
| `-sw`  | Stored weapons                                     |
| `-sa`  | Stored armor                                       |
| `-ss`  | Stored shields                                     |
| `-sm`  | Stored magic                                       |
| `-si`  | Stored items                                       |
| `-s`   | Special items                                      |
| `-g`   | Game-state flags (aka "quest flags")               |

Stored equipment, special items, and game-state flags accept comma-separated lists:

```text id="ufwxnr"
eoe-cli m -sw longsword,giantblade -sm fire,death -si mattock,wingboots
```

Names can be abbreviated as long as the abbreviation has only one possible match. For example, `drag` matches `dragonslayer`. Ambiguous abbreviations are rejected.

### Spawn Count

Vanilla Faxanadu has 8 spawn locations. Echoes of Eolis allows spawn locations to be added or removed, which changes the number of bits required to encode the current spawn and therefore changes the mantra format.

The mantra tool assumes 8 spawns by default. When working with a project that has a different number, pass the game's spawn count using `-sc`:

```text id="k3fmd6"
eoe-cli m -sc 12 -m <mantra>
```

The correct spawn count must be used when creating or decoding mantras for that project.

### Available Values

**Ranks**

```text id="14h7py"
novice, aspirant, battler, fighter, adept, chevalier, veteran, warrior,
swordman, hero, soldier, myrmidon, champion, superhero, paladin, lord
```

**Locations**

```text id="jnkz37"
eolis, apolune, forepaw, mascon, victim, conflate, daybreak, dartmoor
```

A numeric location can also be supplied for games containing additional locations.

**Weapons**

```text id="ocg2b4"
handdagger, longsword, giantblade, dragonslayer
```

**Armor**

```text id="z05f4x"
leatherarmor, studdedmail, fullplate, battlesuit
```

**Shields**

```text id="w4g5q2"
smallshield, largeshield, magicshield, battlehelm
```

**Magic**

```text id="ew0qdn"
deluge, thunder, fire, death, tilte, elfring, rubyring, dworfring
```

**Items**

```text id="2lvhk5"
elfring, rubyring, dworfring, demonsring,
ace, king, queen, jack, joker,
mattock, rod, crystal, lamp, hourglass, book, wingboots,
redpotion, blackpotion, elixir, pendant, blackonix, firecrystal,
g0, g1, g2, g3, g4, g5, g6, g7, g8, g9
```

**Special Items**

```text id="phk39a"
elfring, rubyring, dworfring, demonsring,
elixir, magicalrod, pendant, blackonix
```

**Game-State Flags**

```text id="dy3nlj"
u1, u2, masconpath, mattock, wingboots,
dungeonspring, skyspring, towerspring
```

### Example

Options can be combined to construct a specific game state:

```text id="kpyk6z"
eoe-cli m -r paladin -l dartmoor -ew dragonslayer -ea battlesuit -es battlehelm -sm fire,death -si mattock,wingboots -s elfring
```

The mantra tool can therefore be used both as a password decoder and as a convenient way to generate specific game states for gameplay, testing, and ROM development.
