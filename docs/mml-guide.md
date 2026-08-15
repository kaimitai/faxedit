<hr>

# Faxanadu MML (Music Macro Language) format

<hr>

Creating music for **Faxanadu (NES)** presents a unique challenge: the original game uses raw bytecode tailored to a custom music engine to define music sequences, which is difficult and unintuitive for composers. Our goal is to **provide a layer of abstraction** that allows composers to write music using **MML** - a notation-based format familiar to many chiptune composers. This approach makes music creation more accessible and expressive while preserving full compatibility with the NES sound hardware.

The **MML compiler** translates human-readable musical notation into the **bytecode format** required by Faxanadu’s sound engine. This enables composers to focus on musical ideas rather than low-level implementation details.

The descriptions of the Faxanadu music engine opcodes and their arguments in this document are based on the best information currently available from reverse‑engineering the original engine. While every effort has been made to document them accurately, some behaviors may still be incomplete, ambiguous, or engine‑specific in ways that are not yet fully understood.

Composers are encouraged to **experiment freely**:

- Try unusual argument values  
- Combine opcodes in creative ways  
- Explore how the engine responds in different musical contexts  
- Share discoveries so the documentation can continue to improve  

Faxanadu’s sound engine has many quirks, and part of the fun is uncovering them together. If you find behavior that differs from what’s written here, please treat it as an opportunity to refine and expand our collective understanding.

