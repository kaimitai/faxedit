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
  - [BugFixes](#bugfixes)
  - [ConditionalScript](#conditionalscript)
  - [ItemScripts](#itemscripts)
	- [Item List](#item-list)
  - [AtlasDevFrameScheduler](#atlasdevframescheduler)
  - [AtlasDevDayNightCycle](#atlasdevdaynightcycle)
  - [AtlasDevInfectedTint](#atlasdevinfectedtint)
  - [AtlasDevTimeOfDay](#atlasdevtimeofday)
  - [AtlasDevJumpControl](#atlasdevjumpcontrol)
  - [AtlasDevFallControl](#atlasdevfallcontrol)
  - [AtlasDevLadderControl](#atlasdevladdercontrol)
  - [AtlasDevLadderCrown](#atlasdevladdercrown)
  - [AtlasDevSmartKeys](#atlasdevsmartkeys)
  - [AtlasDevEnemyStats](#atlasdevenemystats)
  - [AtlasDevCombatFeel](#atlasdevcombatfeel)

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

> by [Notlob](https://github.com/Notlobb/Randumizer)

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

> by [Notlob](https://github.com/Notlobb/Randumizer)

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

When either of these hacks is installed, the corresponding quest flag can be reused for other things.

### BossLockedItems

Boss-locked item sprites appear regardless of which boss guards the screen, so custom screens can combine any boss with any locked item. Optionally the item stays hidden until every enemy sprite on the screen has been removed.

| parameter | default | meaning |
| --- | --- | --- |
| `enemies` | `true` | keep the item hidden until all enemies are cleared |

```text
BossLockedItems enemies=false
```

### FlexibleItems

> by [Notlob](https://github.com/Notlobb/Randumizer) and [Songbirder](https://github.com/rgeraldporter)

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

See the document [Tileset Graphics and Metatiles](./tileset-gfx.md) for an example of how to use this hack in practice.

### PoisonPickup

> by [Notlob](https://github.com/Notlobb/Randumizer)

Changes poison pickups so that they add an item to the player's inventory instead of damaging the player. By default, poison becomes the Red Potion (`0x10`).

The original "touched poison" script is still run by default. Set `script=false` to skip it entirely.

| parameter | default | meaning                                             |
| --------- | ------- | --------------------------------------------------- |
| `item`    | `0x10`  | Item ID to add to the inventory                     |
| `sound`   | `0x08`  | Sound effect played when the item is picked up      |
| `script`  | `true`  | Whether to run the original "touched poison" script |

```text
PoisonPickup item=16 script=false
```

See the [Item List](#item-list) for valid item values.

### SRAM

Adds battery-backed SRAM saving. Progress is saved via iScript opcode `ShowMantra`, replacing the original password-based save system. On the start screen, `CONTINUE` is only available when a valid SRAM save is present.

By default, the saved state matches the progress preserved by the vanilla password system, with additional state needed for a complete SRAM save. The saved RAM ranges can be overridden to preserve additional game state if modders want to preserve other RAM ranges.

| parameter | default                                                     | meaning                                                                                             |
| --------- | ----------------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| `ranges`  | `$039d:42+$042c:2+$0437:1+$0439:1+$04c2:4+$0101:31+$0390:5` | RAM ranges copied to SRAM. Each entry is written as `address:length`, with entries separated by `+` |
| `save_gold` | `true` | Restore Gold and XP when loading from SRAM |
| `keep_gold` | `false` | Keep Gold and XP on death |
| `absolute_spawn` | `true` | iScript opcode `SetSpawn` always updates your spawn point |
| `color` | `true` | Color `CONTINUE` according to whether a valid save is available |

Custom save ranges can be supplied if needed, but the default value should be a sensible choice for most:

```text
SRAM
```

In the original game, iScript opcode `SetSpawn` only updates the spawn point if it is higher than your current spawn point. That means you can potentially save in one location, but spawn in another. When `absolute_spawn=true` your spawn point is always updated via `SetSpawn`.

With `color=true`, `CONTINUE` on the start screen is colored gray when no valid save is present and green-ish when a save is available, at least with the default start screen palette.

The attribute data is configured by `sram_start_screen_attrs` in `eoe_config.xml`. Override this config item if you want to change the presentation. It specifies a PPU address followed by the attribute bytes written from that address onward for the valid and invalid save states.

Use `color=false` to disable the attribute changes entirely.

### TextSpeed

> by [Songbirder](https://github.com/rgeraldporter)

Changes the text display speed. Lower masks make text display faster. For regular timing, use `%0` or binary masks consisting only of consecutive `1` bits, such as `%1`, `%11`, `%111`, etc. Other values are valid but may produce uneven text timing.

> Note: Only supported for US, US Rev A and EU ROMs.

| parameter | default | meaning |
| --- | --- | --- |
| `mask` | `%0` | Text speed mask. `%0` is fastest; `%11` is vanilla speed. Higher masks such as `%111` and `%1111` are progressively slower |

```text
TextSpeed mask=%1
```

### BugFixes

Fixes several known bugs in the original game:

* The Pendant increases weapon strength by 25% when equipped, instead of when it is not equipped.
* Picking up the Battle Suit uses the armor inventory count instead of the weapon inventory count.
* Picking up the Dragon Slayer uses the weapon inventory count instead of the armor inventory count.

This hack has no parameters.

```text
BugFixes
```

### ConditionalScript

Makes sprite iScripts conditional based on extended flags.

When a configured sprite interaction is activated, its iScript index is also used as an extended flag index. If the corresponding extended flag is set, the iScript is not executed. For example, an interaction using iScript 10 will be disabled while extended flag 10 is set.

By default, this applies to sprite #79 - The invisible dialogue trigger. It can also be enabled for NPC interactions, preventing an NPC's iScript from running when pressing UP if its corresponding extended flag is set.

This can be used to create one-time or conditional events and NPC interactions. Scripts can set or clear their corresponding extended flag as needed.

| parameter | default | meaning |
| --- | --- | --- |
| `trigger` | `true` | Enables conditional scripts for invisible triggers |
| `npc` | `false` | Enables conditional scripts for NPC interactions |

A script might look like this, if extended opcode `SetFlag` is available:

```asm
.entrypoint 10
.textbox GENERIC
  Msg "one time text"
  SetFlag 10 ; makes sure this script can't fire again
  End
```

By default the behavior is applied to triggers, but not NPCs.

```text
ConditionalScript
```

### ItemScripts

Makes configured items execute iScripts when used.

Each configured item ID is mapped to an iScript ID. When the item is used, the corresponding iScript is executed and the item is removed from the player's inventory.

Items 0-16 normally use the game's item-use handlers. Configured items replace their normal behavior, while unconfigured items are left unchanged. Item IDs above 16 can also be configured, allowing normally unusable item slots to execute scripts.

This can be used to implement custom consumables and other scripted item effects. Extended script opcodes can provide the actual item behavior without requiring additional item-specific code.

| parameter | default  | meaning                     |
| --------- | -------- | --------------------------- |
| `data`    | required | List of `item:script` pairs |

For example, this makes item 14 execute iScript 90 and item 17 execute iScript 91:

```text
ItemScripts data=14:90+17:91
```

The scripts might look like this:

```asm
.entrypoint 90
.textbox GENERIC
  ; custom behavior for item 14
  End

.entrypoint 91
.textbox GENERIC
  ; custom behavior for item 17
  End
```

#### Item List

| ID            | item          | notes                                  |
| ------------- | ------------- | -------------------------------------- |
| `0x00`        | Ring of Elf   | Not a normal selectable inventory item |
| `0x01`        | Ruby Ring     | Not a normal selectable inventory item |
| `0x02`        | Ring of Dworf | Not a normal selectable inventory item |
| `0x03`        | Demon's Ring  | Not a normal selectable inventory item |
| `0x04`        | Key A         |                                        |
| `0x05`        | Key K         |                                        |
| `0x06`        | Key Q         |                                        |
| `0x07`        | Key J         |                                        |
| `0x08`        | Key Jo        |                                        |
| `0x09`        | Mattock       |                                        |
| `0x0a`        | Rod           |                                        |
| `0x0b`        | Crystal       |                                        |
| `0x0c`        | Lamp          | Unused in the vanilla game             |
| `0x0d`        | Hour Glass    |                                        |
| `0x0e`        | Book          | Unused in the vanilla game             |
| `0x0f`        | Wing Boots    |                                        |
| `0x10`        | Red Potion    |                                        |
| `0x11`        | Black Potion  | Unused in the vanilla game             |
| `0x12`        | Elixir        | Not a normal selectable inventory item |
| `0x13`        | Pendant       | Not a normal selectable inventory item |
| `0x14`        | Black Onix    | Not a normal selectable inventory item |
| `0x15`        | Fire Crystal  | Unused in the vanilla game             |
| `0x16`-`0x1f` | Glitched      | Not valid normal items                 |

Items from index 0x16 and up can be stored and displayed in the inventory, but has no effect in the vanilla game. They can be given a purpose with a hack that adds or overrides item-use behavior.

### AtlasDevFrameScheduler

A neutral frame scheduler other hacks build on: an NMI tick with three role slots and an exclusive post-deadline lane for work that must run after the frame's last critical PPU write. PRE roles run only when both the PPU queue and nametable-strip work are idle. On its own it changes nothing visible — it exists so per-frame hacks can share one hook instead of each patching the NMI. Role hacks like AtlasDevDayNightCycle require it and refuse to build without it.

The three slots are RAM, so scripts can switch roles on and off at runtime with the AtlasDevArmRole and AtlasDevDayNight opcodes. At build time, a boot slot is unclaimed only when its arm byte is zero and its PRE vector still points to the scheduler's default stub. A role installer reuses only a compatible existing kind or claims the first unclaimed slot, refusing without modifying the ROM when none is available. The single POST lane similarly refuses a second claimant.

Kinds `$80` to `$ff` are gate only: a slot holding one is armed, cleared and tested exactly like any other kind, but the tick never calls its vector, so a hack that only needs a runtime switch costs the tick nothing beyond the slot test. AtlasDevJumpControl with `switchable=1` is such a client, on kind `$86`.

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
claims kind `$86` in the next free boot slot, every stub runs only while
some slot holds that kind, and `AtlasDevArmRole $86, 0` switches jump
control off mid game while `AtlasDevArmRole $86, 1` switches it back on;
switching off also clears its RAM byte. `armed=0` installs it off until a
script arms it. The kind is gate only, so the tick never calls the slot;
the cost is 82 bytes of gates in the stubs.

`flag=n` is the lighter runtime switch: every stub runs only while extended
flag `n` is set, so a script switches jump control with `SetFlag n` and
`ClearFlag n`, no scheduler needed. The flag page is cleared at reset, so a
`flag` install is off until a script sets the flag; a clear flag is
indistinguishable from stock. `flag` and `switchable` are two forms of the
same switch and cannot be combined; `armed` belongs to `switchable`.

| parameter | default | meaning |
| --- | --- | --- |
| `coyote` | `5` | frames after leaving a ledge during which A still jumps, 0 to 15 |
| `buffer` | `5` | frames before landing during which an A press is remembered, 0 to 15 |
| `shorthop` | `3` | ascent phases before releasing A shortens the jump, 0 to 15; 3 is 16 pixels, one tile |
| `airjumps` | `0` | extra jumps allowed in the air, 0 to 15 |
| `switchable` | `0` | `1` gates the hack on scheduler kind `$86` so AtlasDevArmRole can switch it; requires AtlasDevFrameScheduler |
| `armed` | `1` | with `switchable=1`, whether the hack is on at boot |
| `flag` | none | extended flag 0 to 247 that switches the hack on at runtime; cannot be combined with `switchable` |


`profile` sets all four at once, in the spirit of the game named, and any
knob given explicitly overrides it. With no profile the defaults above apply.

| profile | coyote | buffer | shorthop | airjumps |
|---|---|---|---|---|
| `vanilla` | 0 | 0 | 0 | 0 |
| `zelda2` | 3 | 3 | 1 | 0 |
| `metroid` | 5 | 5 | 3 | 0 |
| `megaman` | 0 | 2 | 1 | 0 |
| `castlevania` | 0 | 0 | 0 | 0 |
| `ninjagaiden` | 2 | 3 | 0 | 0 |
| `ghostsngoblins` | 0 | 0 | 0 | 0 |
| `kidicarus` | 4 | 4 | 2 | 0 |
| `contra` | 2 | 3 | 0 | 0 |
| `arcade` | 5 | 5 | 3 | 1 |

```text
AtlasDevJumpControl coyote=5 buffer=5 shorthop=3 airjumps=1
AtlasDevJumpControl switchable=1 armed=0
AtlasDevJumpControl flag=20
```

### AtlasDevFallControl

Replaces the constant 8 px-per-frame drop with a fall curve, and reads Left/Right while the hero is in the air. Profiles expand to both knobs; an explicit `curve=` or `steer=` overrides one of them. `profile=vanilla` installs nothing. No RAM is used: the fall phase lives in the jump-phase byte, which is idle during free fall.

| parameter | default | meaning |
| --- | --- | --- |
| `profile` | `arc` | `vanilla`, `arc`, `zelda2`, `floaty`, `moon`, or a shared feel name from the table below |
| `curve` | from the profile | px per fall frame, `+`-separated, 1 to 16 entries of 0 to 8; the last entry repeats as the terminal speed |
| `steer` | from the profile | `0` vanilla, `1` steer only while a direction is held (momentum kept otherwise), `2` full air control |
| `kind` | `0` | `0` always on; `1` to `255` makes the hack script controllable: every stub runs the vanilla bytes unless an AtlasDevFrameScheduler slot holds this kind, so `AtlasDevArmRole kind, 1` and `AtlasDevArmRole kind, 0` switch it at runtime. Requires AtlasDevFrameScheduler earlier in the list. Kind `6` is the registered number for this hack |
| `boot` | `true` | with `kind`, seed a scheduler boot slot so the hack is on from power on; `false` leaves arming to a script |
| `flag` | none | extended flag 0 to 247 that switches the hack on at runtime: every stub runs the vanilla bytes while the flag is clear, so `SetFlag n` and `ClearFlag n` switch it with no scheduler. Cannot be combined with `kind`; `boot` belongs to `kind` |

| profile | curve | steer |
| --- | --- | --- |
| `vanilla` | none | 0 |
| `arc` | `1+1+1+1+2+2+4+4+4+4+8` | 1 |
| `zelda2` | `1+2+3+4+5+6+7+8` | 2 |
| `floaty` | `1+1+2+2+3+3+4+4+5+5+6` | 2 |
| `moon` | `0+0+1+1+1+2+2+2+3+3+4` | 2 |
| `metroid` | `1+1+2+2+3+3+4+4+5+5+6` | 2 |
| `megaman` | `1+2+3+4+5+6+7+8` | 2 |
| `castlevania` | vanilla | 0 |
| `ninjagaiden` | `1+2+3+4+5+6+7+8` | 2 |
| `ghostsngoblins` | vanilla | 0 |
| `kidicarus` | `1+1+2+2+3+3+4+4+5+5+6` | 1 |
| `contra` | `1+2+3+4+5+6+7+8` | 2 |
| `arcade` | `1+2+3+4+5+6+7+8` | 2 |

The names from `metroid` down are shared with `AtlasDevJumpControl`,
`AtlasDevLadderControl` and `AtlasDevCombatFeel`, so the same profile on
each composes into one feel. Each is a first pick in the spirit of the game
named, not a port of its numbers; several share a curve for now, and the
values will be tuned by play.

Steering changes which gaps can be crossed; mode 1 keeps every vanilla jump as it was until a direction is pressed. A jump that runs out over a pit keeps its speed, and a phase left over from a longer curve is clamped before use, so switching the hack on mid fall is safe. Knockback in the air restarts the curve. The Wing Boots slow fall is untouched. With `kind` set, the scheduler must be installed first; the installer reuses a slot already holding the kind or claims the first free boot slot, and refuses without modifying the ROM when none is available. With `flag` set the same gated stubs test the extended flag instead of the slots; the flag page is cleared at reset, so a `flag` install is off until a script sets the flag, and a clear flag is indistinguishable from stock.

```text
AtlasDevFallControl profile=zelda2
AtlasDevFallControl curve=1+1+2+2+4+8 steer=1
AtlasDevFrameScheduler
AtlasDevFallControl profile=arc kind=6
AtlasDevFallControl profile=zelda2 flag=21
```

### AtlasDevLadderControl

How fast the hero climbs, and whether he may attack while on a ladder.
Climb speed is four numbers the vanilla ROM already holds as constants, so
with default parameters this hack writes nothing at all and the patched ROM
is byte identical to the source.

Stock climbs up at 160 subpixels per frame and down at 192, so descent is a
fifth faster than ascent, and the two are separate numbers rather than one
speed with a sign. `up` and `down` set them, and `wingdown` sets the Wing
Boots descent, all in subpixels per frame where 256 is one pixel. `wingup`
is the odd one out and is whole pixels per frame, because vanilla subtracts
only from the whole pixel byte there and widening it would need more space
than the instruction has.

`attack=1` lets the hero swing while on a ladder, which vanilla refuses with
a single branch. The refusal outlasts the input: the game counts the hero as
climbing until his body leaves the rung, not only while he is moving, so one
tap of Up keeps it in force.

`attackpose=1` fixes the animation that goes with it. Two frame selectors
choose the hero's appearance, one for the weapon and shield overlays and one
for the body, and both ask whether he is climbing before they ask whether he
is attacking, because vanilla never needed the second question. Fixing only
one gives the hero three arms, the overlays drawing the swing while the body
still holds the rungs. Both are reordered to test the attack bit first, in
place, and the result is smaller than what it replaces.

`attackflag=n` gates the attack on extended flag `n` at runtime instead of
enabling it outright, so a script can allow ladder attacks after an item or
an event with `SetFlag` and take them away again with `ClearFlag`. Only the
operand of the existing call is retargeted, into a 14 byte stub, so the
the other users of the same climbing test keep vanilla behavior: jumping off
a ladder, casting magic and the body's own climb pose are untouched. A
clear flag is indistinguishable from stock, and the flag page is cleared at
reset, so a runtime build plays exactly like the original game until the
script sets the flag. `attackflag` and `attack=1` are mutually exclusive.

`flag=n` does the same for the climb speeds: every speed block whose value
differs from vanilla (`up`, `down`, `wingup`, `wingdown`) becomes a call into
a stub that runs the new constants while extended flag `n` is set and the
displaced vanilla bytes while it is clear. Blocks left at vanilla speed are
not touched. The stubs live in the free block, 35 bytes per 16 bit speed and
23 for the wing boots ascent, and leave carry and the accumulator exactly as
the vanilla code does, since the instructions after each block read them.
`flag` alone, with every speed at vanilla, is refused because there is
nothing to switch. `flag` and `attackflag` are independent and may name the
same flag or different ones.

Every site is verified against its exact vanilla bytes before anything is
written, and all of them are identical in the US, US rev A, EU and JP ROMs.
Does not require AtlasDevFrameScheduler.

| parameter | default | meaning |
| --- | --- | --- |
| `up` | `160` | climb up speed in subpixels per frame, 1 to 2048 |
| `down` | `192` | climb down speed in subpixels per frame, 1 to 2048 |
| `wingup` | `1` | Wing Boots ascent in whole pixels per frame, 1 to 8 |
| `wingdown` | `384` | Wing Boots descent in subpixels per frame, 1 to 2048 |
| `attack` | `0` | 1 allows attacking while on a ladder |
| `attackpose` | `0` | 1 draws the attack frames while climbing instead of the climb pose |
| `attackflag` | none | extended flag 0 to 247 that allows the attack at runtime |
| `flag` | none | extended flag 0 to 247 that switches the changed climb speeds on at runtime; while it is clear the vanilla speeds run |


`profile` sets `up`, `down`, `attack` and `attackpose` at once, in the spirit
of the game named; Wing Boots are left as given, and any knob given
explicitly overrides the profile. `metroid` and `contra` have no ladders and
are refused here rather than invented: leave this hack out for those feels.

| profile | up | down | attack | attackpose |
|---|---|---|---|---|
| `vanilla` | 160 | 192 | 0 | 0 |
| `zelda2` | 224 | 224 | 1 | 1 |
| `megaman` | 255 | 255 | 0 | 0 |
| `castlevania` | 160 | 160 | 0 | 0 |
| `ninjagaiden` | 255 | 255 | 1 | 1 |
| `ghostsngoblins` | 160 | 192 | 0 | 0 |
| `kidicarus` | 224 | 224 | 1 | 1 |
| `arcade` | 255 | 255 | 1 | 1 |

```text
AtlasDevLadderControl up=384 down=448 attack=1 attackpose=1
AtlasDevLadderControl up=320 attackflag=4 attackpose=1
AtlasDevLadderControl up=384 down=448 flag=22
```

### AtlasDevLadderCrown

Gives ordinary ladder tops a stable exit. In `crown` mode, climbing onto a
supported top rung lets the hero stand with their feet on it, walk away,
jump, or press Down to climb back. Support is earned by climbing Up; falling
past a ladder or jumping back onto it does not create a platform. Wing Boots
flight and ordinary falls keep their usual behavior.

`floor` mode stops ascent at the adjacent floor height without adding a
standing surface. `vanilla` installs nothing. Omitting the hack entirely also
leaves the original behavior unchanged. Neither option removes patches from
an already modified input ROM; always rebuild from your clean source ROM.

| parameter | default | meaning |
| --- | --- | --- |
| `mode` | `crown` | `crown`, `floor` or `vanilla` |
| `downhold` | `0` | eligible Down movement ticks to wait before leaving an earned crown, 0 to 8 |
| `align` | `0` | maximum horizontal correction toward the saved rung when descending from an earned crown, 0 to 2 pixels |
| `roompolicy` | `all` | `all` rooms, only an `allow` list, or every room except a `deny` list |
| `rooms` | none | up to eight distinct `area:screen` byte pairs, separated by `+` |

Room policies apply to the ladder exit behavior, not to climb speed or
attacking. `allow` and `deny` require a nonempty list. `all` rejects a room
list. Mode and policy names are lowercase. Numeric values accept decimal,
`$` or `0x` hexadecimal, and `%` binary.

`downhold=3` waits three eligible Down ticks and begins descent on the fourth.
Releasing Down or pressing Up resets the wait. Alignment is limited to the
saved rung; it does not pull the hero toward other ladders. Both options are
exclusive to `crown` mode. `vanilla` rejects nondefault options that would
otherwise have no effect.

```text
AtlasDevLadderCrown
```

For a short pause before descending and a small alignment assist:

```text
AtlasDevLadderCrown downhold=3 align=2
```

To restrict the change to selected rooms:

```text
AtlasDevLadderCrown roompolicy=allow rooms=6:3+3:12
AtlasDevLadderCrown mode=floor roompolicy=deny rooms=1:2
```

These are alternatives; only one AtlasDevLadderCrown entry is allowed.

The ladder top must have clear body space above it and an adjacent solid
floor beneath the top rung. Walking off, taking damage, starting a jump or
changing that geometry releases crown support. This is not a general
invisible floor over every ladder.

Supported input is an unexpanded MMC1 iNES or NES2 ROM with compatible movement
code. Compatibility is checked against the instructions and routines used by
the hack, not the region name. Mirroring and battery-backed RAM header flags
are accepted, as is unused iNES padding; the original header is preserved.
Trainer, CHR-ROM and expanded layouts are not supported. The default uses 605
bytes of bank 15 space. Crown reserves `$04e0` for support and `$04e1` for
the optional down-hold timer, separate from the scheduler, jump buffer and
palette roles. Script storage configured over either cell is rejected,
including mirrored addresses. Floor mode uses 315 bytes and no persistent RAM.
Room gates add code and tables; the largest crown configuration uses 713 bytes.
Code uses the bank 15 space assigned by FaxEdit's general-hack allocator,
including reclaimed space. Its location and capacity depend on the project.

AtlasDevJumpControl, AtlasDevFallControl and the native scheduler's palette
roles can be listed together with this hack when their combined code fits.
Crown is installed first; other hacks keep their relative order, so place
AtlasDevFrameScheduler before its roles. A jump buffer adds ladder timer
handling: default crown plus default jump control uses 785 bytes;
default crown plus the scheduler uses 761. Larger combinations can
exceed the available space and are rejected. No other hack is relocated or
given extra space by this feature.

Build these combinations from a compatible base ROM. Crown accepts an existing
native scheduler only with unclaimed PRE and POST vectors; preinstalled
handlers and unknown NMI hooks are rejected. Hook checks also reject
incompatible movement patches. Script activation, climb speeds and ladder
attack settings are not part of this hack.


### AtlasDevSmartKeys

Opens a locked door with a matching key the hero is carrying, without
making the player select it first, and without disturbing whatever item is
currently selected. Keys are still spent one per door, still counted and
still bought in shops; only the trip to the item menu goes away.

The door gate checks a ring by asking whether the hero owns it, and a key
by asking whether it is the selected item. With this hack a carried key is
accepted the way a ring is, and it is still spent.

When the selected item is the right key, the door behaves exactly as it
always did and that key is the one spent. When it is not, one matching key
is taken from the item list instead, the list closes up around the gap, and
the selected item is left alone. A door the hero has no key for refuses with
its usual message.

The five key checks in vanilla are replaced by one shared check that lives in
the same bytes they occupied, so this hack uses no general hack space at all.
The unlocked case and the three ring checks are not touched. Every site is
verified against its exact vanilla bytes before anything is written; the gate
is identical in the US, US rev A and EU ROMs, and the JP ROM, where it
differs, is refused.

`mode=vanilla` installs nothing. The default is `mode=carried`.

```
AtlasDevSmartKeys
```

### AtlasDevEnemyStats

Scales the enemy tables at build time. Every enemy's hit points, contact
damage, experience reward and coin drop are byte tables in the ROM, one entry
per enemy type, and this hack multiplies each table by a percent. The stun an
enemy takes when hit is a single byte and can be set directly.

`hp`, `damage`, `xp` and `gold` are percents of vanilla, 1 to 400, applied to
every entry of the table named. Entries that are zero stay zero, since those
are the ids that are not monsters; everything else rounds to nearest, never
drops below one, and saturates at 255, so a boss already at 250 hit points
does not grow past that. `gold` scales the coin drops only; bread is left
alone. `stagger` is the stun in frames, vanilla 8, and it is also the length
of the hit flash, since one counter drives both.

`profile` sets all five at once, and any knob given explicitly overrides it:

| profile | hp | damage | xp | gold | stagger |
|---|---|---|---|---|---|
| `normal` | 100 | 100 | 100 | 100 | 8 |
| `easy` | 75 | 75 | 150 | 150 | 12 |
| `hard` | 150 | 150 | 100 | 100 | 6 |
| `nightmare` | 200 | 200 | 75 | 75 | 4 |
| `grind` | 100 | 100 | 200 | 200 | 8 |

The default is `normal` and writes nothing. No code, RAM or general hack
space is used. Every table is verified against its vanilla bytes before
anything is written, and the tables are identical in the US, US rev A, EU and
JP ROMs.

```
AtlasDevEnemyStats profile=hard
AtlasDevEnemyStats profile=nightmare xp=150
AtlasDevEnemyStats hp=150 damage=150 xp=75
```

### AtlasDevCombatFeel

How the hero walks, how long he is safe after a hit, how far he is shoved,
and how fast he swings. Every knob is an operand byte the vanilla ROM already
holds, so with default parameters this hack writes nothing at all and the
patched ROM is byte identical to the source.

The walk is not one speed. The hero starts every walk at 192 subpixels per
frame, three quarters of a pixel, and accelerates every frame by an amount
that depends on his title until he reaches 384, a pixel and a half. `walk`
and `walkmax` set those two speeds in subpixels per frame, where 256 is one
pixel, and `ramp` is the four per frame increments by title tier, vanilla
`2+4+6+8`; `ramp=0+0+0+0` gives a flat walk at the base speed. `walkmax` is
capped at 2048, eight pixels per frame, which is the speed the engine already
uses for the shove.

`iframes` is the mercy time after a hit in frames, vanilla 60, written to all
three places the game sets it. The shove after a hit lasts while that counter
is above a threshold, so in vanilla it lasts three frames, and the threshold
is always written as `iframes` minus `knockbackframes`: raising the mercy
time never lengthens the shove by accident, and `knockbackframes=0` removes
the shove while keeping the mercy time. `knockback` is the shove speed in
subpixels per frame, vanilla 2048.

`attack` is the three phase lengths of a swing in frames, vanilla `8+3+8`,
of which the middle phase and the one after it are the frames the blade can
hit. `moveattack=1` lets the hero keep walking during a swing, which vanilla
refuses with a single branch; the pose is not changed, so he slides in the
swing pose, which is the honest cost of the option.

`profile` sets every knob at once. Each profile is a pick of numbers in this
game's units that plays in the spirit of the game it is named after, the way
`AtlasDevFallControl`'s `zelda2` is a curve shaped like that descent rather
than its bytes; none is a port of another engine's constants. A knob given
explicitly overrides the profile.

| profile | walk to walkmax | ramp | iframes | knockbackframes | knockback | attack | moveattack |
|---|---|---|---|---|---|---|---|
| `vanilla` | 192 to 384 | 2+4+6+8 | 60 | 3 | 2048 | 8+3+8 | 0 |
| `zelda2` | 320 flat | 0 | 60 | 4 | 1536 | 6+3+5 | 1 |
| `metroid` | 256 to 384 | 4+4+4+4 | 90 | 6 | 2048 | 8+3+8 | 1 |
| `megaman` | 352 flat | 0 | 60 | 2 | 1024 | 5+3+4 | 1 |
| `castlevania` | 192 flat | 0 | 45 | 8 | 2048 | 10+6+10 | 0 |
| `ninjagaiden` | 384 flat | 0 | 40 | 10 | 2048 | 4+3+4 | 1 |
| `ghostsngoblins` | 160 flat | 0 | 75 | 6 | 1536 | 7+4+7 | 1 |
| `kidicarus` | 288 flat | 0 | 60 | 3 | 1280 | 5+3+5 | 1 |
| `contra` | 384 to 448 | 8+8+8+8 | 30 | 2 | 1024 | 4+3+4 | 1 |
| `arcade` | 320 to 512 | 8+8+8+8 | 30 | 2 | 1024 | 6+3+6 | 1 |

Every site is verified against its exact vanilla bytes before anything is
written, and all of them are identical in the US, US rev A, EU and JP ROMs.

```
AtlasDevCombatFeel profile=zelda2
AtlasDevCombatFeel profile=castlevania iframes=60
AtlasDevCombatFeel walk=256 walkmax=512 ramp=4+4+4+4
```
