# Atlas Movie Creator

Atlas Movie Creator is an experimental movie editor inside FaxEdit. It uses
real NES graphics, previews the runtime movement, checks ROM space, and exports
projects or compiled movies.

The current implementation supports **Faxanadu USA Rev 0** and offers two
mutually exclusive runtimes:

- **Standalone** uses the generated `AtlasDevPlayMovie <MovieId>` opcode. It
  carries its own 1,998-byte player, leaves the original intro/ending engine
  untouched, and is the safer default. Standalone currently supports ROM-owned
  graphics only; projects with imported ATI assets must use Shared mode.
- **Shared — HIGHLY EXPERIMENTAL** installs Atlas Movie Engine and its six-byte
  `AtlasDevPlayMovieShared <MovieId>` adapter. It replaces Faxanadu's internal
  cinematic engine, so the native intro, native ending, and scripts all use the
  same FMB bundle. Imported Atlas-owned graphics are supported.

The mode selector changes packaging, not movie authoring data. A ROM cannot
contain both runtimes.

Neither runtime suspends unrelated resident hooks. When FaxEdit detects the
current Atlas Resident Scheduler it warns but still permits installation. Its
active roles continue during movies: palette effects such as day/night remain
visible, while roles that write OAM may cause sprite glitches. Other resident
hooks can behave the same way. Test the combination in an emulator.

## Basic and Advanced workspaces

The creator opens in **Basic** mode. Changing workspace only hides or reveals
controls; it never converts the project or changes runtime bytes.

| Basic tab | Intended task |
| --- | --- |
| Movie | Name the movie, choose intro/ending status, exit behavior, and music. |
| Scene | Select, rename, place, drag, delete, and draw movement for actors. |
| Timeline | Choose phases, set actor movement/visibility, and edit simple durations. |
| Add Assets | Import Faxanadu sprites, rooms, palettes, and choose music. |
| Poses | Browse real movie metasprites and apply them to the selected actor. |
| Preview | Play or scrub the completed movie without editing overlays. |

Project entry is always visible, including with no project loaded. **New
Project** creates a valid USA Rev 0 starter from ROM-owned asset references;
**Open Project**, **Load AME**, and **Load installed** accept existing data.

**Advanced** adds:

- FMB export, Standalone opcode-config export, Shared AME export, and Shared
  in-memory ROM application;
- raw metasprite bank and pointer fields;
- ROM asset descriptors and detailed byte attribution;
- actor multiselect, box selection, copy/paste, grouping, mirroring, alignment,
  and spacing;
- precise waypoint insertion, deletion, dragging, snapping, and validation;
- onion skins and semantic idle/directional/action animation sets;
- raw track integrator, keyframe, animation-slot, phase, fade, and SFX fields;
- preview labels, placement overlays, and technical runtime information.

## Recommended authoring workflow

1. Load a clean USA Rev 0 ROM and choose **New Project**, or open an existing
   AMP. A sidecar named `atlas-movie-project.amp` is still opened automatically
   when present, but it is no longer required to begin.
2. Select an existing movie, duplicate one, or create an editable copy through
   the Example picker.
3. In Movie, choose whether it is normal, the official intro, or the official
   ending. A project always has exactly one enabled official intro and ending.
4. In Shared mode, use Add Assets to import a room or gameplay animation when
   the existing movie graphics are insufficient. Imported data is copied into
   Atlas-owned FMB data and never changes FaxEdit's source editors. Standalone
   projects must keep ROM-owned asset descriptors.
5. In Poses, choose an appearance. Return to Scene to place a new actor or
   select an existing actor from the roster.
6. Drag actors directly on the stage. Use Draw movement path for the normal
   visual workflow. Advanced waypoint mode is available for exact correction.
7. Use Timeline to choose which actors move and which are visible in each
   phase. Select a timed phase and edit its duration below the timeline;
   event-based phase endings remain available in Advanced.
8. Use Preview to play or scrub the movie. **Save Project** writes back to the
   opened AMP path, while **Save As** chooses a new path. Project replacement,
   and application exit ask before discarding dirty work. ROM replacement is
   blocked until the dirty movie project is saved.
9. Choose **Standalone** to export
   `eoe_config_override-atlas-movie-standalone.xml`, use it as (or merge it
   into) the project's `eoe_config_override.xml`, call `AtlasDevPlayMovie` in
   iScript, and assemble normally. Choose **Shared — HIGHLY EXPERIMENTAL** to
   export/install an AME before assembling other generated opcodes. FaxEdit
   builds the canonical AME from its compiled-in engine and the current bundle;
   an external `atlas-movie-engine.ame` is optional and used only for inspection
   or importing its movie bundle.

## Actor and path model

Each movie contains one to eight runtime tracks:

- **Path** uses 8-bit fixed-point X/Y integration, up to fifteen movement
  keyframes, and one animation row per movement stage.
- **Cyclic** continuously moves and cycles through visible animation frames.
- **Counter toggle** selects between two poses from a RAM counter and is also
  the compact representation used for a static actor.

The preview models the movie frame counter at `$001A` directly. An arbitrary
game-RAM counter cannot be known by an offline editor, so the UI marks that
toggle unresolved and displays Frame A unless a preview host supplies a counter
reader. It never pretends that the synthetic movie frame is another RAM byte.

Freehand drawing is simplified to at most sixteen editor waypoints. The runtime
path must remain monotonic on one dominant axis, with one optional final
turnaround. Invalid waypoint segments are shown in red and compilation is
blocked. The cyan route is simulated with the same fixed-point behavior used by
the runtime.