This tool would not exist without **Christian Hammond’s reverse engineering of Faxanadu’s music engine**, which provided the foundation for understanding how the original system works. His disassembly and research can be found [here](https://chipx86.com/faxanadu/).

Special thanks to [Jessica](https://www.romhacking.net/community/9037/) for helping out with the documentation, and for providing us with high quality well-commented [MML files, which serve as great examples](#example-mml-songs). These can be a used as starting point for learning MML music composition as they exemplify a lot of what is covered in this document.

---

### Table of Contents

<!--TOC-->
  - [The NES Audio Channels](#the-nes-audio-channels)
  - [Timing & Tempo: Making It Simple for Composers](#timing-tempo-making-it-simple-for-composers)
  - [Fractional Drift & Loops: Why Tempo Choice Matters](#fractional-drift-loops-why-tempo-choice-matters)
  - [MML Tools in Echoes of Eolis](#mml-tools-in-echoes-of-eolis)
  - [Command-Line Interface](#command-line-interface)
    - [**Extract MML from ROM**](#extract-mml-from-rom)
    - [**Compile and Inject MML into ROM**](#compile-and-inject-mml-into-rom)
    - [**Extract Songs as MIDI from ROM**](#extract-songs-as-midi-from-rom)
    - [**Convert MML to MIDI**](#convert-mml-to-midi)
  - [MML Syntax Basics](#mml-syntax-basics)
  - [MML File Structure](#mml-file-structure)
    - [Song Header](#song-header)
    - [Channels](#channels)
    - [Minimal Example](#minimal-example)
  - [Musical Notation in MML](#musical-notation-in-mml)
    - [Lengths: Musical vs. Raw](#lengths-musical-vs.-raw)
    - [Tied Notes](#tied-notes)
    - [Octave Control](#octave-control)
    - [Percussion Notes (noise channel)](#percussion-notes-noise-channel)
    - [Loops](#loops)
    - [Song Transpose](#song-transpose)
    - [Channel Transpose](#channel-transpose)
    - [Volume Control](#volume-control)
    - [Control-Flow Opcodes](#control-flow-opcodes)
      - [Subroutines](#subroutines)
      - [Conditional Loop Exit: `!endloopif`](#conditional-loop-exit-endloopif)
    - [Effect Opcodes](#effect-opcodes)
      - [SQ2 Detune: `!detune <n>`](#sq2-detune-detune-n)
      - [SQ2 Pitch Effect: `!effect <n>`](#sq2-pitch-effect-effect-n)
      - [Envelope Mode: `!envelope <n>`](#envelope-mode-envelope-n)
      - [Square Wave Channel Control: `!pulse`](#square-wave-channel-control-pulse)
- [Tempo, Tick Math, and Choosing Safe Tempos](#tempo-tick-math-and-choosing-safe-tempos)
  - [Quarter‑notes, ticks, and how tempo works](#quarternotes-ticks-and-how-tempo-works)
  - [Fractional drift and the fractional accumulator](#fractional-drift-and-the-fractional-accumulator)
    - [Correcting drift manually](#correcting-drift-manually)
- [Tempo Coverage Table](#tempo-coverage-table)
- [ROM to MML](#mml-extracted-from-rom)
- [🎧 MIDI Output and How the Playback VM Works](#midi-output-and-how-the-playback-vm-works)
- [LilyPond export](#lilypond-export)
- [Example mml music](#example-mml-songs)
  - [Pokemon RB - Pallet Town](#pokemon-rb-pallet-town)
  - [Mother - Humoresque of a Little Dog](#mother-humoresque-of-a-little-dog)
  - [Ultima 3 - Wanderer](#ultima-3-wanderer)
- [Troubleshooting](#troubleshooting)
- [Original Songs](#original-songs)
<!--/TOC-->

---

## The NES Audio Channels

The NES sound hardware provides **five channels**, but Faxanadu uses **four** for music:

1. **Square Wave 1 (SQ1)**  
   - Primary melodic channel  
   - Supports **duty cycles**, **volume envelopes**, and **pitch bends**  
   - Often used for lead melodies  

2. **Square Wave 2 (SQ2)**  
   - Secondary melodic channel  
   - Similar capabilities to SQ1  
   - Commonly used for harmony or counter-melody  

3. **Triangle Wave (TRI)**  
   - Produces a fixed-volume triangle waveform  
   - No volume control, but supports pitch changes  
   - Typically used for bass lines  

4. **Noise Channel**  
   - Generates pseudo-random noise patterns  
   - Used for percussion (e.g., snare, hi-hats)

*(The NES also has a DPCM channel for sample playback, but Faxanadu does not use it.)*

---



## Timing & Tempo: Making It Simple for Composers

The original Faxanadu music engine is very low-level:  
- It doesn’t know what a “quarter note” or “tempo” is.  
- It only understands **ticks** (tiny time units) and a command called `set-length` that tells the engine how long each note or rest lasts.  
- The engine runs at **60 ticks per second** on NTSC (or **50 ticks per second** on PAL).

That means the original system is basically saying:  
> “Play this note for 30 ticks, then the next for 15 ticks…”  

Not very musical, right?  

---

### What We Changed

In MML, you don’t need to think about ticks at all. Instead, you write music like you normally would:  
- **Set a tempo** (e.g., `t120` for 120 BPM)  
- Use **musical lengths** (quarter notes, dotted halves, triplets, etc.)  

The compiler does the math behind the scenes:  
- It figures out how many ticks each note should last based on your tempo.  
- It inserts the right `set-length` commands automatically.  

So you can focus on **music**, not math.

---

### Example

```mml
t120
l4 c d e f    ; Four quarter notes
l8 g a b c    ; Four eighth notes
```

At 120 BPM:  
- A quarter note lasts **½ second**  
- The compiler converts that to **30 ticks**
- You never have to worry about those numbers - the compiler handles it.

---

### Why This Matters

- **Composers think in beats, not ticks.**  
- MML gives you a familiar musical language.  
- The compiler ensures everything lines, even for dotted notes, triplets, and odd note fractions.

---


## Fractional Drift & Loops: Why Tempo Choice Matters

Behind the scenes, every note length is converted into **ticks**, and ticks are whole numbers.  
When you use **loops** or **subroutines (jsr)** in MML, the compiler calculates tick lengths for each note and rounds them to the nearest integer.  

This works fine for most music, but there’s a catch:  
- If your tempo produces **fractional tick values** (e.g., 12.5 ticks for an eighth note), the compiler rounds them.  
- Over time, these tiny rounding differences can **add up** when the same pattern repeats many times.  
- This is called **fractional drift**—your loop might end slightly early or late compared to other channels.

---

### Why Can’t the Compiler Fix This Automatically?

The compiler does its best:
- It uses an **error accumulator** to spread rounding differences evenly (e.g., alternating 12 and 13 ticks for eighth notes).
- This keeps measures aligned **most of the time**.

But if you loop a section hundreds of times, even small rounding errors can accumulate.
The engine itself doesn’t know about measures or beats—it just plays ticks.

---

### How to Avoid Drift

Choose tempos that produce **clean tick values** for common note lengths.  
For example:
- At **120 BPM (NTSC)**, a quarter note = 30 ticks (perfect integer).
- At **100 BPM (NTSC)**, a quarter note = 36 ticks (also clean).
- At **125 BPM**, a quarter note = 28.8 ticks (fractional → rounding needed).

We’ll provide a **recommended tempo chart** later in this documentation so you can pick tempos that minimize and eliminate drift.

---

### TL;DR for Composers

- Loops and subroutines repeat set patterns many times, so rounding errors can accumulate.
- Stick to recommended tempos for best results.
- If you use unusual tempos, expect tiny timing differences after long loops—but it might not necessarily be audible unless the loop runs for a very long time.

---

## MML Tools in Echoes of Eolis

The MML tools are available directly from the **Scripting &gt; MML (music)** tab in Echoes of Eolis.

The GUI provides commands for:

- Compiling MML and injecting the resulting music into the current ROM
- Decompiling the current ROM's music to MML
- Exporting music from the ROM to MIDI
- Exporting MML to MIDI
- Exporting music to LilyPond

The same functionality is also available from the `eoe-cli` command-line application, as described below.

---

## Command-Line Interface

The `eoe-cli` CLI application provides several commands to work with MML files and the Faxanadu ROM:

---

### **Extract MML from ROM**
```bash
eoe-cli xmml "faxanadu (u).nes" "faxanadu.mml"
```
**Description:**  
Extracts the music layer from `faxanadu (u).nes` and saves it as an MML file named `faxanadu.mml`.

---

### **Compile and Inject MML into ROM**
```bash
eoe-cli bmml "faxanadu.mml" "faxanadu.nes" -s "faxanadu (u).nes"
```
**Description:**  
Compiles and injects the music layer from `faxanadu.mml` into `faxanadu.nes`.  
- The `-s` option specifies a source ROM (`faxanadu (u).nes`) for reference.  
- If no source ROM is provided, the output file will be patched directly.

---

### **Extract Songs as MIDI from ROM**
```bash
eoe-cli r2m "faxanadu (u).nes" faxanadu
```
**Description:**  
Extracts all songs from `faxanadu (u).nes` into MIDI files named:  
`faxanadu-01.mid` up to `faxanadu-16.mid`.

---

### **Convert MML to MIDI**
```bash
eoe-cli m2m faxanadu.mml faxanadu
```
**Description:**  
Translates all songs in `faxanadu.mml` into MIDI files named:  
`faxanadu-01.mid` up to `faxanadu-16.mid` (depending on the number of songs defined in the MML).  
This command is **independent of any ROM file** and can be used as a general MML renderer.

---

## MML Syntax Basics

When you extract a ROM to MML using `eoe-cli xmml`, the top of the file will include comments like:

```mml
; global transpose for channel sq1: -12 semitones
; global transpose for channel sq2: -12 semitones
; global transpose for channel tri: 12 semitones
; global transpose for channel noise: 127 semitones
```

### What This Means
- These are **global transpositions** applied by the Faxanadu music engine and stored in the ROM.
- **SQ1 and SQ2**: Tuned **down one octave** (`-12 semitones`).  
  - If you write `C` in **octave 4**, the engine plays `C` in **octave 3**.
- **Triangle (TRI)**: Tuned **up one octave** (`+12 semitones`).  
  - `octave 4 C` in MML becomes `octave 5 C` in the engine.
- **Noise**: Has a transpose value (`127 semitones`), likely an unused sentinel.
- Anything after `;` on any line is a **comment** and ignored by the compiler.

---

### Key Points for Composers
- These transpositions are **automatic**; you don’t need to adjust your notation but you need to be aware of them.
- Just write music normally in MML—octaves and notes as you expect.
- Comments (`;`) are safe to add anywhere for your own notes.

---


## MML File Structure

The MML is a **list of songs**, each with **four channels**. A song starts with its header, followed by channel blocks.

### Song Header
- `#song <song number>` — identifies the song.
- `t<tempo>` — default tempo for the whole song (quarter notes per minute).  
  Can be overridden by per-channel tempo commands inside the channel blocks.

### Channels
Four channel blocks follow, each enclosed in `{ }`:
- `#sq1 { ... }`
- `#sq2 { ... }`
- `#tri { ... }`
- `#noise { ... }`

### Minimal Example
```mml
#song 1
t120

#sq1 {
!end
}

#sq2 {
!end
}

#tri {
!end
}

#noise {
!end
}
```

**Notes:**
- The tempo in `t<tempo>` applies to the entire song unless overridden inside a channel.
- Tempo can be fractional, for example ```t112+1/2``` means a tempo of 112.5 quarter notes per minute.

## Musical Notation in MML

- **Notes:** Use letters `c d e f g a b`.  
  - Sharps: `+` or `#` (e.g., `c+` or `c#`)  
  - Flats: `-` (e.g., `d-`)

- **Rests:** Use `r`.

### Lengths: Musical vs. Raw

- **Musical lengths**:  
  - `l<n>` sets the default note and rest length in musical terms (quarter, eighth, etc.).  
    Example:
    ```mml
    l8 c d e    ; same as c8 d8 e8
    ```
  - Dotted notes: add a dot after the length.  
    Example:
    ```mml
    l4. c       ; dotted quarter note
    ```

    The mml parser will accept any length you give it and convert it to ticks, even if it makes little musical sense. For example is ```c15..``` a double dotted 15th note.

- **Raw lengths**:  
  - Use `l~<ticks>` to set the length in **raw ticks**, bypassing tempo calculations.  
    Example:
    ```mml
    l~4 c       ; note lasts exactly 4 ticks
    ```
  - This is only useful for precise timing or effects that don’t depend on BPM. Since Faxanadu allows 254 different note lengths, we can't fit them all on a musical grid necessarily. When extracting ROM to MML for example, the decompiler does its best to infer a tempo that puts as many tick lengths as possible on a musical grid, but for some cases it will not be possible - and in these cases we use ~ for raw tick-lengths. They are best avoided by composers in most cases, since their length is the same under any tempo.

**Summary:**  
- `l<n>` → musical length (tempo-based).  
- `l<n>.` → dotted musical length.  
- `l~<ticks>` → raw tick length (absolute timing).


**Summary:**  
- `l<n>` sets the default length for subsequent notes and rests.  
- Individual notes and rests can override the length by appending a number and optional dots.



### Tied Notes

- Use `&` to tie two notes together:
  ```mml
  a4 & a8
  ```
  This means the note `a` lasts for the combined length of a **quarter note + eighth note**.

- The compiler will **collapse ties** into a single note:
  ```mml
  a4 & a8  →  a4.   ; dotted quarter
  ```

#### Rules for Ties
- **Pitch must match** (e.g., `a4 & a8` is valid, but `a4 & b8` is not).
- **Notes must be consecutive** — no other command can come between them.
- Ties work across **length changes** (e.g., `a8 & a16` combines both lengths).

---

**Why use ties?**
- Useful for writing sustained notes that span irregular lengths.
- The compiler optimizes ties into the shortest representation when possible.

### Octave Control

- **Set octave:**  
  ```mml
  o<n>
  ```
  Example:
  ```mml
  o4    ; sets current octave to 4
  ```

- **Shift octave:**  
  - `>` increases the octave by 1  
  - `<` decreases the octave by 1  
  Example:
  ```mml
  o4 c d > e f    ; e and f will play in octave 5
  ```

---

#### Important Note
Octaves are **not part of the Faxanadu engine**. The engine uses **absolute note values**, not octaves.  
We include `o<n>`, `>` and `<` in MML for consistency with standard syntax, but when compiling, these are **collapsed into concrete note numbers**.

This means that the octave commands do not emit any bytecode, and are only used by the compiler to emit the correct notes.

**Implication:**  
- In loops or subroutines (`jsr`), the octave cannot change dynamically between iterations because the engine only sees absolute notes.
- **Good practice:** Always set the octave explicitly at the start of each loop or subroutine target:
  ```mml
  [o4 c d e f]2    ; loop starts with explicit octave
  ```

---

### Percussion Notes (noise channel)

Percussion works a little differently from melodic channels:

- **Length is shared:**  
  Individual percussion hits cannot have their own length.  
  Instead, set the lengths using `l<n>` (just like normal notes), then write percussion notes and rests.

- **Percussion notes:**  
  `p0, p1, p2, p3, p4, p5, p6 and p7`  
  (In Faxanadu’s original data, only `p1`, `p2`, and `p3` are used. Perhaps we can think of p1, p2 and p3 as kick, snare and hi-hat)

- **Repeats:**  
  You can repeat a percussion note by adding `*<count>` after it:  
  - `p1*5` → plays `p1` five times  
  - Valid repeat counts: `1–15 and 256`
  - If no repeat is given, the note plays once.

- **Rests:**  
  Use `r` for rests between percussion hits.

**Example:**
```
mml
l4 p1 p2 p3 r p2*4
```

This sets length = quarter note, then plays p1, p2, p3, a rest, and p2 four times.

---

### Loops

- Loops are enclosed in square brackets `[...]` followed by an optional repeat count:
  ```mml
  [o4 c d > e f ]4    ; repeat the sequence 4 times
  [o4 c d > e f ]     ; infinite loop
  ```

#### Rules
- If **no counter** is given, the loop is **infinite**.
- Loops **cannot be nested**, except when one is infinite and the other is finite.  
  - This is because under the hood, infinite loops are modeled using `push address` and `pop address and jump`, which behave differently from finite loops.

---


### Song Transpose

- Use `S_<n>` to transpose the entire song by **n semitones** (can be negative):
  ```mml
  S_-12    ; transpose down one octave
  S_5      ; transpose up 5 semitones
  ```

#### Details
- This command affects **all melodic channels** in the song.
- It is **sparingly used** in the original game data.
- Recommended placement: **early in the SQ1 channel**, so it’s clear and applied before any notes.
- Example:
  ```mml
  #sq1 {
    S_-12
    l4 o4 c d e f
    !end
  }
  ```

**Note:**  
This is a global transpose for the song, separate from the ROM’s built-in global transpositions (shown in comments when extracting).


### Channel Transpose

- Use `_<n>` to transpose the **current channel** by **n semitones** (can be negative):
  ```mml
  _-12    ; transpose this channel down one octave
  _5      ; transpose this channel up 5 semitones
  ```

#### Details
- Unlike `S_<n>` (song transpose), this only affects the **channel where it appears**.
- Can be placed anywhere in the channel block, and used in loops to transpose up and down, for example.
- Example:
  ```mml
  #sq2 {
    l2 [o4 c d _-12 e f _0]5
    !end
  }


### Volume Control

- Use `v<n>` to set the **volume** for the current channel:
  ```mml
  v8    ; set volume to 8
  v15   ; maximum volume
  v0    ; silent
  ```

#### Details
- Volume applies **only to square wave channels** (`#sq1` and `#sq2`).
- Valid range:  
  - `0` = silent  
  - `15` = maximum volume
- Example:
  ```mml
  #sq1 {
    v12
    o4 l4 c d e f
  }
  ```


### Control-Flow Opcodes

These special commands control the flow of music in a channel:

- **`!start`**  
  Marks the **entry point** for the music in the current channel.  
  - If omitted, the entry point is assumed to be the **start of the channel block**.  
  - Reason for existence: It’s theoretically possible to have a subroutine with a return before the entry point.
  - It can also be used to make a song start from different places during testing
  ```mml
  !start
  ```

- **`!end`**  
  Ends the music for the channel.  
  - **Must be present** if the control flow would otherwise spill out of the channel block. Can be omitted if a channel is in an infinite loop or ends with !restart.
  ```mml
  !end
  ```

- **`!restart`**  
  Restarts the channel from its entry point.  
  - Needed for looping entire songs and their channels.
  ```mml
  !restart
  ```

---

### Subroutines

- **Label definition**  
  Define a label with `@label:` (note the trailing colon):
  ```mml
  @intro:
  ```
  - Labels mark a target location for subroutine calls.

- **Jump to subroutine**  
  Use `!jsr @label` to jump to a label (label name **without** the colon):
  ```mml
  !jsr @intro
  ```
  - Execution continues at `@intro:` until a return.

- **Return from subroutine**  
  Use `!return` to return to the command **after the last `!jsr`**:
  ```mml
  !return
  ```

#### Rule/Constraint
- You **should not** jump to a subroutine **from another subroutine**.  
  - Only **one** return address can be on the stack at any time.
- You can not jump to a label within an other channel block - all channels are self-contained.


### Example: Using Subroutines

```mml
#sq1 {
  !start
  o4 l4
  c d e f
  !jsr @intro    ; jump to subroutine
  g a b
  !end

  @intro:        ; subroutine definition
  o4
  c e g
  !return        ; return to after !jsr
}
```

**Explanation:**
- The main sequence plays `c d e f`, then jumps to `@intro`.
- The subroutine plays `c e g` and returns.
- After returning, the channel continues with `g a b` before ending.


### Conditional Loop Exit: `!endloopif`

`!endloopif <n>` exists a loop if &lt;n&gt; iterations have passed.  
When execution reaches this opcode, it **checks how many iterations have already completed**; if **`<n>` iterations have passed**, it **jumps to the end of the current loop** and continues from there.

#### Correct Usage Example
```mml
#sq1 {
  o4 l4
  ; Repeat the pattern 4 times, but skip the remainder of the last pass:
  [ 
    c d e f
    !endloopif 3   ; When entering the 4th pass (after 3 completed), jump to loop end
    g a             ; Only plays on passes 1–3
  ]4

  !end
}
```

**What happens here:**
- Pass 1: `c d e f`, `!endloopif 3` (condition not met), `g a` plays.
- Pass 2: same as Pass 1.
- Pass 3: same as Pass 1.
- Pass 4: `c d e f`, `!endloopif 3` **triggers** (3 passes have completed), execution **jumps to the end of the loop**, and **`g a` is skipped**. Control continues **after** the loop.

When the loop counter is 2, you can think of !endloopif as being used for first and second endings.

---

#### Rules & Warnings
- Place `!endloopif` **inside the loop**—it evaluates when the opcode is reached.
- It is **only useful** for ending a loop **during its last iteration** (e.g., in a `]4` loop, use `!endloopif 3`).
- Do **not** use it before the loop-end position has been seen at least once (i.e., don’t rely on it in the very first pass).
- Triggering it **after a loop has finished** will cause the **program counter to jump backward**, effectively creating an **infinite loop**.

**Summary:**  
Use `!endloopif <n>` to cleanly skip the tail of a loop during the final pass. Keep it inside the loop body, and select `<n>` as **(repeat count − 1)** for predictable behavior.

### Effect Opcodes


### SQ2 Detune: `!detune <n>`

Applies a **SQ2-only** pitch bias (detune). This subtracts a small value from the **low byte of the SQ2 timer period** before writing to the APU. The value is compared against the computed timer low byte and **clamped** to avoid underflow, producing a **subtle upward pitch shift** when non‑zero.

```mml
#sq2 {
  !start
  o4 l8 c d e f

  !detune 2     ; slight upward detune on SQ2 only
  c d e f

  !detune 0     ; turn off detune
  g a b c
  !end
}
```

**Details**
- **Channel:** Only affects **SQ2**.
- **Effect:** Small upward pitch shift by biasing the **timer low byte** post–note calculation, pre-APU write.
- **Timing:** Not related to note length or envelopes; it’s a **pitch-only** modification.
- **Use case:** Simple **chorus/harmony**-like effect on SQ2.
- **Disable:** `!detune 0` disables the effect.
- **Range:** `n` is an integer; nonzero values increase the bias. (Exact safe range depends on engine clamping—use modest values.)


### SQ2 Pitch Effect: `!effect <n>`

Applies a **vibrato effect** to **SQ2 only**.  
This opcode is extremely rare in the original Faxanadu data (only 3 uses total).

```mml
#sq2 {
  !start
  o4 l8 c d e f

  !effect 2    ; enable subtle pitch modulation
  g a b c

  !effect 0    ; turn off effect
  !end
}
```

#### What It Does
- Introduces a **delayed pitch modulation** after the envelope phase reaches a certain point.

#### Practical Notes
- `!effect 0` disables the effect.
- Non-zero values (e.g., `2`) define the **depth mask**, controlling how far the pitch can shift.
- This is **not related to note length or envelopes**—it’s a **pitch-only modulation** applied after note calculation.

**Summary:**  
`!effect <n>` adds a subtle pitch modulation to SQ2, likely for a vibrato effect.  
Values:
- `0` = off
- `2` = light modulation (as seen in original data)
- Any byte value (```0-255```) is allowed as an argument, but not all values will produce reasonable effects


### Envelope Mode: `!envelope <n>`

Sets the **envelope mode** for **square wave channels** (`#sq1` and `#sq2` only).  
This opcode is **always present at the start of SQ1 and SQ2 channels** in the original game data.

```mml
#sq1 {
  !start
  !envelope 0    ; volume decay
  o4 l8 c d e f

  !envelope 2    ; attack/decay curve
  g a b c
  !end
}
```

#### Supported Values
- `0`: **Volume decay**  
  Smoothly ramps down the volume.
- `1`: **Curve but held**  
  Clamps the envelope into a range (0..15) and slows the decay.
- `2`: **Attack/decay curve**  
  Applies a pre-built pitch envelope: pitches up then down in a set pattern.

---

#### Constants for Readability
You can use predefined constants instead of raw numbers:
```mml
!envelope $ENV_VOLUME_DECAY          ; same as !envelope 0
!envelope $ENV_CURVE_HELD            ; same as !envelope 1
!envelope $ENV_ATTACK_DECAY_CURVE    ; same as !envelope 2
```

**Details**
- Only applies to **SQ1** and **SQ2**.
- Always set at the **start of the channel** before any notes.
- Independent of note length or timing—purely an envelope/pitch behavior.

---

### Square Wave Channel Control: `!pulse`

**Purpose:**  
Configures the Square Wave channel control byte for **SQ1** or **SQ2**. This byte determines the duty cycle, length counter behavior, and volume mode (constant or envelope).

---

#### Byte Composition
The control byte is built from **4 arguments** for the current channel:

| Argument         | Description                                      |
|------------------|--------------------------------------------------|
| **Duty Cycle**   | Waveform shape: 12.5%, 25%, 50%, or 75%         |
| **Length Mode**  | Run or Halt length counter                      |
| **Volume Mode**  | Constant volume or envelope mode                |
| **Volume Value** | 0–15 (lower nibble)                             |

---

##### Constants for MML Parser
Instead of raw numbers, use these constants:

### Duty Cycle
- `$DUTY_12_5` → 0  
- `$DUTY_25` → 1  
- `$DUTY_50` → 2  
- `$DUTY_75` → 3  

### Length Counter
- `$LEN_RUN` → 0  
- `$LEN_HALT` → 1  

### Volume Mode
- `$ENV_MODE` → 0  
- `$CONST_VOL` → 1  

---

### Usage in Script
```mml
!pulse $DUTY_75 $LEN_RUN $CONST_VOL 0
 ```

---

### Composer Notes

The `!pulse` command sets the **basic tone shape** for the square wave channels (SQ1 and SQ2).  
Think of it as choosing the **waveform flavor** and a few playback options:

- **Duty cycle** changes the character of the sound:
  - 12.5% = thin and nasal
  - 25% = bright
  - 50% = balanced
  - 75% = not used much in practice, as it basically sounds the same as 25%
- **Length counter** and **volume mode** are always set to “run” and “constant” in Faxanadu, so you don’t need to worry about them.
- The last number (0-15) works as the base envelope volume, although the original game doesn't often use other constants than 0 here. If nonzero, enevelope 0 sustains a little bit instead of going completely silent.

**Tip:** Use `!pulse` mainly to pick the duty cycle for the tone color. The envelope and volume opcodes will handle the actual loudness and expression.


### Opcode: `!nop`

**Purpose:**  
`!nop` stands for “no operation.” It does absolutely nothing when executed.

**Why does it exist?**  
It’s not used anywhere in Faxanadu’s original music data. Most likely, it’s a leftover from Hudson’s shared sound engine, which was adapted for multiple games. Some engines included `NOP` as a placeholder or for alignment purposes, but Faxanadu never needed it.

**Composer Notes:**  
You can safely ignore this opcode. It has no effect on playback and is only here for completeness.

---

# Tempo, Tick Math, and Choosing Safe Tempos

## Quarter‑notes, ticks, and how tempo works
The engine measures time in **ticks**, not **BPM**.  
Internally, one minute is treated as **3600 ticks**, so the number of ticks per quarter‑note is:

T = 3600 / tempo (in quarter-notes per minute)

A tempo of **150 q/min** gives:

T = 3600/150 = 24 ticks per quarter

This value must be an integer for musical durations to land cleanly on the grid. When it isn’t, durations accumulate fractional remainders.

## Very fast tempos: short notes collapse to 0 ticks
At high tempos, T becomes small.  
If T is too small, dividing it into sixteenth, thirty‑second, or dotted values can produce:

- 0‑tick durations (notes that vanish). The compiler will not allow this.

This is why extremely fast tempos must be chosen carefully.

## Very slow tempos: long notes exceed the 255‑tick limit
At slow tempos, T becomes large.  
Whole notes, dotted whole notes, or tied notes can exceed the engine’s maximum duration:

- **255 ticks** (the largest representable length)

When this happens, compilation will also fail - as it is impossible to emit notes that long.

---

## Fractional drift and the fractional accumulator
When T is not divisible by common musical fractions (½, ¼, ⅛, dotted, triplet), durations produce **fractional remainders**. The compiler uses a **fractional accumulator** to keep timing accurate:

- Each note adds its fractional remainder to the accumulator
- When the accumulator crosses 1.0, an extra tick is inserted
- This keeps linear playback correct

However, this system cannot guarantee correctness inside:

- loops
- subroutines
- patterns that repeat many times

Fractional drift inside loops cannot be corrected automatically, since the note lengths remain fixed for each iteration.

Note also that the fractional corrections will increase the size of your bytecode as length-setting code has to be inserted. If a quarter note has tick length 11 for example, the eighth notes will have a tick length of 5.5.

When you then write ```l8 c c``` this will compile to ```l~5 c l~6 c``` since the eighth notes must sum to 11 for channels to stay in sync over time.

### Correcting drift manually
If a looped passage drifts over time, composers can insert:

- synthetic rests (e.g., r64)
- explicit raw ticks (advanced)

If composers don't use any loops or subroutines for a channel, the fractional accumulator will have an easy job correcting the output as it goes along.

## Tempo Coverage Table
It is best to avoid fractional tick lengths for your notes completely, both for channels to stay in sync when using loops and subroutines, but also for bytecode size reasons. If the compiler needs to insert a lot of length-setting commands to keep notes aligned, the byte output can grow significantly.

The table below lists all tempos between 30 and 300 with fraction-free whole notes and 8th notes. For each tempo:

- **✅** means the duration fits perfectly (no fractional remainder)
- **❌** means the duration produces fractional drift
- **⛔** means the duration is outside the allowed tick range (too short or too long)

The coverage column is just a count of "well behaved notes" for the tempo.

Use these tempos, and the notes marked with ✅ for that tempo, if possible - to avoid drift and ensure clean bytecode output.

| tempo (q/min) | T (ticks/q) | coverage | 1 | 2 | 4 | 8 | 16 | 32 | 1. | 2. | 4. | 8. | 16. | 3 | 6 | 12 | 24 |
|:-------------:|:-----------:|:--------:|:-:|:-:|:-:|:-:|:--:|:--:|:--:|:--:|:--:|:--:|:---:|:-:|:-:|:--:|:--:|
| 150           | 24          | 15       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ✅ | ✅ | ✅  | ✅  |
| 75            | 48          | 14       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ⛔  | ✅  | ✅  | ✅  | ✅   | ✅ | ✅ | ✅  | ✅  |
| 100           | 36          | 13       | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ✅  | ✅  | ✅  | ✅  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 300           | 12          | 13       | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ✅  | ✅  | ✅  | ✅  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 60            | 60          | 12       | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ⛔  | ✅  | ✅  | ✅  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 85+5/7        | 42          | 11       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 90            | 40          | 11       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |
| 112+1/2       | 32          | 11       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |
| 120           | 30          | 11       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 200           | 18          | 11       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 225           | 16          | 11       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ✅  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |
| 64+2/7        | 56          | 10       | ✅ | ✅ | ✅ | ✅ | ✅  | ✅  | ⛔  | ✅  | ✅  | ✅  | ✅   | ❌ | ❌ | ❌  | ❌  |
| 66+2/3        | 54          | 10       | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ⛔  | ✅  | ✅  | ❌  | ❌   | ✅ | ✅ | ✅  | ✅  |
| 128+4/7       | 28          | 9        | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ✅  | ✅  | ✅  | ✅  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 180           | 20          | 9        | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ✅  | ✅  | ✅  | ✅  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 69+3/13       | 52          | 8        | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ⛔  | ✅  | ✅  | ✅  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 81+9/11       | 44          | 8        | ✅ | ✅ | ✅ | ✅ | ✅  | ❌  | ⛔  | ✅  | ✅  | ✅  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 94+14/19      | 38          | 7        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 105+15/17     | 34          | 7        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 138+6/13      | 26          | 7        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 163+7/11      | 22          | 7        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 257+1/7       | 14          | 7        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ✅  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 58+2/31       | 62          | 6        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ⛔  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 62+2/29       | 58          | 6        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ⛔  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 72            | 50          | 6        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ⛔  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |
| 78+6/23       | 46          | 6        | ✅ | ✅ | ✅ | ✅ | ❌  | ❌  | ⛔  | ✅  | ✅  | ❌  | ❌   | ❌ | ❌ | ❌  | ❌  |

150 is the golden tempo for which all normal note lengths resolve to integral tick lengths.

If you can forego triplets or 32nd notes, there are other good options.

85+5/7 and 112+1/2 are very strange tempos indeed, but they too fall neatly on the musical grid. While they make little sense to composers, the point is that it produces integer tick lengths.

With a tempo of 85+5/7 for example, the quarter note tick length becomes 3600/(85+5/7) = 42 ticks exactly. So while it looks like a senseless tempo, all normal notes apart from 16th and 32nd notes and their dotted versions will have integer tick lengths - as we can see in the table above. In the world of Faxanadu music this is actually a very nice tempo value.

Note that for any tempo you can tie any number of fraction-free notes together, and the tie will stay fraction-free too - but the total tick length must resolve to at most 255.

Note also that all quarter notes in this table have integer tick lengths. If you choose a tempo like 132, the quarter note will have a tick length of 3600/132 which is 27+3/11. This will cause fractional notes, so it is recommended to at least use a tempo which makes quarter notes integers - even if you don't use a tempo in the table above.

---

## MML extracted from ROM

The Faxanadu music tools provide a **perfect round‑trip at the bytecode level**, not at the MML level.

- **MML → bytecode** is exact and authoritative.  
  The compiler produces the same bytecode the engine would expect, with no ambiguity.

- **bytecode → MML** is a *best‑effort reconstruction*.  
  The extractor attempts to infer:
  - a tempo that explains as many durations as possible,
  - musical note lengths that match the observed tick values,
  - and readable MML structure.

However, the extractor has important limitations:

- It only checks durations against a fixed set of **primitive musical fractions**.  
- It does **not** attempt to interpret long durations as **combinations of tied notes**.  
- It does **not** attempt to **split unusual durations into ties**.  
- Composite durations created by ties in MML become a **single tick length** in the ROM, and may not match any primitive fraction under the inferred tempo.  
- When no clean musical representation exists, the extractor falls back to **raw tick lengths** (`l~<ticks>`).

Because of this, the MML produced from a ROM may differ from the original MML used to create it. This is expected: the ROM does not preserve musical structure, only absolute tick durations.

**Summary:**  
- *Bytecode round‑trips perfectly.*  
- *MML does not, because musical structure is not stored in the ROM.*  
- *For composing new music, always treat your MML source as the master.*

---

## MIDI Output and How the Playback VM Works

### 🧩 Per‑channel virtual machine
Each channel is rendered by its **own internal virtual machine**, running independently of the others. This VM executes the fully expanded event stream for that channel:

- **Loops are unrolled**
- **Subroutines (`jsr`) are inlined**
- **All control flow becomes linear**

Because the VM sees a **finite, linear sequence** of events, it never risks running forever. As soon as the channel reaches a previous state, the VM reaches equilibrium and stops.

This design guarantees that MIDI export is:

- deterministic  
- bounded  
- free of infinite loops  
- safe for arbitrarily complex MML  

### 🎼 No fractional drift in MIDI
Since the MIDI exporter works on the **fully unrolled** event stream, it can apply fractional‑tick correction continuously:

- Every duration is converted to ticks  
- Any fractional remainder is accumulated  
- Corrections are applied immediately  
- No drift can accumulate across loops or subroutines  

This differs from the in‑engine compiler, which cannot handle fractional drift for more than one loop iteration or a call to subroutine. The MIDI exporter *can*, because it expands everything first. The result is **perfectly stable timing** in the MIDI output, even for passages that would drift inside the NES engine.

### 🧪 Testing compositions in the real engine
While MIDI export is ideal for checking musical correctness, the **final authority** is always the real engine running inside the ROM.

The easiest way to test your composition in‑game is:

- Patch your MML into the ROM  
- Load the ROM in an emulator
- **Song #1 plays immediately on boot**, because it is the intro theme  

This makes song #1 the most convenient slot for rapid iteration:

- no menu navigation  
- no triggers required  
- instant playback on reset  

For deeper testing, you can assign your composition to any song index, but the intro slot is the fastest workflow during development.

---

## LilyPond export

[Lilypond](https://lilypond.org/) is a music engraving program, which is free, open-source, and part of the [GNU Project](https://www.gnu.org/).

Echoes of Eolis can export an mml file - or music directly from ROM - to LilyPond files.

The command

```
eoe-cli r2l faxanadu.nes faxanadu
```

will read music data from faxanadu.nes and output them to faxanadu-01.ly to faxanadu-16.ly.

The command

```
eoe-cli m2l faxanadu.mml faxanadu -lp
```

will export all songs in faxanadu.mml to individual LilyPond files, starting with faxanadu-01.ly - and with the ```-lp``` option enabled, the percussion channel will be included as a drum staff in the output. This is optional, and disabled by default.

The LilyPond application can then turn an .ly-file into a pdf with the engraved music. 

After installing LilyPond, and having the lilypond executable reachable from your command line, the command:

```
lilypond faxanadu-01
```

will turn faxanadu-01.ly into faxanadu-01.pdf - which engraves the song as a musical score - as well as faxanadu-01.mid, which is a midi version of the music.

This provides an alternative way to turn MML into midi.

Some directives can be provided in the mml file to influence the LilyPond pdf output.

There are song directives for time signature and song title, which are strings enclosed in double quotes. For example, starting a song with:

```
#song 1
#title "Tom Lehrer - New Math"
#time "3/4"
```
will make LilyPond show the given title at top of the pdf, and use time signature 3/4 in the engraving. Default time signature for a song, if not given, is 4/4. Default title is just "Song &lt;n&gt;" where n is the song number.

There is also a channel-specific directive for the melodic channel which sets the clef.

For example, starting a triangle channel with:

```
#tri {
#clef "bass"
```

will use the bass clef for this channel in the engraving. By default, if a value is not given, the treble clef will be used.

Other possible clef values can be seen in [LilyPond's documentation](https://lilypond.org/doc/v2.24/Documentation/notation/clef-styles).

As for the midi converter, the LilyPond output will linearize the entire song - it will unroll loops and calls to subroutines.

Note: The Eolis theme in the original game data has an incredibly long percussion channel because it uses a lot of percussions that are repeated 256 times, probably by error. Lilypond exports of this song with the drum staff enabled will emit hundreds of bars.

---

## Example mml songs

The following three MML songs were graciously provided by Jessica ([RHDN profile](https://www.romhacking.net/community/9037/)).

### Pokemon RB - Pallet Town

```
; Pokemon RB - Pallet Town
#song 9
; Song 9 is the theme for most towns

t120
; 120 bpm (march tempo).
; At this tempo, 8th notes are 15 frames, which means
; we don't have symmetrical 16th notes.
; Luckily, this song doesn't use 16th notes.

#Sq1 {
	!start	
	; start is not necessary, strictly speaking, unless you write
	; a subroutine above it.

	!pulse $DUTY_50 $LEN_RUN $CONST_VOL 0
	; 50% duty cycle (sounds kind of like a clarinet).
	; Always put LEN_RUN and CONST_VOL. These are part of the 
	; byte that is written to the volume and duty cycle registers
	; by Faxanadu's engine. You don't need to understand this 
	; so don't worry too much!
	; I think the trailing 0 sets a base volume level. Each note 
	; ordinarily fades to 0 volume. Setting this higher will
	; allow a little sustain volume.
	
	!envelope 0 
	; Envelope 0 is a gradual decay.
	; Envelope 1 is sustain.
	; Envelope 2 (not used much) is a crescendo and decrescendo.
	
	v13 o7 l8
	; Slightly louder than Sq2.
	; I recommend not using volume 14 and 15 because the square
	; channels tend to be louder than everything else.
	; Octave 7 may seem high but it's just octave 5 in famitracker.
	; l8 means that a note without a number after it is an 8th note.
	; This is how other versions of MML work too.

	dc < ba > ge f+e 
	; Type a through g to play a note and r to play a rest.
	; + signifies a sharp and - signifies a flat.
	; To go down or up octaves, use < and >.
	; OR specify the octave with o2, o3, etc.

	d4. < b gg ab > 
	; Numbers after notes are their rhythms. 4. is a dotted quarter.
	; I find it useful to write 1 measure per line.
	; Most people do not use as much whitespace as I do, 
	; but that's OK because the MML interpreter will get rid of it.
	
	c4. r4 < f+ ga
	b4. > c16 < b16 a4. r >
	dc < b > d gf+ f+g
	e4. d d4. r 
	c < bag > dc < ba 
	g4. r4 gab >
	c4. r d4. c <
	; c4. r could be written as a half note (c2)
	; but I wanted it to cut off early.
	
	b4. r4 gab >
	c4 c4 d4. c16 d16 <
	b4. r4 bag l4
	a4. r8 eb 
	a4. r8 ge
	f+4. r8 gb
	b4. r8 a4. r8
	!restart
	; restart is only necessary in one channel
}

#Sq2 {
	!start
	!pulse $DUTY_50 $LEN_RUN $CONST_VOL 0
	!envelope 0 

	[ v11 o5 l8 
	; This channel is a little quieter because Sq1 has the melody.
	; Anything inside of [ ] is a loop. If there is a number after
	; the ], it will loop that many times.
	; Otherwise, it is an infinite loop.
	; You can put a second loop inside an infinite loop, but you
	; cannot use a finite loop inside another finite loop.

	b4 > c d4 gdc <
	b4 g > d4 d !endloopif 1 c < b 
	rb > c < b > c4. r4 <
	b > c < abgaf+ ]2
	o6 g f+ e4 d c4 < 
	ab > cdc < ba g4 f+4

	[ o6 c < geg > d < a f+ a
	bgdgbgdg ]2
	; Use repeats whenever possible to save space in the ROM.
	[o5 aece ]4
	f+ dcdgece
	gece f+ dcd
}

#Tri {
	!start
	[ o4 l4
	; default length is 4 in this block instead of 8,
	; because this channel has no 8th notes.
	; Unlike the square channels, o4 is the same in Famitracker.
	; Triangle doesn't have volume or duty cycles.

	g4. e4. f+
	g4. a4. g
	e4. f+4. !endloopif 1
	; this breaks the loop if it has played all the way through once.
	; The number you use with !endloopif should be 1 less than
	; the total number of loops (in this case, two). 

	e g4. f+4. d ]2 ; 1st ending
	l4 a g4. e4. d ; 2nd ending.
	; l4 has to be specified again because the last note
	; before endloopif was a different length.

	c2 d2 g2 ed 
	c2 d2 g2 ag
	l2 eaeg f+ 
	; I set l2 here because there are so many half notes
	e1 f+
}

#Noise {
	!end
	; You have to specify the noise channel, 
	; even if there is nothing in it.
}
```

---

### Mother - Humoresque of a Little Dog

```
#song 15
; Mother - Humoresque of a Little Dog
; Song 15 is the shop theme.

t225
; The actual tempo is 150, but if you don't want to use
; a lot of triplets, you can write it as though it's in 6/8 and raise
; the tempo by 50%. The beat becomes the dotted quarter note,
; and 8th notes become triplets.

#Sq1 {
	!start
	!pulse $DUTY_25 $LEN_RUN $CONST_VOL 0
	!envelope 0

	v12 o5 l4.
	a+4 b8 > g4 < a+8+b4 > g8 < a+4 b8 >
	g4 < a+8 b4 > g8 f4e8 e-4d8
	c4 r8 c4d8 e-4e8 > c < 
	b- a d+4e8 c4 r 
	[b8 a+4b8 a4g8r4 ]2 
	a8 g4 a8 b > c < f+ g
	d+4e8 d+4e8 < b > c

	o5 g4f+8 a+4b8 > d4 < b8 > g4 r8
	f4 < g8 > e4 < g8 > d4d+8 e4 < g8 >
	c c4d8 d+4e8 > c <
	b- a g e
	a a b a
	g4f+8 g4ag8 e
	d d g4e-8 d
	c < g > c r
	!restart	
}

#Sq2 {
	!start
	!pulse $DUTY_25 $LEN_RUN $CONST_VOL 0
	!envelope 0

	; The original game has Sq2 doubled with Sq1. I wrote this
	; harmony part, but I think Mother 2 had something similar.

	v9 o5 l4.
	g4r8 > d4 < g8r4 > d8 < g4 r8 >
	d4 < g8 r4 > d8 < a4g8 g-4f8
	e4 r8 e4f8 f+4g8 > e
	d c < b4 > c8 < g4 r >
	
	[ g8 f+4g8 f4d8 r4 ]2
	c8 < b4 > c8 g g d d
	c c d e
	o5 e4 d8 f+4 g8 b4 g8 > d4 r8
	c4 < b8 > c4 < b8 b-4 b8 > c4 < b8
	g > e4f8 f+4g8 e 
	d c < b > c 
	c c d+ d
	e4d+8 e4 f d8 c <
	b b > d4 < g8 f
	e d c r 
}

#Tri {
	!start
	o2 l4.
	gr > dr <
	ggab >
	cr < gr >
	cc < ba
	[ o2 gr > dr ]2
	o3 c r1 r8 r1 r2

	[ o2 gr > dr ]2
	[ o3 cr < gr ]2
	o3 ff f+f+ 
	gg aa
	dd < gg >
	c < g > c r 
}

#Noise {
	; In drum tracks:
	; p1 - bass drum
	; p2 - snare drum
	; p3 - hi-hat
	; p4 - harsh noise
	; p5 & p6 - other hi-hats, ordinarily not used.
	;
	; Because the number signifies which drum instrument you
	; are using, lengths must be specified before the notes
	; whenever you change rhythms.
	;
	; Volume changes (v) do not seem to work here.

	[ l4. p1 l4 p2 l8 p3 ]12 ; Basic swing rhythm.
	l4. p1 r r r r r r r 

	; Writing the above rests wastes space, but it's easier
	; to count the beats this way. As I mentioned at the top
	; of the file, l4. is one beat in this song.

	[ l4. p1 l4 p2 l8 p3 ]8
	[ l4. p1 l8 p2 r p3 p1 r p3 p1 r p3 ]3
	l4. p2 p1 p2 r 
}
```
---

### Ultima 3 - Wanderer

```
#song 7
; Ultima 3 - Wanderer
t120

; Ultima 3 is the first RPG with a full soundtrack, which directly influenced
; the original Dragon Quest. This is much better work than you might expect
; from a game in 1983. There was limited disk space, so the composer wrote a lot
; of musical patterns, which he re-used to make songs.
; We can do the same thing via !jsr (see below).
;
; I transcribed this using the sheet music available here:
; https://www.youtube.com/watch?v=XZZ2J44wPl4&list=RDXZZ2J44wPl4
; I have written some notes with alternate spellings (e.g. F+ instead of Gb)
; for music theory reasons that I won't get into here.

#Sq1 {
	!start
	!pulse $DUTY_50 $LEN_RUN $CONST_VOL 0
	!envelope 0
	v11 ; I recommend never using volume level 15 or 14
	; because square channels are much louder than the other instruments.
	; A lot of great games use lower volume than you might expect!
	
	r1 ; intro (triangle bass is playing)
	[ r2. r8 o5 l8 g ; eighth note pickup
	
	!jsr @melody8ths 
	!jsr @melody8ths 
	!jsr @melodytriplets 
	!jsr @melody8ths 

	; About !jsr:
	; Whenever you have a song structure where a section is repeated, not
	; immediately, but some time later, consider making that into a subroutine
	; and use !jsr. JSR is "jump to subroutine" in assembly language,
	; but it works like the macros of other MML implementations. 
	; The only difference is JSR jumps to a different memory location and
	; saves the return address, similar to loops.
	; I place subroutines at the end of their respective instrument blocks.
	; You cannot use a Sq1 subroutine in other instruments.

	o6 l8 dcdc < b4 > a-4 
	; this isn't a subroutine because it only happens once
	g4 e-2 e-4
	fe-fe- d4 a-4
	g4 e-2 d4
	e-4 c4 < b-4 a-4
	g2 b2 

	!jsr @melody8ths ; As you can see, without subroutines, this song would
	!jsr @melodytriplets ; use a lot more bytes than it currently does!
	!jsr @melodybridge
	!jsr @melody8ths
	!jsr @melody8ths
	!jsr @melodybridge
	!jsr @melodytriplets
	!jsr @melodytriplets

	o6 l8 cde-fga-bg >
	c2 r2 ] ; Ending/repeat.

	; Subroutines are below this point.
	; You MUST name them with @ and call them with !jsr.
	; You MUST end them with !return or weird things will happen.

	@melody8ths:
	o6 l8 cde-cde-fd 
	e-4 g4 fe-df 
	e-dce- l4 d < b 
	> c < gr g 
	!return

	@melodytriplets:
	o6 l6 ; l6 is 1/3 of a half note, twice as long as ordinary triplets.
	cde-de-f
	e-fgfe-d
	e-dcdc < b >
	cdc < g3 g6
	!return

	@melodybridge:
	o5 l8
	a-g f4 g2
	ag f4 g4 a-4
	a4 > e4 f+edc+
	d2 e4 c+4
	d < a f+ a d2 > 
	d < a f a d2 > 
	c < geg c2 > 
	c < ge-g c4 < b4 
	!return
}

#Sq2 {
	!start
	!pulse $DUTY_50 $LEN_RUN $CONST_VOL 0
	!envelope 0
	
	r1
	[ v10 l8
	[ r1 ]13
	
	!jsr @counter8ths

	o5 l8 baba g4 > f4 
	e-4 c2 c4
	dcdc l4 < b- > f
	e- < b-2 g >
	c < a- d f
	e-2 d2

	!jsr @counter8ths
	!jsr @counter8ths
	!jsr @counterbridge
	[ r1 ]4
	!jsr @counter8ths
	!jsr @counterbridge
	!jsr @counter8ths
	!jsr @counter8ths

	o5 l8 e-d ce- dc < b > d
	c2 r2 ]
	
	@counter8ths:
	o5 e-g > c < e- g > c d < b >
	c4 e-4 dc < b > d
	c4 < g4 b4 f4
	e-g e-c < ga-b > f
	!return

	@counterbridge:
	o5 l8 fe- d4 e-4 c4
	fe d4 e-4 e-4
	e2 a2.
	g+4 a2
	af+ df+ < a2 >
	af df < a2 >
	ge ce < g2 >
	ge- ce- < g4 g4
	!return
}

#Tri {
	!start

	[ [ o3 l2 c < g ]6 ; This isn't a subroutine because it's so simple
	
	!jsr @bassquarter
	!jsr @bassquarter

	[ o3 l2 c < g ]4

	o3 l8 d2 < g2 > 
	cde-f g2
	f2 < b-2 >
	e-fga- b-4 b4 >
	c2 < f2 
	ge-dc dcd < g

	!jsr @bassquarter
	!jsr @bassquarter
	!jsr @bassbridge

	[ o3 l2 c < g ]4

	!jsr @bassquarter
	!jsr @bassbridge
	
	[ o3 l2 c < g ]4
	!jsr @bassquarter
	o4 l4 c < c g < g ]

	@bassquarter:
	[ o3 l4 c g < !endloopif 3 g > g ]4
	o2 g2
	!return

	@bassbridge:
	o2 l4 f b- > e- e
	l8 fgab > c4 < b4 >
	c+2 dc+ < ba
	b2 > c+4 g4
	o4 l8 f+d < a > d < f+2
	o4 fd < a > d < f2
	o4 ec < g > c < e2
	o4 e-c < g > c < e-4 f4 
	!return
}

#Noise {
	!end ; #Noise block must be included, even if it doesn't play anything
}
```

---
## Troubleshooting

Some common problems that can occur when compiling mml.

- A note length becomes 0 or higher than 255 ticks. This is not allowed by the engine. The error message will tell you which song and channel caused the error. Maybe you tied too many notes together, or your tempo was too fast or too slow relative to the note lengths you used. The calculated illegal tick count (0 or &gt;255) will also show in the error message.
- If a note length does not sound for as many ticks as expected, it has been reported that you need to use length counter halt. ($len_halt)
- Some notes might not play, if their pitch is too high or too low, depending on the channel. The range of legal note values is bigger than the range of notes that can actually be played.
- If one of your notes has a pitch that is too low or too high to even be turned into bytecode, compilation will fail. The base range is 127 different semitones, starting from C in octave 2.
- Opcodes (commands) must start with !, if you write jsr instead of !jsr you will get an error message. Error messages during parsing will tell you which line and column in your mml the problem exists at.
- Labels must start with @ and end with a colon in the definition, but not with a colon when they are referenced.
- Loops and subroutines do not emit what you expected? This is probably because they are compiled once, but can be called many times. You cannot call a subroutine with different tempos or octaves for example, although the channel transpose-operator can be used for the latter.

```
o4
!jsr @sub
_12
!jsr @sub
!end

@sub:
c d e
!return
```

This will play c d e both times the subroutine is called, but on the second call the channel has been tuned one octave higher - so the pitches will be different although the bytecode inside the subroutine was unchanged.

```
o4
!jsr @sub
o5
!jsr @sub
!end

@sub:
c d e
!return
```

This, however, will not do the same thing as above. Octave is just a concept for the compiler, and whatever octave the compiler last saw when the subroutine is compiled will be used. This is why it is good practice to set an explicit octave at the beginning of your subroutines and your loops.

Similarly for tempo, it cannot be changed between calls to jsr or loop iterations. The music engine simply does not support any way to do it.

To play the exact same music with different tempos the music has to be duplicated into separate subroutines, starting with different tempo values. Once the note lengths have been set once, they cannot change.

- Some midi channels or LilyPond staves stop early? Each channel is converted separately, and stops once the internal VM has reached a previous state. If one channel plays twice in the time another channel plays once, for example, this can happen.
- The midi or lilypond doesn't necessarily behave the way the music does in ROM. They are converted from the mml and not the bytecode, so they support tempo changes between calls to subroutine, unlike the real engine.
- Compilation happens linearly from the top of the channel's mml until the end, and not necessarily in the order the music is played. This is another good reason to write octaves inside subroutines and loops. (and tempos if you use tempo changes)
- The notes plays extremely fast? You probably did not set a note length. The default note length in Faxanadu is 1 tick, and will be used until a length is given.
- If your song jumps into another song, or starts playing strange noise, one of two problems could be happening:
  - Your channel doesn't end cleanly. It needs to be in an infinite loop, or end with !restart or !end. Your subroutines must end with a !return somewhere.
  - Your channel is in an infinite loop but not emitting any notes or rests. This causes the program counter to get corrupted in Faxanadu's music engine. Some examples:
     - A channel only consisting of !restart
     - An infinite loop [ ... ] with no notes or rests inside.
- If MML extracted from ROM contains lots of garbage bytes, it means the composer took advantage of the fact that !restart applies to all channels simultaneously, and did not add an !end or !restart to the other channels. This works, but the MML decompiler has no hope of detecting this, and will extract all data for each channel individually until it hits an !end or infinite loop.

---

## Original Songs

These are the songs in the original game:

1. "Intro"
2. "Land of Dwarf (Dartmoor Castle)"
3. "Trunk"
4. "Branches"
5. "Mist"
6. "Towers"
7. "Eolis"
8. "Mantra/Death"
9. "Towns"
10. "Boss Music"
11. "Hour Glass"
12. "Outro"
13. "King"
14. "Guru"
15. "Shops/House"
16. "Zenis (evil lair)"
