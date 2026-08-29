# Script Recipes

Working, tested script patterns for common game moments. Every
recipe here was assembled through eoe-cli and verified in an
emulator harness before publication. Paste the block, change the
defines, and bind it to an entrypoint.

Each recipe lists the opcode implementations it needs in your
`iscript_opcodes` map (as `<entry byte="$xx" str="Impl=Name" />`).

One rule that applies to every recipe: the `define` lines belong
in your `[defines]` section, not inside the entrypoint. A define
in the middle of `[iscript]` makes the assembly fail, and before
eoe-cli returned proper exit codes that failure was easy to miss.

## Table of Contents

- [An NPC who gives an item once](#give-item-once)
- [A toll gate](#toll-gate)
- [A door opened by a lever elsewhere](#lever-opens-door)
- [An NPC with three moods](#npc-moods)
- [A choice with consequences](#choice-with-consequences)
- [A switch that permanently changes the room](#switch-changes-room)
- [A trapped treasure](#trapped-chest)
- [Random loot](#random-loot)
- [A healing spring that charges gold](#healing-spring)
- [A door only open after dark](#night-gated-door)
- [A vision - a mini cutscene](#vision-cutscene)
- [A boss introduction](#boss-introduction)
- [A door for veterans](#veterans-door)
- [A guide who follows your progress](#progress-guide)

<a name="give-item-once"></a>
### An NPC who gives an item once

The flag guards the gift: the first talk gives the item and sets the flag, every later talk takes the other branch.

Needs: `SetFlag`, `IfFlag`

```asm
define GIFT_FLAG 100
.textbox GENERIC
    IfFlag GIFT_FLAG @again
    GetItem ITEM_RED_POTION
    SetFlag GIFT_FLAG
    Msg "Take this<n>potion.<p>Use it well."
    End
@again:
    Msg "I have nothing<n>more for you."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `GIFT_FLAG` | `100` | extended flag that remembers the gift (0-247) |
| `ITEM_RED_POTION` | `$90` | the item to give (any Item define) |

<a name="toll-gate"></a>
### A toll gate

Vanilla LoseGold does the charging, exactly as the game's own pay-doors do; the flag makes the payment stick.

Needs: `AtlasDevIfGoldAtLeast`, `SetFlag`, `IfFlag`

```asm
define PAID_FLAG 101
.textbox GENERIC
    IfFlag PAID_FLAG @open
    AtlasDevIfGoldAtLeast 100 0 0 @canpay
    Msg "The toll is<n>100 golds.<p>You cannot pay."
    End
@canpay:
    IfMsgPrompt "The toll is<n>100 golds.<p>Will you pay?" @paid
    Msg "Then you<n>shall wait."
    End
@paid:
    LoseGold 100
    SetFlag PAID_FLAG
    Msg "The way is<n>open."
    End
@open:
    Msg "You have paid<n>already.<p>Pass."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `PAID_FLAG` | `101` | extended flag that remembers the payment |
| `(toll)` | `100` | the price, in the LoseGold and IfGoldAtLeast lines |

<a name="lever-opens-door"></a>
### A door opened by a lever elsewhere

Two scripts share one flag. The lever sets it; the door script refuses until it is set, then ForceDoor lets the transition through. Bind the second block to the door's requirement script.

Needs: `SetFlag`, `IfFlag`, `ForceDoor`

```asm
define LEVER_FLAG 102
; --- the lever (an NPC or object script) ---
.textbox GENERIC
    IfFlag LEVER_FLAG @done
    SetFlag LEVER_FLAG
    Msg "Something<n>heavy moved<n>far away."
    End
@done:
    Msg "The lever will<n>not move again."
    End

; --- the door (bind to the door requirement script) ---
;.textbox GENERIC
;    IfFlag LEVER_FLAG @openup
;    Msg "It will not<n>budge."
;    End
;@openup:
;    ForceDoor
;    End
```

| change this | default | meaning |
| --- | --- | --- |
| `LEVER_FLAG` | `102` | the flag the lever sets and the door checks |

<a name="npc-moods"></a>
### An NPC with three moods

Flag checks fall through from latest to earliest, so the NPC always speaks to your furthest progress.

Needs: `IfFlag`

```asm
define MOOD_FLAG_A 103
define MOOD_FLAG_B 104
.textbox GENERIC
    IfFlag MOOD_FLAG_B @c
    IfFlag MOOD_FLAG_A @b
    Msg "These are dark<n>days, stranger."
    End
@b:
    Msg "You have given<n>us hope."
    End
@c:
    Msg "Our hero<n>returns!"
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `MOOD_FLAG_A` | `103` | first progress flag |
| `MOOD_FLAG_B` | `104` | second progress flag |

<a name="choice-with-consequences"></a>
### A choice with consequences

IfMsgPrompt is the vanilla yes/no box as a branch; the flag makes the refusal permanent.

Needs: `SetFlag`

```asm
define CHOICE_FLAG 105
.textbox GENERIC
    IfFlag CHOICE_FLAG @never
    IfMsgPrompt "I can teach you<n>a secret.<p>Do you want it?" @teach
    SetFlag CHOICE_FLAG
    Msg "Then I shall<n>never offer<n>again."
    End
@teach:
    Msg "The east wall<n>of the spring<n>is hollow."
    End
@never:
    Msg "You refused.<p>I keep my<n>secrets now."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `CHOICE_FLAG` | `105` | remembers the refusal forever |

<a name="switch-changes-room"></a>
### A switch that permanently changes the room

The [tilemap_changes] section binds tile edits to the flag; RunScreenHandler applies them immediately, and the screen handler re-applies them on every later visit. Set the screen's event handler to 3 in the editor.

Needs: `SetFlag`, `IfFlag`, `RunScreenHandler`

```asm
define WALL_FLAG 106
.textbox GENERIC
    IfFlag WALL_FLAG @done
    SetFlag WALL_FLAG
    RunScreenHandler
    Msg "The wall<n>crumbles!"
    End
@done:
    Msg "The way stands<n>open."
    End
```

And the matching `[tilemap_changes]` entry (copy your own
tiles with Ctrl+Shift+C in the editor):

```asm
[tilemap_changes]

  world 0
  screen 5
  flag 106

  4,4,66
  5,4,66

```

| change this | default | meaning |
| --- | --- | --- |
| `WALL_FLAG` | `106` | the flag the tilemap change is bound to |
| `[tilemap_changes]` | `world/screen/tiles` | copied from the editor with Ctrl+Shift+C |

<a name="trapped-chest"></a>
### A trapped treasure

The gift and the ambush share one script: the item lands first, then the guards spawn beside the player.

Needs: `SetFlag`, `IfFlag`, `AtlasDevSpawnEntity`, `AtlasDevPlaySFX`

```asm
define TRAP_FLAG 107
.textbox GENERIC
    IfFlag TRAP_FLAG @empty
    SetFlag TRAP_FLAG
    GetItem ITEM_WING_BOOTS
    AtlasDevPlaySFX 4
    AtlasDevSpawnEntity $21 $84
    AtlasDevSpawnEntity $21 $88
    Msg "It was<n>guarded!"
    End
@empty:
    Msg "The chest is<n>empty."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `TRAP_FLAG` | `107` | one trap per game |
| `$21` | `monster id` | who jumps out (SpawnEntity id) |
| `$84 / $88` | `positions` | packed YX spawn spots |

<a name="random-loot"></a>
### Random loot

Sampling the free-running frame counter at talk time is the cheapest honest coin flip the console has.

Needs: `IfAddrBetween`

```asm
.textbox GENERIC
    IfAddrBetween $001a 0 127 @lucky
    Msg "The old bag<n>is empty today."
    End
@lucky:
    GetItem ITEM_RED_POTION
    Msg "Something<n>useful remains!"
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `$1a` | `-` | the game's frame counter, the randomness source |
| `0 / 127` | `odds` | the counter band that wins (127 of 256 here) |

<a name="healing-spring"></a>
### A healing spring that charges gold

GetHealth is the vanilla healer the hospitals use; the gold check and charge wrap it in a price.

Needs: `AtlasDevIfGoldAtLeast`, `AtlasDevPlaySFX`

```asm
.textbox GENERIC
    AtlasDevIfGoldAtLeast 50 0 0 @heal
    Msg "The spring<n>asks 50 golds."
    End
@heal:
    IfMsgPrompt "Bathe for<n>50 golds?" @yes
    Msg "The water<n>waits."
    End
@yes:
    LoseGold 50
    GetHealth 80
    AtlasDevPlaySFX 9
    Msg "Your wounds<n>close."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `(price)` | `50` | in the IfGoldAtLeast and LoseGold lines |
| `(amount)` | `80` | GetHealth heals to this many points |

<a name="night-gated-door"></a>
### A door only open after dark

The day/night hack keeps its phase at $04e3; IfAddrBetween reads it straight from RAM. Requires AtlasDevDayNightCycle in the general hacks list.

Needs: `IfAddrBetween`, `ForceDoor` — and the hacks `AtlasDevFrameScheduler`, `AtlasDevDayNightCycle`

```asm
.textbox GENERIC
    IfAddrBetween $04e3 2 6 @dark
    Msg "The shrine<n>opens only<n>after dark."
    End
@dark:
    Msg "The night<n>lets you pass."
    ForceDoor
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `$04e3` | `-` | the day/night phase byte (requires AtlasDevDayNightCycle) |
| `2 / 6` | `dark band` | phases that count as night |

<a name="vision-cutscene"></a>
### A vision - a mini cutscene

Freeze the world, fade it away, hold the dark, come back, and reopen the box for the aftermath text. Every step is blocking, so the script reads top to bottom exactly as it plays.

Needs: `AtlasDevHideTextbox`, `AtlasDevOpenTextbox`, `AtlasDevFreezeEntities`, `AtlasDevResumeEntities`, `AtlasDevFadeOut`, `AtlasDevFadeIn`, `AtlasDevWaitFrames`, `AtlasDevPlaySFX`

```asm
.textbox GENERIC
    MsgNoskip "Let me show you<n>what I saw..."
    AtlasDevHideTextbox
    AtlasDevFreezeEntities
    AtlasDevFadeOut 60 4
    AtlasDevPlaySFX 11
    AtlasDevWaitFrames 45
    AtlasDevFadeIn 60 4
    AtlasDevResumeEntities
    AtlasDevOpenTextbox
    Msg "...the mist,<n>swallowing the<n>fortress whole."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `60 / 4` | `fade` | frames and depth of the darkness |
| `45` | `hold` | frames of black |

<a name="boss-introduction"></a>
### A boss introduction

Freeze, shake, switch the music, put a face on the words, release. Eight lines of drama.

Needs: `AtlasDevFreezeEntities`, `AtlasDevResumeEntities`, `AtlasDevShakeScreen`, `AtlasDevSetMusic`, `AtlasDevSetPortrait`, `AtlasDevClearPortrait`

```asm
.textbox GENERIC
    AtlasDevFreezeEntities
    AtlasDevShakeScreen 60 2 1
    AtlasDevSetMusic 5
    AtlasDevSetPortrait KING
    MsgNoskip "You dare enter<n>my hall?"
    AtlasDevClearPortrait
    AtlasDevResumeEntities
    Msg "The air turns<n>cold..."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `5` | `song` | the battle music id (1-16) |
| `60 2 1` | `shake` | frames, amplitude, period |

<a name="veterans-door"></a>
### A door for veterans

One experience check; the door itself does the gatekeeping.

Needs: `AtlasDevIfXPAtLeast`, `ForceDoor`

```asm
.textbox GENERIC
    AtlasDevIfXPAtLeast 3000 @worthy
    Msg "Return when<n>your deeds<n>weigh more."
    End
@worthy:
    Msg "The guild<n>knows your<n>name. Enter."
    ForceDoor
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `3000` | `xp bar` | experience required to pass |

<a name="progress-guide"></a>
### A guide who follows your progress

The same falling-through pattern as the moods recipe, scaled up: quest flags for the story spine, extended flags for your own milestones, latest first.

Needs: `IfFlag`

```asm
define STEP_ONE 110
define STEP_TWO 111
define STEP_THREE 112
.textbox GENERIC
    IfFlag STEP_THREE @s3
    IfFlag STEP_TWO @s2
    IfFlag STEP_ONE @s1
    Msg "Seek the elder<n>first."
    End
@s1:
    Msg "Good. Now the<n>eastern spring."
    End
@s2:
    Msg "One trial<n>remains."
    End
@s3:
    Msg "Nothing more<n>to teach you."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `110-112` | `flags` | the milestones the guide tracks |
