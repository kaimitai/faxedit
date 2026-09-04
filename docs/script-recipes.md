# Script Recipes

These recipes are integration fragments for common game moments,
not standalone assembly files. Use them inside a full script export:

1. Copy any define fragment into the existing `[defines]` section.
2. Copy its script body into `[iscript]`, immediately after the
   `.entrypoint N` that you want to bind.

The fragments omit section headers and numeric entrypoints so several
recipes can be combined in one file without duplicate sections or
entrypoint numbers.

Each recipe lists the opcode implementations it needs in your
`iscript_opcodes` map (as `<entry byte="$xx" str="Impl=Name" />`).

Extended flags persist across screen and area visits, but on an
ordinary ROM they are cleared on game initialization and are not
stored in mantras. Unless a project explicitly stores them in SRAM,
"once" in these recipes means once per loaded session, until reset or
mantra load.

## Table of Contents

- [An NPC who gives an item once](#give-item-once)
- [A toll gate](#toll-gate)
- [A door opened by a lever elsewhere](#lever-opens-door)
- [An NPC with three moods](#npc-moods)
- [A choice with consequences](#choice-with-consequences)
- [A switch that changes the room](#switch-changes-room)
- [A trapped treasure](#trapped-chest)
- [Random loot](#random-loot)
- [A healing spring that charges gold](#healing-spring)
- [A door only open after dark](#night-gated-door)
- [A vision - a mini cutscene](#vision-cutscene)
- [A boss introduction](#boss-introduction)
- [A door for veterans](#veterans-door)
- [A guide who follows your progress](#progress-guide)
- [A ferryman - a repeatable paid crossing](#ferryman)
- [A merchant with limited stock](#limited-stock)
- [A combination lock](#combination-lock)
- [A training hall - buying experience](#training-hall)
- [An arena master - clear the beasts for a prize](#arena-master)
- [A collector - bring me three](#potion-collector)
- [A merchant who only trades at night](#nocturnal-merchant)
- [A gambling den - double or nothing](#gambling-den)

<a name="give-item-once"></a>
### An NPC who gives an item once

The flag guards the gift: the first talk gives the item and sets the flag, every later talk takes the other branch.

Needs: `SetFlag`, `IfFlag`

Add to `[defines]`:

```asm
define GIFT_FLAG 100
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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

Vanilla LoseGold does the charging, exactly as the game's own pay-doors do; the flag remembers the payment for the loaded session.

Needs: `AtlasDevIfGoldAtLeast`, `SetFlag`, `IfFlag`

Add to `[defines]`:

```asm
define PAID_FLAG 101
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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

Two scripts share one flag. The lever sets it; the door script refuses until it is set, then ForceDoor lets the transition through. Bind the two bodies to separate entrypoints, with the second used as the door's requirement script.

Needs: `SetFlag`, `IfFlag`, `ForceDoor`

Add to `[defines]`:

```asm
define LEVER_FLAG 102
```

Add the lever body to `[iscript]` after its entrypoint:

```asm
.textbox GENERIC
    IfFlag LEVER_FLAG @done
    SetFlag LEVER_FLAG
    Msg "Something<n>heavy moved<n>far away."
    End
@done:
    Msg "The lever will<n>not move again."
    End
```

Add the door body to `[iscript]` after a different entrypoint, and bind
that entrypoint as the door requirement script:

```asm
.textbox GENERIC
    IfFlag LEVER_FLAG @openup
    Msg "It will not<n>budge."
    End
@openup:
    ForceDoor
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `LEVER_FLAG` | `102` | the flag the lever sets and the door checks |

<a name="npc-moods"></a>
### An NPC with three moods

Flag checks fall through from latest to earliest, so the NPC always speaks to your furthest progress.

Needs: `IfFlag`

Add to `[defines]`:

```asm
define MOOD_FLAG_A 103
define MOOD_FLAG_B 104
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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

IfMsgPrompt is the vanilla yes/no box as a branch; the flag remembers the refusal for the loaded session.

Needs: `SetFlag`, `IfFlag`

Add to `[defines]`:

```asm
define CHOICE_FLAG 105
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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
| `CHOICE_FLAG` | `105` | remembers the refusal until reset or mantra load |

<a name="switch-changes-room"></a>
### A switch that changes the room

The [tilemap_changes] section binds tile edits to the flag; RunScreenHandler applies them immediately, and the screen handler re-applies them on later visits in the loaded session. Set the screen's event handler to 3 in the editor.

Needs: `SetFlag`, `IfFlag`, `RunScreenHandler`

Add to `[defines]`:

```asm
define WALL_FLAG 106
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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
  flag WALL_FLAG

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

Add to `[defines]`:

```asm
define TRAP_FLAG 107
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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
| `TRAP_FLAG` | `107` | one trap per loaded session |
| `$21` | `monster id` | who jumps out (SpawnEntity id) |
| `$84 / $88` | `positions` | packed YX spawn spots |

<a name="random-loot"></a>
### Random loot

Sampling the free-running frame counter at talk time is the cheapest honest coin flip the console has.

Needs: `IfAddrBetween`

Add to `[iscript]` after the entrypoint you bind:

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
| `0 / 127` | `odds` | the inclusive counter band that wins (128 of 256 here) |

<a name="healing-spring"></a>
### A healing spring that charges gold

GetHealth is the vanilla additive healer the hospitals use; it restores the operand amount and caps total health at 80. The gold check and charge wrap it in a price.

Needs: `AtlasDevIfGoldAtLeast`, `AtlasDevPlaySFX`

Add to `[iscript]` after the entrypoint you bind:

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
| `(amount)` | `80` | health GetHealth adds, capped at 80 total HP |

<a name="night-gated-door"></a>
### A door only open after dark

The day/night hack publishes its current darkness level at $04e2. Daylight is $00, while $10 through $20 covers every dim or dark phase and repeats correctly each cycle. Requires AtlasDevDayNightCycle in the general hacks list.

Needs: `IfAddrBetween`, `ForceDoor` — and the hacks `AtlasDevFrameScheduler`, `AtlasDevDayNightCycle`

Add to `[iscript]` after the entrypoint you bind:

```asm
.textbox GENERIC
    IfAddrBetween $04e2 $10 $20 @dark
    Msg "The shrine<n>opens only<n>after dark."
    End
@dark:
    Msg "The night<n>lets you pass."
    ForceDoor
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `$04e2` | `-` | current darkness level (requires AtlasDevDayNightCycle) |
| `$10 / $20` | `dark band` | inclusive levels that count as night |

<a name="vision-cutscene"></a>
### A vision - a mini cutscene

Freeze the entities, fade the background and UI while sprites remain lit, hold the background at full fade, come back, and reopen the box for the aftermath text. The fade and wait steps are blocking, so the timing reads top to bottom as it plays.

Needs: `AtlasDevHideTextbox`, `AtlasDevOpenTextbox`, `AtlasDevFreezeEntities`, `AtlasDevResumeEntities`, `AtlasDevFadeOut`, `AtlasDevFadeIn`, `AtlasDevWaitFrames`, `AtlasDevPlaySFX`

Add to `[iscript]` after the entrypoint you bind:

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
| `60 / 4` | `fade` | frames and depth of the background fade |
| `45` | `hold` | frames at full background fade; sprites stay lit |

<a name="boss-introduction"></a>
### A boss introduction

Freeze, shake, switch the music, put a face on the words, release. Eight lines of drama.

Needs: `AtlasDevFreezeEntities`, `AtlasDevResumeEntities`, `AtlasDevShakeScreen`, `AtlasDevSetMusic`, `AtlasDevSetPortrait`, `AtlasDevClearPortrait`

Add to `[iscript]` after the entrypoint you bind:

```asm
.textbox GENERIC
    AtlasDevFreezeEntities
    AtlasDevShakeScreen 60 2 1
    AtlasDevSetMusic 10
    AtlasDevSetPortrait KING
    MsgNoskip "You dare enter<n>my hall?"
    AtlasDevClearPortrait
    AtlasDevResumeEntities
    Msg "The air turns<n>cold..."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `10 / $0a` | `song` | the boss music id |
| `60 2 1` | `shake` | frames, amplitude, period |

<a name="veterans-door"></a>
### A door for veterans

One experience check; the door itself does the gatekeeping.

Needs: `AtlasDevIfXPAtLeast`, `ForceDoor`

Add to `[iscript]` after the entrypoint you bind:

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

Add to `[defines]`:

```asm
define STEP_ONE 110
define STEP_TWO 111
define STEP_THREE 112
```

Add to `[iscript]` after the entrypoint you bind:

```asm
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

<a name="ferryman"></a>
### A ferryman - a repeatable paid crossing

The toll gate without the flag: no memory, so every crossing costs again. Bind it to a door's requirement script and the door becomes the boat.

Needs: `AtlasDevIfGoldAtLeast`, `ForceDoor`

```asm
.textbox GENERIC
    AtlasDevIfGoldAtLeast 100 0 0 @canpay
    Msg "The crossing<n>costs 100."
    End
@canpay:
    IfMsgPrompt "Cross for<n>100 golds?" @sail
    Msg "The river<n>waits."
    End
@sail:
    LoseGold 100
    Msg "Hold tight."
    ForceDoor
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `(fare)` | `100` | in the IfGoldAtLeast and LoseGold lines |

<a name="limited-stock"></a>

<a name="limited-stock"></a>
### A merchant with limited stock

Extended flags only ever turn on, so a chain of them is a counter that survives forever: each sale sets the next flag, and the third closes the shop for the rest of the game.

Needs: `SetFlag`, `IfFlag`, `AtlasDevIfGoldAtLeast`

```asm
define SOLD_ONE 114
define SOLD_TWO 115
define SOLD_THREE 116
.textbox GENERIC
    IfFlag SOLD_THREE @out
    AtlasDevIfGoldAtLeast 80 0 0 @sell
    Msg "80 golds<n>a bottle."
    End
@sell:
    LoseGold 80
    GetItem ITEM_RED_POTION
    IfFlag SOLD_TWO @last
    IfFlag SOLD_ONE @second
    SetFlag SOLD_ONE
    Msg "Two left."
    End
@second:
    SetFlag SOLD_TWO
    Msg "Last one<n>after this."
    End
@last:
    SetFlag SOLD_THREE
    Msg "That is the<n>last bottle."
    End
@out:
    Msg "Sold out.<p>Come back<n>next season."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `SOLD_ONE..THREE` | `114-116` | one flag per bottle sold |
| `(price)` | `80` | in the IfGoldAtLeast and LoseGold lines |

<a name="combination-lock"></a>

<a name="combination-lock"></a>
### A combination lock

Three lever scripts set three flags; the door demands left and middle set with the right one clear. A wrong flag is checked first, so pulling everything is not the answer.

Needs: `SetFlag`, `IfFlag`, `ForceDoor`

```asm
define LEVER_LEFT 117
define LEVER_MID 118
define LEVER_RIGHT 119
; --- the door (the levers are three one-line SetFlag scripts) ---
.textbox GENERIC
    IfFlag LEVER_RIGHT @wrong
    IfFlag LEVER_LEFT @l1
    Msg "Cold iron.<n>Nothing moves."
    End
@l1:
    IfFlag LEVER_MID @open
    Msg "Something<n>clicks,<n>half-way."
    End
@open:
    Msg "The seal<n>breaks!"
    ForceDoor
    End
@wrong:
    Msg "A grinding<n>noise. It<n>resets."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `LEVER_LEFT/MID/RIGHT` | `117-119` | the levers; the right one must stay untouched |

<a name="training-hall"></a>

<a name="training-hall"></a>
### A training hall - buying experience

The reward mirror of the veterans' door: gold in, experience out, and the engine's own level thresholds do the rest.

Needs: `AtlasDevIfGoldAtLeast`, `GetXP`, `AtlasDevPlaySFX`

```asm
.textbox GENERIC
    AtlasDevIfGoldAtLeast 200 0 0 @afford
    Msg "Training costs<n>200 golds."
    End
@afford:
    IfMsgPrompt "Train for<n>200 golds?" @train
    Msg "Rest, then."
    End
@train:
    LoseGold 200
    GetXP 500
    AtlasDevPlaySFX 9
    Msg "You feel<n>seasoned."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `(fee)` | `200` | in the IfGoldAtLeast and LoseGold lines |
| `500` | `lesson` | experience per session (AddExperience) |

<a name="arena-master"></a>

<a name="arena-master"></a>
### An arena master - clear the beasts for a prize

The first recipe where combat is the quest. The count check reads the live entity slots, so the master knows the beasts are dead without any flag from them - the threshold counts the master's own NPC too, so it is one more than the beasts left.

Needs: `SetFlag`, `IfFlag`, `AtlasDevSpawnEntity`, `AtlasDevIfEntityCountAtLeast`

```asm
define ARENA_OPEN 120
define ARENA_WON 121
.textbox GENERIC
    IfFlag ARENA_WON @champ
    IfFlag ARENA_OPEN @check
    SetFlag ARENA_OPEN
    AtlasDevSpawnEntity $21 $84
    AtlasDevSpawnEntity $21 $86
    AtlasDevSpawnEntity $21 $88
    Msg "Three beasts!<p>Survive them,<n>then return."
    End
@check:
    AtlasDevIfEntityCountAtLeast 2 @fighting
    SetFlag ARENA_WON
    GetGold 800
    Msg "Cleared!<p>Your purse<n>grows heavy."
    End
@fighting:
    Msg "They still<n>breathe."
    End
@champ:
    Msg "The crowd<n>remembers you."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `ARENA_OPEN/WON` | `120-121` | challenge started / prize taken |
| `$21` | `monster id` | who fights (SpawnEntity id) |
| `2` | `threshold` | entity count that still means fighting |

<a name="potion-collector"></a>

<a name="potion-collector"></a>
### A collector - bring me three

Stackable items keep their count in one RAM byte (red potions at $03c6), so IfAddrBetween reads the player's stock directly - AtlasDevIfItemCount counts item slots, and a stack only ever occupies one. The collector inspects rather than confiscates (nothing upstream can take items yet), and the flag keeps the reward single.

Needs: `IfAddrBetween`, `SetFlag`, `IfFlag`

```asm
define BOUNTY_FLAG 122
.textbox GENERIC
    IfFlag BOUNTY_FLAG @paid
    IfAddrBetween $03c6 3 255 @enough
    Msg "Three red<n>potions buy<n>my secret."
    End
@enough:
    SetFlag BOUNTY_FLAG
    GetGold 400
    Msg "All three!<p>The secret is<n>yours."
    End
@paid:
    Msg "I keep no<n>more secrets."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `BOUNTY_FLAG` | `122` | pays out once |
| `$03c6` | `count byte` | red potions stack here; 3/255 is the band |

<a name="nocturnal-merchant"></a>

<a name="nocturnal-merchant"></a>
### A merchant who only trades at night

The night-gated door pattern applied to commerce: the phase byte decides whether the shop exists at all. Requires AtlasDevDayNightCycle in the general hacks list.

Needs: `IfAddrBetween`, `AtlasDevIfGoldAtLeast` — and the hacks `AtlasDevFrameScheduler`, `AtlasDevDayNightCycle`

```asm
.textbox GENERIC
    IfAddrBetween $04e3 2 6 @open
    Msg "We open when<n>the sun dies."
    End
@open:
    AtlasDevIfGoldAtLeast 60 0 0 @sell
    Msg "The night rate<n>is 60 golds."
    End
@sell:
    LoseGold 60
    GetItem ITEM_RED_POTION
    Msg "Sold, shadow<n>to shadow."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `$04e3` | `-` | the day/night phase byte (requires AtlasDevDayNightCycle) |
| `2 / 6` | `dark band` | phases that count as night |

<a name="gambling-den"></a>

<a name="gambling-den"></a>
### A gambling den - double or nothing

Random loot with stakes: the wager leaves first, and the frame counter decides whether twice of it comes back.

Needs: `AtlasDevIfGoldAtLeast`, `IfAddrBetween`

```asm
.textbox GENERIC
    AtlasDevIfGoldAtLeast 100 0 0 @stake
    Msg "Stakes are<n>100. You<n>lack them."
    End
@stake:
    IfMsgPrompt "Double or<n>nothing?<n>100 golds." @flip
    Msg "Wise,<n>perhaps."
    End
@flip:
    LoseGold 100
    IfAddrBetween $001a 0 127 @win
    Msg "The coin<n>laughs at you."
    End
@win:
    GetGold 200
    Msg "Doubled!<p>Walk away<n>while you can."
    End
```

| change this | default | meaning |
| --- | --- | --- |
| `(stake)` | `100` | in the gold check and LoseGold lines |
| `0 / 127` | `odds` | the frame-counter band that wins |
