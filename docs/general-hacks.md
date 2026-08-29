<hr>

# General Hacks

<hr>

[Echoes of Eolis](https://github.com/kaimitai/faxedit) ships a library of optional general hacks: self-contained gameplay and engine modifications that are enabled from the configuration and injected into the ROM at build time. Unlike the extended script opcodes, a general hack needs no script changes at all — enabling it is the whole integration.

General hacks are completely optional. A project that enables none of them produces behavior identical to the original game. Each hack verifies its patch sites before writing and refuses the build loudly if the ROM does not match what it expects, so a wrong combination fails at build time rather than at play time.

This document describes the hacks in the current library and their parameters. It assumes you are familiar with the configuration override system described in the [advanced modding documentation](advanced-modding.md).

<hr>

> **Important:** General hacks modify fixed call sites in the ROM. Rebuilding from a previously patched ROM is generally safe if the same hacks remain enabled on every subsequent build. Problems can arise when the enabled hack set changes: for example, building a ROM with hacks A, B and C, then using that patched ROM as the source for a later build with hacks D, E and F. Call sites modified by the first build may then be stale or no longer match what the later build expects.
>
> For this reason, we recommend keeping an unmodified base ROM and treating generated ROMs as transient build outputs. Your project sources - project data xml, ASM and music sources, and configuration overrides - should be the master copy from which ROMs are rebuilt.

<hr>

## Table of Contents

- [Enabling General Hacks](#enabling-general-hacks)
- [The Library](#the-library)
  - [KillSwitch](#killswitch)
  - [SameWorldTransPal2Mus](#sameworldtranspal2mus)
  - [FastStart](#faststart)
  - [QuestFlagItemDrops](#questflagitemdrops)
  - [BossLockedItems](#bosslockeditems)
  - [FlexibleItems](#flexibleitems)
  - [FogRules](#fogrules)
  - [DynamicTilesets](#dynamictilesets)
  - [AtlasDevFrameScheduler](#atlasdevframescheduler)
  - [AtlasDevDayNightCycle](#atlasdevdaynightcycle)

<hr>

## Enabling General Hacks

General hacks are listed in a `general_hacks` string in the configuration, one hack per line, with optional `key=value` parameters after the name. The entry can be scoped to a region with the `region` attribute.

```xml
<strings>
	<string name="general_hacks" region="us">
		KillSwitch
		FastStart gold=2000 ring_of_elf=false
		FlexibleItems price=250
		FogRules rules=0:1+0:3+7
	</string>
</strings>
```

Hacks are installed in the order listed. An unknown hack name or an invalid parameter stops the build with an error. Numeric parameters accept decimal, `$` or `0x` hexadecimal, and `%` binary.

<hr>

## The Library

### KillSwitch

Pressing Select while the game is paused kills the player when the game is unpaused. This gives players a way out of softlocks without resetting the console and losing progress since the last password.

No parameters.

```text
KillSwitch
```

### SameWorldTransPal2Mus

Screen transitions inside the same world apply the palette-to-music rules that normally only run when passing through a door. A transition that changes the area palette can then also change the music, which makes large single-world maps feel like distinct areas.

No parameters.

```text
SameWorldTransPal2Mus
```

### FastStart

Starts a new game with more resources: health and mana start at 80, starting gold is configurable, and the Ring of Elf can be granted from the beginning so the Eolis gate content is open immediately.

| parameter | default | meaning |
| --- | --- | --- |
| `gold` | `1500` | starting gold |
| `ring_of_elf` | `true` | start with the Ring of Elf |

```text
FastStart gold=2000 ring_of_elf=false
```

### QuestFlagItemDrops

The wyvern's mattock and the stone dropper's wing boots normally depend on quest flags, which makes the drops unrepeatable. With this hack the drop check asks whether the player actually has the item in inventory or equipped, so a lost item can be earned again.

| parameter | default | meaning |
| --- | --- | --- |
| `type` | `both` | which drops to change: `both`, `mattock` or `wing_boots` |

```text
QuestFlagItemDrops type=mattock
```

### BossLockedItems

Boss-locked item sprites appear regardless of which boss guards the screen, so custom screens can combine any boss with any locked item. Optionally the item stays hidden until every enemy sprite on the screen has been removed.

| parameter | default | meaning |
| --- | --- | --- |
| `enemies` | `true` | keep the item hidden until all enemies are cleared |

```text
BossLockedItems enemies=false
```

### FlexibleItems

Loosens the vanilla item restrictions in three independent ways: items can be used inside buildings, the player-state gate on item use is removed, and shops will buy any item — items without a sell-table entry sell for a configurable price.

| parameter | default | meaning |
| --- | --- | --- |
| `buildings` | `true` | allow item use inside buildings |
| `state` | `true` | ignore the player-state gate on item use |
| `selling` | `true` | shops buy any item |
| `price` | `100` | sell price for items without a sell-table entry |

```text
FlexibleItems buildings=true state=false price=250
```

### FogRules

Enables the fog effect on arbitrary world and palette combinations while reusing the vanilla fog update routine. Rules are `world:palette` pairs separated by `+`; a bare world number enables fog on every palette in that world. At least one rule is required.

| parameter | default | meaning |
| --- | --- | --- |
| `rules` | none, required | `world:palette` pairs, `+`-separated; a bare world covers the whole world |

```text
FogRules rules=0:1+0:3+0:5+6:3+7
```

### DynamicTilesets

Allows individual screens to override their world's normal tileset. Overrides are given as `world:screen:tileset` entries separated by `+`. Screens without an entry continue to use the tileset selected by the normal game logic.

The lookup code and data are placed with the rest of the hack by default. For ROMs where fixed-bank space is limited, `bank` and `addr` can place them in another PRG bank instead.

The transition hooks are individually configurable. Hooks which are disabled are not installed and their trampolines do not consume ROM space. Entering buildings is disabled by default because buildings already have their own tileset selection mechanism.

| parameter | default | meaning |
| --- | --- | --- |
| `data` | none, required | `world:screen:tileset` entries separated by `+`
| `bank` | 15 | PRG bank containing the lookup code and data table |
| `addr` | none | CPU address of the lookup code when `bank` is not 15 |
| `enter_building` | `false` | apply overrides when entering buildings |
| `exit_building` | `true` | apply overrides when exiting buildings |
| `sameworld` | `true` | apply overrides to same-world doors and screen transitions |
| `start_screen` | `true` | apply an override when loading the starting screen |
| `otherworld` | `true` | apply overrides to otherworld-transitions |
| `stage_doors` | `true` | apply overrides to stage-door transitions |

For normal use, no placement parameters are necessary:

```text
DynamicTilesets data=0:1:6+2:2:5+2:3:5+7:0:5
```

To keep the lookup code and data in another bank:

```text
DynamicTilesets bank=28 addr=0x8000 data=0:1:6+2:2:5+2:3:5+7:0:5
```


Hooks can be disabled when a project does not need those transition types:

```text
DynamicTilesets start_screen=false enter_building=false exit_building=false data=0:1:6+2:2:5
```

DynamicTilesets changes which CHR tileset is loaded; it does not change a world's metatile definitions. Alternate tilesets should therefore use a compatible tile layout for the screens and metatiles that use them. In the vanilla game, the buildings world uses a shared set of metatile definitions, but 3 different tilesets. They partitioned 256 metatiles across the tilesets, and such an approach can be taken when using this feature.

### AtlasDevFrameScheduler

A neutral frame scheduler other hacks build on: an NMI tick with three role slots and a post-deadline lane for work that must run after the frame's last critical PPU write. On its own it changes nothing visible — it exists so per-frame hacks can share one hook instead of each patching the NMI. Role hacks like AtlasDevDayNightCycle require it and refuse to build without it.

The three slots are RAM, so scripts can switch roles on and off at runtime with the AtlasDevArmRole and AtlasDevDayNight opcodes.

No parameters.

```text
AtlasDevFrameScheduler
```

### AtlasDevDayNightCycle

A day and night cycle: the three background palette rows dim from the engine's own palette shadow and return on a configurable day length, with the HUD row untouched. Requires AtlasDevFrameScheduler earlier in the list. Scripts can stop and start the cycle with AtlasDevDayNight or AtlasDevArmRole 2; stopping restores full daylight before going quiet.

| parameter | default | meaning |
| --- | --- | --- |
| `length` | `2048` | frames per full day cycle, multiple of 8 |

```text
AtlasDevDayNightCycle length=7200
```
