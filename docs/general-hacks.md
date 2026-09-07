<hr>

# General Hacks

<hr>

[Echoes of Eolis](https://github.com/kaimitai/faxedit) ships a library of optional general hacks: self-contained gameplay and engine modifications that are enabled from the configuration and injected into the ROM at build time. Unlike the extended script opcodes, a general hack needs no script changes at all — enabling it is the whole integration.

General hacks are completely optional. A project that enables none of them produces behavior identical to the original game. Installers validate their parameters and available output capacity, and newer engine-level installers also verify their hook preimages. Some legacy installers do not yet verify every overwritten byte, so use a ROM compatible with the selected configuration region and do not assume arbitrary pre-patched ROMs can safely compose with every hack.

This document describes the hacks in the current library and their parameters. It assumes you are familiar with the configuration override system described in the [advanced modding documentation](advanced-modding.md).

> **Warning:** General hacks are not automatically removed from an already patched ROM. If you build a ROM with hacks A, B and C, then load that ROM and rebuild it with only hacks D, E and F, hooks and other patches from A, B and C may remain in the ROM.
>
> Keep a clean base ROM and treat patched ROMs as build outputs. Your project XML, config overrides, ASM/music sources and other source files should be considered the authoritative project state.
>
> Rebuilding an already patched ROM is generally safe when you continue installing the same hacks, but changing the selected set of hacks should be done from a clean base ROM.


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
  - [PoisonPickup](#poisonpickup)
  - [SRAM](#sram)
  - [TextSpeed](#textspeed)
  - [AtlasDevFrameScheduler](#atlasdevframescheduler)
  - [AtlasDevDayNightCycle](#atlasdevdaynightcycle)
  - [AtlasDevInfectedTint](#atlasdevinfectedtint)
  - [AtlasDevTimeOfDay](#atlasdevtimeofday)
  - [AtlasDevJumpControl](#atlasdevjumpcontrol)

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

Loosens the vanilla item restrictions in four independent ways: items can be used inside buildings, the player-state gate on item use is removed, and shops will buy any item — items without a sell-table entry sell for a configurable price.

| parameter | default | meaning |
| --- | --- | --- |
| `buildings` | `true` | allow item use inside buildings |
| `wep_indoors` | `false` | allow weapon use inside buildings |
| `state` | `true` | ignore the player-state gate on item use |
| `selling` | `true` | shops buy any item |
| `price` | `100` | sell price for items without a sell-table entry |

```text
FlexibleItems buildings=true state=false price=250 wep_indoors=true
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

The transition hooks are individually configurable. Hooks which are disabled are not installed and their trampolines do not consume ROM space. Entering buildings is disabled by default because buildings already have their own per-screen tileset selection mechanism via the Building Scene objects.

The tileset does not change during normal scrolling transitions, since reloading CHR while the transition is visible would not be seamless.

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

For expanded ROMs, it is reasonable to use `bank=28` and `addr=$8000` unless something else was deliberately put there. By default bank 29 is used for the doubled tileset collection, and bank 30 for dynamic tilemap changes.

Hooks can be disabled when a project does not need those transition types:

```text
DynamicTilesets start_screen=false enter_building=false exit_building=false data=0:1:6+2:2:5
```

DynamicTilesets changes which CHR tileset is loaded; it does not change a world's metatile definitions. Alternate tilesets should therefore use a compatible tile layout for the screens and metatiles that use them. In the vanilla game, the buildings world uses a shared set of metatile definitions, but 3 different tilesets. They partitioned 256 metatiles across the tilesets, and such an approach can be taken when using this feature. Use the advanced **Custom Import Definition** in the GUI to import graphics into specific metatile and tileset CHR ranges.

### PoisonPickup

Changes poison pickups so that they add an item to the player's inventory instead of damaging the player. By default, poison becomes the normally unused black potion (`0x11`).

The original "touched poison" script is still run by default. Set `script=false` to skip it entirely.

| parameter | default | meaning                                             |
| --------- | ------- | --------------------------------------------------- |
| `item`    | `0x11`  | Item ID to add to the inventory                     |
| `sound`   | `0x08`  | Sound effect played when the item is picked up      |
| `script`  | `true`  | Whether to run the original "touched poison" script |

Vanilla item IDs are:

| ID            | item          | notes                                  |
| ------------- | ------------- | -------------------------------------- |
| `0x00`        | Ring of Elf   |                                        |
| `0x01`        | Ruby Ring     |                                        |
| `0x02`        | Ring of Dworf |                                        |
| `0x03`        | Demon's Ring  |                                        |
| `0x04`        | Key A         |                                        |
| `0x05`        | Key K         |                                        |
| `0x06`        | Key Q         |                                        |
| `0x07`        | Key J         |                                        |
| `0x08`        | Key Jo        |                                        |
| `0x09`        | Mattock       |                                        |
| `0x0a`        | Rod           |                                        |
| `0x0b`        | Crystal       |                                        |
| `0x0c`        | Lamp          |                                        |
| `0x0d`        | Hour Glass    |                                        |
| `0x0e`        | Book          |                                        |
| `0x0f`        | Wing Boots    |                                        |
| `0x10`        | Red Potion    |                                        |
| `0x11`        | Black Potion  | Unused in the vanilla game             |
| `0x12`        | Elixir        | Not a normal selectable inventory item |
| `0x13`        | Pendant       | Not a normal selectable inventory item |
| `0x14`        | Black Onix    | Not a normal selectable inventory item |
| `0x15`        | Fire Crystal  | Not a normal selectable inventory item |
| `0x16`-`0x1f` | Glitched      | Not valid normal items                 |

The black potion can be stored and displayed in the inventory, but has no effect in the vanilla game. It can be given a purpose by combining `PoisonPickup` with a hack that adds or overrides item-use behavior.

```text
PoisonPickup item=16 script=false
```

### SRAM

Adds battery-backed SRAM saving. Progress is saved via iScript opcode `ShowMantra`, replacing the original password-based save system. On the start screen, `CONTINUE` is only available when a valid SRAM save is present.

By default, the saved state matches the progress preserved by the vanilla password system, with additional state needed for a complete SRAM save. The saved RAM ranges can be overridden to preserve additional game state if modders want to preserve other RAM ranges.

| parameter | default                                                     | meaning                                                                                             |
| --------- | ----------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `ranges`  | `$039d:42+$042c:2+$0437:1+$0439:1+$04c2:4+$0101:31+$0390:5` | RAM ranges copied to SRAM. Each entry is written as `address:length`, with entries separated by `+` |

Custom save ranges can be supplied if needed, but the default value should be a sensible choice for most:

```text
SRAM
```

The text `CONTINUE` on the start screen is colored gray when no valid save is present, otherwise green-ish, at least with the default start screen palette. See config item `sram_start_screen_attrs` in `eoe_config.xml`. Make an override if you want to change this presentation. A PPU-address is given, followed by attribute bytes to push from that address onward. If you want no coloring, you can make this item an empty string in your config override.

### TextSpeed

Changes the text display speed. Lower masks make text display faster. For regular timing, use `%0` or binary masks consisting only of consecutive `1` bits, such as `%1`, `%11`, `%111`, etc. Other values are valid but may produce uneven text timing.

> Note: Only supported for US, US Rev A and EU ROMs.

| parameter | default | meaning |
| --- | --- | --- |
| `mask` | `%0` | Text speed mask. `%0` is fastest; `%11` is vanilla speed. Higher masks such as `%111` and `%1111` are progressively slower |

```text
TextSpeed mask=%1
```

### AtlasDevFrameScheduler

A neutral frame scheduler other hacks build on: an NMI tick with three role slots and an exclusive post-deadline lane for work that must run after the frame's last critical PPU write. PRE roles run only when both the PPU queue and nametable-strip work are idle. On its own it changes nothing visible — it exists so per-frame hacks can share one hook instead of each patching the NMI. Role hacks like AtlasDevDayNightCycle require it and refuse to build without it.

The three slots are RAM, so scripts can switch roles on and off at runtime with the AtlasDevArmRole and AtlasDevDayNight opcodes. At build time, a boot slot is unclaimed only when its arm byte is zero and its PRE vector still points to the scheduler's default stub. A role installer reuses only a compatible existing kind or claims the first unclaimed slot, refusing without modifying the ROM when none is available. The single POST lane similarly refuses a second claimant.

The current runtime opcodes do not retain persistent kind-to-slot affinity: when arming an inactive kind, they select the first zero RAM slot. Runtime composition is therefore safe only while candidate slots use stub PRE vectors. A future scheduler ABI extension is required before boot-off non-stub PRE roles can reserve a lane across runtime disarm/rearm operations.

No parameters.

```text
AtlasDevFrameScheduler
```

### AtlasDevDayNightCycle

A day and night cycle: the three background palette rows dim from the engine's own palette shadow and return on a configurable day length, with the HUD row untouched. Requires AtlasDevFrameScheduler earlier in the list and exclusive ownership of its POST lane. Scripts can stop and start the cycle with AtlasDevDayNight or AtlasDevArmRole 2; stopping always completes an eight-call full-daylight sweep before going quiet, even when stopped during dawn.

| parameter | default | meaning |
| --- | --- | --- |
| `length` | `2048` | frames per full day cycle, multiple of 8 |

```text
AtlasDevDayNightCycle length=7200
```

### AtlasDevInfectedTint

Tints sprite palette 0 with three configurable colors and a pulse, for a
poisoned or cursed look on the hero. Requires AtlasDevFrameScheduler.
Runs beside other roles on the same scheduler; scripts switch it with
AtlasDevArmRole 3, and switching off restores the palette from the
engine's shadow. When combined with AtlasDevDayNightCycle, list the
tint after it - the tint chains onto a claimed post lane, while the
day cycle demands an unclaimed one.

| parameter | default | meaning |
| --- | --- | --- |
| `colors` | `$09+$19+$29` | three palette values, plus separated |
| `pulse` | `$20` | pulse mask, a power of two; `0` for a steady tint |
| `armed` | `1` | `0` installs it dormant, for scripts to switch on |

```text
AtlasDevInfectedTint colors=$0C+$1C+$2C pulse=0
```

### AtlasDevTimeOfDay

An in-game clock with a two-digit hour readout in the HUD, running as a
role on the AtlasDevFrameScheduler. The readout uses the engine's own
digit convention, so it inherits the HUD font and palette automatically.
Requires AtlasDevFrameScheduler. Like the tint, it chains onto a post
lane another role already holds, so the day cycle, the tint and the
clock can all run in the same frame. Scripts switch it with
AtlasDevArmRole 4; switching off blanks the readout and resets the
clock, so re-arming starts the day at the start hour again.

| parameter | default | meaning |
| --- | --- | --- |
| `hourlength` | `300` | frames per in-game hour |
| `start` | `12` | the hour the clock boots at, `0` to `23` |
| `cell` | `$2038` | nametable address of the two readout cells |

```text
AtlasDevTimeOfDay hourlength=600 start=6 cell=$2038
```


### AtlasDevJumpControl

Coyote time, a jump buffer, a short hop on early release and optional
extra jumps in the air, for the hero's jump. Step off a ledge and A still
jumps for `coyote` frames. Press A up to `buffer` frames before landing
and the jump fires on the first grounded frame. Release A after
`shorthop` ascent phases and the arc comes down along the same curve it
went up, so a tap is a one-tile hop and a hold is the full jump.
`airjumps` extra jumps are allowed in the air, reloaded on landing and
never while the Wing Boots are flying. Each parameter is 0 to 15 and 0
turns that feature off; with all four at 0 nothing is installed.

Three vanilla instructions in the bank 15 jump code are retargeted to
stubs placed with the other general hacks, 152 bytes with the defaults
and 214 with every feature on. The stubs run in the main loop and cost
nothing in vblank. One RAM byte at $04df holds the buffer countdown and
the air jumps left; the fall grace counter at $b1 and bit 1 of $a4 are
used with their vanilla meaning. The sites are identical in the US, US
rev A, EU and JP ROMs; the installer verifies them and refuses a ROM
where they differ. Jumps made with A held are unchanged frame for frame.
Does not require AtlasDevFrameScheduler unless `switchable` is set.

With `switchable=1` the hack becomes a script controlled client of
AtlasDevFrameScheduler, which must then appear earlier in the list. It
claims kind 6 in the next free boot slot, every stub runs only while some
slot holds that kind, and `AtlasDevArmRole 6, 0` switches jump control off
mid game while `AtlasDevArmRole 6, 1` switches it back on; switching off
also clears its RAM byte. `armed=0` installs it off until a script arms it.
This costs 82 bytes of gates and, while armed, the tick's call into the
slot's stub vector every frame.

| parameter | default | meaning |
| --- | --- | --- |
| `coyote` | `5` | frames after leaving a ledge during which A still jumps, 0 to 15 |
| `buffer` | `5` | frames before landing during which an A press is remembered, 0 to 15 |
| `shorthop` | `3` | ascent phases before releasing A shortens the jump, 0 to 15; 3 is 16 pixels, one tile |
| `airjumps` | `0` | extra jumps allowed in the air, 0 to 15 |
| `switchable` | `0` | `1` gates the hack on scheduler kind 6 so AtlasDevArmRole can switch it; requires AtlasDevFrameScheduler |
| `armed` | `1` | with `switchable=1`, whether the hack is on at boot |

```text
AtlasDevJumpControl coyote=5 buffer=5 shorthop=3 airjumps=1
AtlasDevJumpControl switchable=1 armed=0
```
