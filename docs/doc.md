# Echoes of Eolis - User Documentation

This is the user documentation for Echoes of Eolis (version beta-5.2), a Faxanadu data editor which can be found on its [GitHub repository](https://github.com/kaimitai/faxedit/). It is assumed that users are somewhat acquainted with Faxanadu on the NES.

<hr>

You can start the editor with a filename parameter to make the editor load a given ROM, or use the file picker that appears if no ROM is loaded at startup, to select a file to load. The editor depends on a ROM being loaded and cannot be used without one.

It also depends on configuration file eoe_config.xml being present, as this file contains necessary constants per ROM region. This configuration file contains the needed information to know which ROM region you are using, and how to read and write data from it - which differs between regions.

The editor will automatically deduce the ROM region, unless you specify it as a command-line parameter following a ROM-filename.

We use some conventions in the editor:

* Hold down shift to use some buttons which make big changes to your data; loading xml and deleting screens and such
* Yellow sliders are selection sliders to navigate to objects within a container; these will not make any changes to your data, only select it.
* Faxanadu is divided into what has come to be commonly known as "worlds", of which there are eight. We label them - in order - as "Eolis", "Trunk", "Mist", "Towns", "Buildings", "Branches", "Darmoor Castle" and "Zenis". All worlds are modeled pretty much in the same way, but worlds "Towns" and "Buildings" are associated with some special handling. More on that in the various sections below.

The main window for file operations is [Project Control](#project-control).

The data we can edit forms a data hierarchy, from the top-level game metadata down to the individual screens and their sub-data.

## Table of Contents

- [Project Control](#project-control)
- [Game Metadata](#game-metadata)
  - [Stages](#stages)
  - [Spawn Points](#spawn-points)
  - [Building Sprite Sets](#building-sprite-sets)
  - [Push-Block](#push-block)
  - [Jump-On Animation](#jump-on-animation)
  - [Palette to Music map](#palette-to-music-map)
  - [Fog metadata](#fog-metadata)
- [World Metadata](#world-metadata)
  - [Metatile Definitions](#metatile-definitions)
  - [Scenes](#scenes)
  - [Mattock Animation](#mattock-animation)
  - [Cleanup](#cleanup)
- [Screen Metadata](#screen-metadata)
  - [Screen Tilemap](#screen-tilemap)
  - [Screen Sprites](#screen-sprites)
    - [Building Sprite Set visualization](#building-sprite-set-visualization)
  - [Screen Doors](#screen-doors)
  - [Screen Scrolling](#screen-scrolling)
  - [Screen Transitions](#screen-transitions)
- [Graphics](#graphics)
  - [World gfx](#world-gfx)
  - [World palettes](#world-palettes)
  - [Background gfx](#background-gfx)
  - [Background palettes](#background-palettes)
  - [HUD](#hud)
  - [Tips for bmp import](#tips-for-bmp-import)

<hr>

# Project Control

This is the screen used for file operations and data analysis.

![Project control](./img/win_project_control.png)

* Save xml: Saves the project as an xml-file, the recommended master data format
* Patch nes ROM: Writes the ROM file, appends -out to the filename so your loaded file is not overwritten. Will show output messages regarding the used data sizes (hold shift to patch the ROM in-place)
* Save ips: Generates an ips patch file
* Data Integrity Analysis: Does some checking on whether there is problems in your data
* Show/Hide gfx editor: Opens or closes the Graphics Editor window
* Load xml: Reloads xml from file and re-populates your data. Hold Shift to use.
* Output Messages: The messages from the editor

The tilemaps are stored in four different banks in ROM, but the tilemaps for all screens for any world need to be fully contained within one bank. The editor will tell you which banks it used for which worlds, and report on used and available space.

Some Keyboard shortcuts are always availabe when a ROM is loaded:
* Ctrl + S: Save xml
* Ctrl + Shift + L: Load xml
* Ctrl + P: Patch ROM
* Ctrl + Shift + P: Patch ROM in-place (patch the loaded ROM directly)

Holding Alt when patching a ROM will enable semi-static patching mode - which generates ROMs that are compatible with the [Faxanadu Randomizer](https://github.com/Notlobb/Randumizer/), with the limitation that sprites, screen connections and metadata will remain unpatched.

<hr>

# Game Metadata

## Stages

In addition to the worlds in the game, there is a concept of "stage". The game defines six stages, and each of these is associated with a world. When the game starts both your world and stage are set to zero. Each stage is associated with a next stage and a previous stage, and your stage value is updated whenever you pass through a next-stage or previous-stage door. Stage transitions can be a bit confusing - the stage number doesn’t simply increase or decrease as you move between them. Instead, your current stage acts as an index into the stage metadata to determine the next stage. In the original game the stages are increasing and consistent backward and forward, but this doesn't have to be the case. If you want to keep things simple, you can only edit the worlds for each stage, as well as the door requirements, and leave the rest.

We do not allow changing the world for stage 0, as the game code seems to ignore the stage to world mapping when starting the game.

![Stages](./img/win_metadata_game_stages.png)

* Selected Stage: Slider which selects the stage you are editing
* Stage World: The world associated with this stage
* Next Stage: The stage you will be on if you go to the next stage
* Next Screen: The screen you will be on (in the stage's world) if you go to the next stage
* Next-Stage Door Requirement: The requirement needed to pass through a next-stage door

Notice that the player position for the next and previous stages are not given here. They are given by door parameters.

Similarly for the Previous-Stage Parameters

The bottom section represents some start parameters:

* Start Screen: The screen you start on when the game begins
* x: Start x-Position on that screen
* y: Start y-Position on that screen
* Starting Health: The health you start the game with

## Spawn Points

During gameplay you will hold a spawn point value, which ranges from 0 to 7. When you speak to a spawn-point setting guru your value will be set. When you die, or restore the game from a mantra, the spawn point will also be restored. Whenever you die or restore, you are placed in the Guru room, and the spawn point is associated with some data that determines where in the game you will be when you exit the Guru room.

![Spawn Points](./img/win_metadata_game_spawns.png)

* Spawn location: Slider for selecting which spawn point to edit
* World: The world you will be on when exiting the Guru room
* Screen: The screen of that world you will be on
* x and y: The position of that screen you will be on
* Stage Number: The stage you will be on

Note that stage cannot be deduced from the world, since most Guru doors are in the Towns world, which is not associated with a stage - it is therefore stored separately in the game.

* Deduce: The editor traverses all doors in the game and looks for spawn-setting gurus. Whenever it finds one, it sets the parameters for that spawn point based on the door entries. To determine stage number for doors not on a stage-world, it traverses the screens left and right until it finds and other-world transition - and if that world is associated with exactly one stage that stage number will be used. An output message will say whether deduction was successful or not. If you have one world mapped to several stages, for example, you need to handle some of these settings manually.

## Building Sprite Sets

Whenever you enter a door to building, a sprite set index is loaded from that door's metadata. Screens in the Buildings world do not have any sprites associated with them, but act as templates. The sprite set index is used to look up a sprite set - which is a set of sprites with optional scripts, and populates the screen with them. In this window you can edit the definitions of these sprite sets.

This data is on the exact same format as screen sprite sets, but we have to treat them separately in the editor. Internally these sprite sets are just the sprite data for the Buildings world, but the number of sprite sets here does not match the number of screens in that world. (which it does for all other worlds)

![Building Sprite Sets](./img/win_metadata_game_sprite_sets.png)

* Building Sprite Set: Slider which selects which sprite-set to edit
* Selected Sprite: The selected sprite within this sprite-set
* Sprite ID: What kind of sprite the selected sprite is
* (x, y): Position of the sprite in the room
* Script: A script-index used by an NPC. Optional. Can be added or deleted.
* Add or Remove sprite: Adds or Removes the selected sprite from the sprite set
* Command-byte: A value which determines the following based on its value:
  * 0 - Automatically initiate the push-block animation when entering
  * 1 - Play boss music if a boss sprite is on screen, until the boss is dead
  * 2 - Play boss music if a boss sprite is on screen, until the boss is dead. Also starts the end-game sequence once all sprites are gone.
* Add or Remove building sprite set: Adds or Removes the selected sprite set. Hold shift to use. Sprite set will not be deleted if any building-doors use it, and we don't allow deleting sprite sets with index less than 70 - since some sprite sets are hard coded in the game and not used by any doors. (at least the end-game sequence sprite set index is hard coded)

## Push-Block

When you have opened all three springs in the game, and obtained the Ring of Ruby, you can push some blocks to reveal the final fountain and cause a tilemap change. In the original game a ladder falls down opening the path to Mascon.

If all requirements are fulfilled, and you push against blocks with block property 6 (push-block) the animation is triggered based on these parameters.

If a room has command byte 0 set, the animation will also play automatically when you enter that room if the corresponding quest flag is set.

For the quest flag to be set, however, you need to be on a certain stage (not world!) and screen when you push these blocks.

The push-block logic takes several parameters, and basically defines a line-drawing function; starting at a position (x, y), draws a certain number of blocks with a certain metatile id, with a certain change in position for each iteration.

To see a rendering of the metatiles defined in this tab you need to navigate to the corresponding stage's world.

Here is what the window looks like when you are navigated to the world where block-push quest-flag is happening.

![Push-Block](./img/win_metadata_game_push_block.png)

* Stage: The stage the quest flag can be set on. We also show the corresponding world here
* Screen: The screen of that world the quest flag can be set on

Then come the line-drawing parameters

* (x, y): The position the line starts at
* Delta-x: The change in position for each new drawing. 16 will be a vertical line since the screen width is 16 tiles. A value of 17 means vertically down to the right, 255 means a backward horizontal line since the byte value wraps around and becomes -1.
* Draw Count: The number of metatiles that will be drawn. (iteration count)
* Draw-block: The block that will be drawn on these positions, forming a line.

Be careful when setting these parameters — if your (x, y) drawing position goes off-screen (e.g., y > 12), it may overwrite unrelated RAM values.

* Fountain Cover Pos (x, y): The position for the top of the pushable blocks. When you push the blocks for the first time, the game takes your current position and uses that, but when you enter the screen and the animation happens automatically it needs to get this position from somewhere.
* Pushable block animations source and target: Four metatiles which animates the pushable blocks themselves. The source tiles are what the pushed block will be replaced with, and the target tiles are what the tiles at the pushed-to location will be replaced with.
* Deduce: Will search the game for two pushable blocks on top of each other, and fill out as much of the information here automatically as it can.

## jump-On Animation

Blocks with property 5 will morph into other blocks when jumped or walked on. This is a feature supported by the game engine, but it was never used in the original game.

It is similar to the Mattock Animation in that it defines a 4-block animation cycle with a destination metatile. The mattock animations are set per world however, whereas the jump-on animation is game-wide. In other words, when you define the animation you define it for all worlds simultaneously.

![Jump-On Animation](./img/win_metadata_jump_on_animations.png)

Set the four metatiles here. The metatiles will be rendered based on the available metatiles in the selected World - as long as the metatile index is within bounds for that world. Using jump-on with undefined metatiles will probably result in garbled graphics showing up in the game.

## Palette to Music map

Same-world doors can come with a palette change, which is part of the door's parameters. Some palettes can be defined to also introduce a music change. This is how towers work in the original game; when you enter a door the palette changes and a music change is triggered too.

![pal2mus](./img/win_metadata_game_pal2mus.png)

There are seven slots for palette to music definitions. If a palette change occurs and it has no entry in this map, the current music will keep playing as you enter the same-world door.

## Fog Metadata

The fog effect happens in world 2 (Mist) in the original game, but not inside Mist towers.

The reason is that for the fog to be in effect, a certain combination of world number and palette number must be active. This combination can be set here. If you set world number to 8 (invalid world number) the fog will not be active anywhere.

![Fog](./img/win_metadata_game_fog.png)

Another reason we need to know the fog world, is that the fog generating code is using certain chr-tiles of that world's tileset to generate the fog effect. When importing bmp we have to keep these tiles fixed, or else we will ruin the fog graphics.

<hr>

# World Metadata

The metadata for the currently selected World in the main window can be edited in these tabs.

## Metatile Definitions

Metatiles are defined on a per-world basis, and are used to make tilemaps for all screens in that world. 

![Metatiles](./img/win_metadata_world_metatiles.png)

The tiles on the bottom are chr-tiles used by this world's tilemap, and are used to define the metatile graphics. One chr-tile for each of the four quadrants of the metatile.

Right-clicking on the metatile will place the selected NES-tile on the clicked quadrant. Ctrl+Left Click will select the NES-tile from the clicked quadrant. ("color picker").

The other things you can change for a metatile are the following:

* Selected Metatile: The metatile you are currently editing
* Block Property: A number ranging from 0-15. The property of the metatile in screens. They are to my knowledge the following:
  * 0: Air
  * 1: Solid
  * 2: Ladder
  * 3: Door
  * 4: Foreground
  * 5: Jump-On (experimental)
  * 6: Pushable
  * 10: Same-World Transition Ladder
  * 12: Other-world / Return from building
  * 13: Other-world / Return from building (foreground)

Missing values here have no meaning as far as I can tell, although based on game data it looks like property 11 at one point in the development cycle of the game meant "mattock-breakable" - but this is handled differently in the actual game.

Property 5 (jump-on) is referenced in the game code, but no metatiles in the original game use it.

Property 6 (pushable) is only used for the two push-blocks in the entire original game, but you are free to put them on other screens.

Property 10 (same-world transition ladder) makes a block climbable, but if placed at the edge of the screen, will make the game look for a same-world transition for the screen when passing it - and initiate a transition if one was found. While the blocks are climbable, the transition will trigger if placed at the edges of screens and just walking through them horizontally. In the original game there is only one pair of screens with a same-world transition, and it was then used for a vertical transition.

Property 12 and 13 (other world / return) are placed at the edge of screens where other-world transitions are defined. It makes the game look for an other-world transition for this screen when clipping the blocks. It is also be used in buildings to return you to where you entered the building. All the bottom right blocks inside the buildings have property 12 in the original game.

Sub-palettes per quadrant:

Each metatile is associated with 4 sub-palettes. These are the sub-palettes the metatile will be drawn at depending on the parity of the coordinates:

* x even, y even: Sub-palette 0
* x odd, y even: Sub-palette 1
* x even, y odd: Sub-palette 2
* x odd, y odd: Sub-palette 3

No metatiles in the original game use this functionality. In the original game all metatiles have the same value here for all its quadrants. It looks like it can be buggy if text boxes appear and such - and the palette attribute table cannot be properly restored.

* Display chr-tiles
  * Tileset: Show only the chr-tiles which belong to the current world
  * Include HUD tiles: The same as above, but include the HUD tiles which are always loaded in the ppu during gameplay. Stable metatiles can be made out of these.
  * Show All: Will show all chr-tiles, including those which update dynamically during gameplay for text and such. Will cause visual glitches if used in a metatile definition, but could possibly be used creatively.


* Add / Remove metatile: Adds a new metatile, or deletes the selected metatile. Metatiles which are placed on any screen tilemap cannot be deleted. The same goes for metatiles that are part of mattock animations and block-push parameters.

## Scenes

This tab allows you to change some default settings of the current world; palette, music and tileset.

![Tilemap](./img/win_metadata_palette.png)

For the buildings world, each screen has a definition here, and for buildings you can also define the entry position when you enter a door to building.

Default palette and music can be overriden using same-world doors and palette to music mappings.

Note that on player death or restore, the tileset for the Guru room your start in is hard coded in the game to be 6. The same happens when you beat the game and are transported to the King's room.

## Mattock Animation

Which blocks are breakable is defined on a per-world basis. A mattock animation defines the breakable block, what this block turns into after having been broken, and two blocks in between used as its animation.

![Mattock Animation](./img/win_metadata_mattock_animation.png)

Each slider lets you pick a world metatile for each of the four parts of the animation. The most important is the first one which defines which block is actually breakable, and the last one, which defines what you end up with.

## Cleanup

Some utility functions added to the editor. If there are any more hard coded metatile IDs or screen numbers in the game, using this could cause problems - although I do not know of any.

![Cleanup](./img/win_metadata_cleanup.png)

* Delete Unreferenced Metatiles: Deletes all metatiles from this world which are not part of any screen tilemap - and not part of other metadata.
* Delete Unreferenced Screens: Deletes all screens from this world which are not referenced by any other screens.

In both these cases, after deletion - the remaining references are re-index to stay consistent.

# Screen Metadata

The Screen window consists of three parts. On the left part is the screen tilemap itself, which can be edited. The right side defines the editing mode, which will be described below. The bottom part has sliders for selecting current world and screen, as well as some navigation buttons that will take you to destinations defined by the screen. These buttons will take palette into account, which cannot easily be done when cycling through screens using the slider.

If for example you want to see a Mist Tower or Trunk Tower, it is best to go to the screen containing a door to that tower and using the button "Enter Door" - as the palette info from the door will be taken into account when rendering from then on. If you navigate to another world, the default palette will be used again.

You can add or remove (hold shift to use button) screens for all worlds apart from Buildings. Screens that are referenced from other screens cannot be deleted, in that case you need to remove the references first.

This tab also has some checkboxes for adding icon overlays to:
* Block properties (0-15)
* Mattock-breakable blocks
* Door requirements

In addition there is a checkbox to turn on or off sprite animations, and a checkbox to turn on or off gridlines.

If two screens on a world are using the exact same tilemap, you do not need more than two bytes of data to store it in ROM as the pointer will be deduplicated. The tilemaps are stored separately from all other screen data, so these screens could still have different sprites, doors and so on. This could be a way to cheaply increase the world size, and maybe use different palettes for the two screens.

In the original game there are some screen pointers pointing to the same data, so there are duplicate screens - but none of these duplicates are ever used.

The screen tilemap data is compressed using a simple algorithm. You save bytes by using a metatile which is the same as the one immediately preceding it, one sixteen tiles before it or seventeen tiles before it. (when traversing row-wise from top left to bottom right)

## Screen Tilemap

When the Tilemap tab is active, you are in Tilemap mode - and can define the screen's image.

![Tilemap](./img/win_screen_tilemap.png)

The top section shows which metatile is selected, and which property it has.

The bottom section is where you select the metatile to use when drawing.

All metatiles are drawn with the top-left attribute defined for that metatile. In practice the attribute for each quadrant should probably be the same. If they differ between quadrants, they will still render correctly on the tilemap itself.

The controls for making a screen tilemap image is as follows:
* Left click: Select a tile position on the tilemap
* Right click: "Paint" with the selected metatile on the tilemap
* Shift+Left click: Select a rectangular area of the tilemap
* Ctrl+Left click: Select the clicked tile as your selected metatile ("color picker")
* Ctrl+C: Copy rectangular area to clipboard (one clipboard per world)
* Ctrl+V: Paste clipboard at selected tilemap position of clipboard fits
* Shit+V: Show clipboard rectangle at position if it fits, without pasting anything. To see where your clipboard data would be pasted.
* Ctrl+Z: Undo the last action
* Ctrl+Y: Redo the last undone action

The undo and redo have a history of 250 steps. Destructive structural changes - deleting a screen or a metatile definition - will clear all the undo history for the related world.

The clipboards are per-world since the metatile definitions are world-specific.

## Screen Sprites

When the Sprites tab is active, you are in Sprites mode - and can define the screen's sprites.

![Sprites](./img/win_screen_sprites.png)

This is exactly the same interface as the Building Sprite Sets, but in this case it is one set of sprites for the screen instead.

The top slider selects current sprite, then you define the sprite ID and position, and optionally add a script to it.

The add and remove sprite buttons are always active. You can delete sprites without worrying about references.

The command-byte is screen specific, and the values are the same as for building sprite-sets: 0 is block-push animation, 1 is boss-room and 2 is endgame-room.

The selected sprite will be enclosed by a bounded rectangle according to its size, and you can move it on the tilemap by holding shift and clicking at the desired position.

### Building Sprite Set visualization

When you are in world 4 (Buildings), sprites cannot be placed directly in the screens. The sprites used for the building rooms are determined by a combination of screen number (room) and building sprite set given by a door to building.

We allow editing building sprite sets when you are in the buildings world, but be aware that what you are editing are not sprite sets for any particular screen. This feature is available only to make it easier to visualize what the building sprite sets will look like when you attach it to a building-door.

If you use this feature, make sure to define your sprite set when navigated to the screen you ultimately want your door to point to.

## Screen Doors

When the Doors tab is active, you are in Doors mode - and can define the screen's doors.

Doors come in different types, and the different types have different parameters.

![Doors](./img/win_screen_doors.png)

* Selected door: The screen's door you are currently editing
* Door Type: Each door is one of four types; Buildings, Same-World, Next-Stage and Previous-Stage.
* Door coordinates: The entry coordinates of the door on the screen. This must be on top of a metatile with the door-property, or else the game will not look for your door definitions when you try to enter.
* Destination coordinates: Where you appear on the destination screen after entering the door. Does not seem to matter for doors to buildings however, as these use a hard-coded position not currently imported to the editor.

These parameters are common to all door types. The rest of the parameters depend on door type.

### Next-stage and previous-stage doors

These doors get their destination world and screen via the stage configuration, and is shared across all doors of this type within the world. If the world is defined to be used for exactly one stage, as it is in the original game, the editor will show the destination world, screen and entry requirements here - but you need to actually edit them in the stage metadata if you want to change them.

### Door to building

These doors go to a room in the Buildings world. The parameters they take are:

* Requirement: Entry-requirement to use the door (none, key or ring)
* Destination screen: The building room it goes to
* Sprite set: An index to the sprite set from the Building Sprite-Sets to populate the room with when you enter.

After this a list of the contents of the sprite set is shown, so you know what is in the building:

Sprite descriptions followed by their optional script description

### Same-World Door

These doors take you to a screen on the same world (not necessarily same stage!) and take the following additional parameters:

* Requirement: Entry-requirement to use the door (none, key or ring)
* Destination screen: The screen on the same world it goes to
* Destination palette/music: The palette to be used when entering. In the original game this is typically used to override the world's default palette when entering towers.

You can add or remove doors of all types at any time. No reference check is necessary to keep data integrity.

When in Door-editing mode, Shift+Click moves the door entry position to the clicked position on the tilemap. Ctrl+Click moves the exit position.

If the door destination is unambiguous, an "Enter Door"-button will let you go to the door destination while taking palette setting into account.

Note: The "Towns" world does not support same-world doors due to special handling in the game's logic.

## Screen Scrolling

The screens in a world are connected to each other via regular scrolling. When you exit the screen horizontally you keep your vertical position, and you keep your horizontal position when you exit vertically. This makes it look like the screens are physically connected.

![Scrolling](./img/win_screen_scrolling.png)

In this tab you can define the scroll-connection for a screen in all four directions. Connections are optional data and can be added and deleted at will. The destination screen slider lets you set the destination screen number for any direction.

## Screen Transitions

Transitions are optional screen data. You can define transitions between screens that don't use regular scrolling. There are two types: Same-world transitions and other-world transitions.

![Transitions](./img/win_screen_transitions.png)

### Same-World Transition

A screen can be connected to another screen in the same world. The following parameters are given:

* Destination screen
* Destination coordinates (x, y)
* Destination palette

In the original game this is only used in one set of screens, where the horizontal position of entry and exit didn't line up. They didn't even use the palette override functionality, so one can wonder why they didn't just align the screens instead.

In any case, to trigger a same-world transition, you need to pass through a metatile with block property 10 when you exit the screen to trigger a lookup into the sw-transition table.

### Other-World Transition

This is the same as same-world transtions, but it has one more parameter to define the destination world in addition to the other parameters.

To trigger an other-world transition, you need to clip a metatile with block property 12 or 13. 13 is foreground, and is typically used when you go from the overworld into a town.

In the original game all other-world transitions are defined from overworld-screens into town screens, and from town-screens back to the overworld.

You can define these transitions between any two worlds (apart from the Buildings world probably), but one important thing to note is that it allows you to change world without changing your stage. This can allow players to reach stage-doors in several ways, and since the stage-door destinations will change depending on the stage number you could have a door with different destinations depending on how you got to that door.

When in Transitions-editing mode, Shift+left Click moves the other-world destination position to the clicked position on the tilemap. Ctrl+Left Click moves the same-world destination position.

# Graphics

We have functionality for editing the graphics by importing bmp files. The NES does not support arbitraty bitmaps, and there are some strict rules we need to follow for our graphics to look good.

NES background graphics are made out of chr-tiles, which are 8x8 pixel images, where each pixel has a value 0 to 3.

One background palette can be active at any one time, and a palette has 4 sub-palettes. When combining a chr-tile with a palette, we get a colored chr-tile.

Even though the sub-palettes have 4 colors, they share the first color - meaning at most 13 distinct background colors can be active at any one time.

On top of this, a sub-palette is not active per chr-tile, but for a 2x2 region of chr-tiles, which we call a metatile.

So these are the effective rules for a NES screen:

The screen is made out of 8x8 pixel chr tiles.

Only 4 distinct colors can be active for any one 16x16 pixel region (2x2 chr-tiles) of the screen.

Only 13 distinct colors can be active for the screen as a whole.

The number of chr-tiles the picture processing unit (ppu) of the nes can access at one time is limited to 256.

## World gfx

![World gfx](./img/win_gfx_world_gfx.png)

In this edit mode, you can extract a world's (or building screen's) metatiles as a bmp - and import one back in.

* Extract from ROM: Make a texture out of your current metatile definitions and show them as an image.
* Save bmp: Save the tilemap as a bmp. The output messages will tell you where it was stored.
* Load bmp: Import a bmp as a metatile tilemap, and render it here. The rendering will show you what your import will look like in the game.

When loading a bmp to a world's chr-data, we have to make sure we don't ruin the chr-data of any other world using the same tilset. In the original game Dartmoor and Zenis use the same tileset. If importing for Zenis, the importer will keep chr-tiles used by metatile definitions in Dartmoor fixed - and the other way around.

For buildings screens the opposite is true. Here different screens in the same world can use different tilesets, but the metatile definitions are shared. The importer will not touch the metatile definitions used by screens which do not use the same tileset as the one that is being imported. It is important that users make separate metatile definitions for each tileset for this reason. For the buildings world, if you want to import graphics for new metatiles, it is not enough to just make a new metatile for the world - the metatile actually has to be used in the screen you are importing for. The gfx importer looks at actual metatile usage so that unrelated metatiles do not get clobbered.

Also, do not make any changes to screen tilemaps or metatiles between the time you export a bmp and the time you import it, otherwise the importer might be working under the wrong assumptions.

The importer will not generate any new metatiles, it will only update the graphics of metatiles that already exist.

* Commit to ROM: If you are happy with the result, you can commit your import to ROM in memory and it will show in your screen tilemaps. If you are not happy with the result, you can extract from ROM again to clear your staging data. If you have a screen loaded in the tilemap editor using this tileset, you will immediately see the change there, and the chr-tile picker in world metadata will also be updated.
* If there is any staging data, a "preview result under other palette"-slider will be available. This is there so you can check your result if you want your metatiles to render under different palettes, if you have doors to towers for example. You need to click the "Re-render" button to regenerate your output image. This does not change any data however, it is for rendering purposes only.
* chr-tile deduplication strategy; When importing bmp, we want to generate as few chr-tiles as possible while still rendering the imported bmp. We have different strategies to decuplicate, and they are:
  * Sub-Palette: chr-tiles are considered equal only if they have the same byte output (strict)
  * NES-palette: chr-tiles are considered equal if their palette indexes resolve to the same NES-palette colors. If you only need your metatiles to show in the context of one single palette, this is a good option.
  * RGB: chr-tiles are considered equal if they resolve to the same RGB-colors after applying the NES palette and resolving its rgb-values defined in the config xml. (loose)

## World palettes

![World palette](./img/win_gfx_world_palette.png)

This screen allows you to change the 4 sub-palettes for any of the world-palettes.

The sub-palettes show at the top as a 4x4 grid, and the full NES-palette shows at the bottom. You can select a palette color, and then change which NES color it resolves to. If you have a screen loaded in the tilemap editor using this palette, you will immediately see the change.

We have a checkbox "Allow editing bg-color", which users can toggle to edit the first color of each sub-palette. Faxanadu seems to set this to color $0f (black) regardless of value in ROM, so there is little use in changing this color unless you have a modified ROM which does not enforce this.

The Undo and Redo buttons are per palette. The copy and paste buttons apply to the entire palette as a whole.

Note that palette 16 is used by the Title Screen graphic, so we only allow editing this palette in the BG Gfx section. It can be used as the default palette for a world in the Scenes metadata, however.

## Background gfx

![BG gfx](./img/win_gfx_bg_gfx.png)

This screen functions similarly to the World Gfx screen, but deals with background tilemaps instead; The title, intro and outro screen - as well as a virtual tilemap for the items when rendered as background objects (and not as sprites).

The intro and outro screens share one tileset. When editing one the other's chr-tiles must be fixed, otherwise we would ruin the other image.

## Background palettes

This screen works the same way as the world palettes, but applies to the palettes used by the background screens. For the items tilemap we defined a palette, but in the game the palette used depends on the world's palette - and items can look different in different contexts.

## HUD

![HUD](./img/win_gfx_hud.png)

For completeness' sake we have a HUD-editor with a preview.

The game has a mapping from palette number to HUD attribute index, and each HUD attribute index is associated with 4 sub-palettes which are used for the metatiles making up the HUD gfx.

When editing the HUD attributes for each quadrant, you are editing for all palettes using this HUD attribute index.

This is for advanced modding only, but if you change palette you might want to use this functionality. Note also that only the bottom left and bottom right palette attribute is of consequence, as the top two rows of the HUD are blank.

The original game only uses 4 different HUD attributte indexes, although there are 24 slots available.

## Tips for bmp import

When editing NES gfx you are working under heavy constraints, but there are some things you can do to help the bmp importer.

Make sure your graphics are aligned on an 8x8 pixel grid, and that you are aware of the "1 sub-palette per 2x2 chr-tile region" rule. The closer your colors are to your import palette the better. If your import bmp only uses colors from the import palette, and you follow all the rules of NES graphics, the importer will have an easy time.

Try to use sub-palettes which have distinct colors. If you have two indexes of the same color, the importer will assign all the matching colors to the first palette index.

If you need your chr-tiles to be rendered under several sub-palettes ensure the colors you want to match have the same index. If you want the color yellow under one sub-palette to be red under another sub-palette, the red and yellow color must have the same index. Otherwise more chr-tiles must be generated, even if the graphics are otherwise identical.

The hardest gfx to make is the intro and outro screens. They are rendered under two different palettes, their **shared** chr-tile bank consists of 128 tiles - but you need to render 2 screens which are drawn with a total of 1920 tiles - in other words a lot of re-use is necessary.

You can start by importing a totally blank bmp to one of them and commit, that will free up all tiles for the other - and once you have a result for one of them, you can iteratively generate the other.

When loading a bmp an output message will tell you how many chr-tiles were reclaimed (how many more you can make and still be within the limits), or how many it needed to approximate. If approximation was necessary, your chr-count overflowed and the importer had to re-use tiles and just chose the tiles that fit the best to generate the rest of the image. Ideally you don't want to overflow.

For some images, if you import a bmp you exported from ROM, it will still fail to stay within the limits. This might seem counter-intuitive, but this is because there were palettes with the same color within a sub-palette - and it therefore failed to match it to chr-tiles it potentially could have matched it if the same-color was distributed to different indexes in some way. It becomes computationally prohibitive to try all such combinations however. You can temporarily alter palettes like that to have all colors distinct within each 4-color sub-palette, and then change the colors back after the import.

Experiment and have fun!