AMP-v2 stores actor names, colors, groups, waypoints, and semantic animation
sets as editor metadata. Those fields do not enter FMB unless an operation such
as Apply waypoints or Apply facing sets compiles them into runtime track data.

## Timeline and animation

A phase owns separate update and draw masks. This permits actors to move while
hidden, remain visible while stationary, do both, or do neither. Phase endings
can depend on elapsed frames, music completion, a frame counter, an actor's Y
position, or effect calls. Palette fades and path-dependent SFX predicates are
available in Advanced.

Semantic animation sets are named idle, left, right, toward, away, attack, and
hurt. Automatic facing chooses the movement set for each compiled path leg.
Onion skinning shows the selected actor at earlier and later preview frames.

## Formats and ownership

| Format | Purpose | ROM content |
| --- | --- | --- |
| AMP1 v2 | Editable project, including disabled templates and editor metadata | Never injected |
| FMB1 v2 | Compiled enabled-movie bundle | Stored after the selected runtime in bank 12 |
| FMV1 v2 | One compiled movie record inside FMB | Stored in bank 12 |
| ATI1 v1 | Optional imported CHR, metasprites, nametable, and palette payloads | Stored after movie records |
| AME1 v1 | ROM-free Shared-mode installer package containing engine code and one FMB | Applied by FaxEdit |

Runtime IDs are deterministic: the official intro is ID 0, the official ending
is ID 1, and enabled normal movies follow in project order. Disabled normal
movies remain editable in AMP but consume no ROM bytes.

In Standalone mode, the generated opcode installs a relocatable 1,998-byte
player followed immediately by the FMB. Its final location and remaining space
depend on every generated opcode selected in the project's explicit opcode
map. The exported override contains both the dense current map and
`hack_movie_data`; the normal FaxIScripts/FaxEdit assembler owns allocation and
dispatch generation.

In Shared mode, the dynamically sized FMB starts at bank-12 CPU address
`$B264`. Its FMB plus the 50-byte iScript dispatch reservation must end before
`$C000`, providing 3,484 bytes total. The bundled three-movie project uses 524
FMB bytes, therefore 574 bytes with dispatch and 2,910 bytes remain.

The Shared engine installer owns these principal locations:

- bank 12 `$A708`: 782-byte movie-player core;
- bank 12 `$AD8B`: six-byte iScript opcode adapter;
- bank 12 `$AD91`: 1,235-byte replacement tail;
- bank 12 `$B264`: variable FMB plus relocated 25-entry low/high dispatch
  tables;
- bank 15 `$FC9C`: native title/intro hook;
- bank 12 `$82AE`: native ending hook.

Shared installation disables FaxEdit's original Cinematics writer because both
features own overlapping cinematic machinery. FaxEdit and FaxIScripts detect
the installed AME, expose its preinstalled opcode `$18`, and reserve its FMB
and dispatch range automatically through their shared explicit-opcode path.
Install Shared mode before generated opcodes; once those opcode tables have
moved, replace the bundle by exporting and applying a new AME earlier in the
toolchain.

## Exit behavior and state

- **New game** follows the intro continuation.
- **Title reset** follows the ending continuation.
- **Reload current room** returns to gameplay and preserves persistent player
  state such as items, rank, equipment, quests, and statistics. It reconstructs
  the room through Faxanadu's normal reload behavior; it is not an exact
  instruction-level resume of the interrupted screen.

## Validation

The codec rejects malformed enums, missing official roles, duplicate roles,
invalid masks, missing frames, overlapping PPU assets, malformed imports,
invalid SFX track references, unsupported path structures, and bank-12
overflow. The ROM Budget tab attributes bytes to movies, tracks, imports,
asset descriptors, phases, and SFX and identifies duplicate imported payloads.

## Source map

- `eoe_core/src/fe/AtlasMovieEngine.*`: ROM compatibility checks and AME
  installation.
- `eoe_core/src/fe/AtlasMovieRuntime.s`: 6502 source shared by both
  runtime modes.
- `util/generate_atlas_movie_runtime.py`: deterministic cc65 rebuild,
  relocation derivation, and generated-code identity check.
- `eoe_core/src/fe/AtlasMovieRuntime.*`: generated Standalone player
  relocation, Standalone override generation, and Shared opcode-map
  integration.
- `eoe_core/src/fe/AtlasMovieBundle.*`: AMP/FMB/FMV/ATI codecs, validation,
  budgeting, extraction, and bundle replacement.
- `eoe_core/src/fe/AtlasMovieAssets.*`, `AtlasMovieEditor.*`, and
  `AtlasMoviePreview.*`: ROM-free asset conversion, path compilation, and
  engine-equivalent preview simulation.
- `faxedit/src/windows/AtlasMovieWindow.cpp`: project/runtime orchestration and
  the compact Movie, Timeline, budget, asset-browser, and pose tabs.
- `faxedit/src/windows/AtlasMovieActorsTab.cpp` and
  `AtlasMovieActorInspector.cpp`: focused stage composition and actor runtime
  controls; `AtlasMovieEditorSession.h` owns transient selection, drag, and
  clipboard state; `AtlasMoviePreviewTab.cpp` owns preview interactions.
- `faxedit/src/windows/AtlasMovieProjectIo.cpp`: project/package load, save,
  export, and installation workflow.
- `faxedit/src/windows/AtlasMovieAssetImport.cpp`, `AtlasMovieRenderer.cpp`, and
  `AtlasMovieUi.cpp`: isolated FaxEdit adapters for imports, SDL rendering, and
  shared ImGui controls.
- `eoe_core/tests/AtlasMovieProjectMetadataTests.cpp`: deterministic project
  migration and runtime-isolation checks.
