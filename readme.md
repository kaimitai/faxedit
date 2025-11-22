# Echoes of Eolis - An editor for Faxanadu (NES)

Welcome to the Echoes of Eolis code repository and release page. The code is standard C++20, and the project files were created using Microsoft Visual Studio Community 2022. You can compile the application from source, or get the latest precompiled Windows x64 build under the [repository releases](https://github.com/kaimitai/faxedit/releases/).

Make sure to read the [documentation](./docs/doc.md) for a detailed overview of all the inter-connected data in this game.

<hr>

## Editor Capabilities

The following data is editable:

### Screen Data

* Screen tilemaps
* Screen scrolling connections
* Screen transition connections
* Screen doors
* Screen sprites

### World Data

* Metatile definitions
* Default palette
* Mattock animations

### Game Data
* Stage definitions
* Spawn points
* Building parameter sprite-sets
* Push-Block metadata
* Jump-On Block metadata

![The editor in action](./docs/img/eoe_presentation.png)
###### The editor will show screen, world and game metadata information
The editor can save your project as a patched NES ROM file or as an IPS patch. We also support our own XML format, which allows users to more easily compare file versions, use version control systems to track file history, and collaborate on projects.

<hr>

### Roadmap

This editor was built during a few intense weeks of development, and is being released as a beta. Some bugs are expected. While we stabilize the codebase, we will prioritize bug-fixing. Once the technical debt has been paid down, we want to prioritize the following additions:

* Add an undo feature
* Show door and transition destinations in the destinations screen, with the option to go back to the entry point
* Add configuration support for more ROM-hacks
* General UI improvements

<hr>

### Version History

* 2025-11-22: version beta-3
  * We show iScript code directly in the editor, with syntax highlighting. To actually edit and assemble the code, however, you need to use a separate tool like [FaxIScripts](https://github.com/kaimitai/FaxIScripts/).
  * We introduce a configuration xml file with region definitions, and constants needed by the editor per region. We now support all major regions, and two ROM hacks, by default.
  * Block Property icon overlays - We can toggle icon overlay rendering for each block property. Currently I am using my own placeholder graphics, but if anyone wants to contribute graphics let me know
  * Door Requirement icon overlays - We can toggle overlays for door requirements too (keys and rings)
  * Sprite animations - Sprites can be rendered using all of their animation frames
  * Made some internal adjustments to sprites which have different positional offsets in the game data versus how they are actually rendered
  * Improved sprite descriptions and categories by verifying the animations in the editor versus actual in-game rendering and behavior
  * Screen tilemap rendering - Some screens in the Buildings world and Mist were rendering slightly incorrectly due to a NES tile mismatch between the editor and the actual game. This has been fixed.
  * We allow patching ROM files in-place by holding shift when using the patch ROM-button
  * Will not show message "clipboard data pasted" when only showing the selection rectangle

* 2025-11-14: version beta-2
  * The editor will extract sprite graphics and metadata when loading a ROM-file, and present them in the UI during sprite-editing
  * Added support for defining the game-wide "Jump-On" tile animation. This is a feature supported by the original game, but it was left unused.

* 2025-11-01: version beta-1
  * Initial release

<hr>

### Credits

Special thanks to the following contributors and fellow digital archaeologists:

["Vagla"](https://www.romhacking.net/community/627/) - For providing the original documentation of various Faxanadu data formats

[Sebastian Porst](https://github.com/sporst) - For discovering and documenting the data format for special screen-transitions and mapping out the door data

[ChipX86/Christian Hammond](http://chipx86.com/) - For helping with some nitty-gritty details surrounding hard-coded game logic and providing us with an invaluable source in his [Faxanadu disassembly](https://chipx86.com/faxanadu/) project

<hr>

### Curiosa

In order to animate sprites in the editor, I had to investigate and map out the animation frame format. I rendered all the animation frames with the tilesets most likely to be associated with each frame, and to my surprise the third and fourth frames of the Maskman enemy (sprite with id 32) showed a walking female NPC never seen before.

The character data for this NPC is stored in the middle of the character data for Maskman, and the animation frames are similarly entangled. The fact that no ID is associated with this sprite is probably the reason it remained undiscovered for so long.

Here is the animation:

![Unused female NPC](./docs/img/unused_female_npc.png)
