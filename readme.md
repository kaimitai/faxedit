# Echoes of Eolis - An editor for Faxanadu (NES)

Welcome to the Echoes of Eolis code repository and release page. The code is standard C++20, and the project files were created using Microsoft Visual Studio Community 2022. You can compile the application from source, or get the latest precompiled Windows x64 build under the [repository releases](https://github.com/kaimitai/faxedit/releases/).

Make sure to read the [documentation](./docs/doc.md) for a detailed overview of all the inter-connected data in this game.

<hr>

## Editor Capabilities
The editor is currently only compatible with the US version of Faxanadu. The following data is editable:

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

* Showing icon overlays for doors and block properties
* Add an undo feature
* Show door and transition destinations in the destinations screen, with the option to go back to the entry point
* Dynamically parse the scripting layer and show information in tooltips. Currently, scripts are labeled based on their original in-game behavior. If you modify shops or other elements using external tools, the labels may no longer be accurate. The scripting layer can be wholly edited with [FaxIScripts](https://github.com/kaimitai/FaxIScripts/)
* Narrow down safe output data sizes with certainty
* General UI improvements

### Known bugs and limitations

* Some screens are not perfectly rendered due to NES-tile mismatches
* Some sprites can look unaligned in the editor, but we are always using the sprite coordinates used in the ROM file. Some sprites were slightly misaligned in the original game, but the game code snaps them into place so it's not visible.
* The safe data size limits are based on educated guesses and may not be fully accurate yet. I cannot guarantee that the editor won't report a successful patch while unintentionally overwriting unrelated data. We recommend using XML as your master data container for safety.

<hr>

### Version History

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
