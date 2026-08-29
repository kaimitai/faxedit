<hr>

# General Hacks

<hr>

[Echoes of Eolis](https://github.com/kaimitai/faxedit) ships a library of optional general hacks: self-contained gameplay and engine modifications that are enabled from the configuration and injected into the ROM at build time. Unlike the extended script opcodes, a general hack needs no script changes at all — enabling it is the whole integration.

General hacks are completely optional. A project that enables none of them produces behavior identical to the original game. Installers validate their parameters and available output capacity, and newer engine-level installers also verify their hook preimages. Some legacy installers do not yet verify every overwritten byte, so use a ROM compatible with the selected configuration region and do not assume arbitrary pre-patched ROMs can safely compose with every hack.

This document describes the hacks in the current library and their parameters. It assumes you are familiar with the configuration override system described in the [advanced modding documentation](advanced-modding.md).

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
  - [AtlasDevFrameScheduler](#atlasdevframescheduler)
  - [AtlasDevDayNightCycle](#atlasdevdaynightcycle)
  - [AtlasDevInfectedTint](#atlasdevinfectedtint)

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
engine's shadow.

| parameter | default | meaning |
| --- | --- | --- |
| `colors` | `$09+$19+$29` | three palette values, plus separated |
| `pulse` | `$20` | pulse mask, a power of two; `0` for a steady tint |

```text
AtlasDevInfectedTint colors=$0C+$1C+$2C pulse=0
```
