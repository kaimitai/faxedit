# Echoes of Eolis - An editor for Faxanadu (NES)

Welcome to the Echoes of Eolis code repository and release page. The code is standard C++20, and the project files were created using Microsoft Visual Studio Community 2022. You can compile the application from source, or get the latest precompiled win-x64 distribution under the [repository releases](https://github.com/kaimitai/faxedit/releases/) . \
Make sure to read the [documentation](./docs/doc.md) for a detailed overview of all the inter-connected data in this game.
<br></br>

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
* Mattock-animations

### Game Data
* Stage definitions
* Spawn points
* Building parameter sprite-sets
* Push-Block metadata

<br></br>
![The editor in action](./docs/img/eoe_presentation.png)
###### The editor will show screen, world and game metadata information
<br></br>
The editor can save your project data as a patched ROM file (nes), or as a patch file (ips). We also support our own xml format - which allows users to more easily compare file versions, use version control systems to track file history, and collaborate on projects.
<br></br>

<hr>

### Roadmap

This editor is the result of intensive coding sessions over a few weeks. This is therefore a beta version, and it is expected that there are bugs. While we stabilize the codebase, we will prioritize bug-fixing. Once the technical debt has been paid down, we want to prioritize the following additions:

* Showing sprite graphics in the editor
* Showing icon overlays for doors and block properties
* Add an undo-interface
* Show door and transition destinations in the destinations screen, with the option to go back to the entry point
* Add support for editing scripts and text. Currently the scripts are presented with a label of what the script meant in the original game, so if you use other tools to change shops and such - the descriptions may not be correct.
* Narrow down safe output data sizes with certainty
* General UI-improvements

### Known bugs and limitations

* Some screens are not perfectly rendered due to nes-tile mismatches
* The safe data size limits are based on educated guesses, and might be wrong. The editor could potentially report that it was able to patch your ROM, and still overwrite code or other data sections. Use xml as your master data container.

<hr>

### Version History

2025-11-01: version beta-1
* Initial release

<hr>

### Credits

["Vagla"](https://www.romhacking.net/community/627/) - For providing the original documentation of various Faxanadu data formats

[Sebastian Porst](https://github.com/sporst) - For discovering and documenting the data format for special screen-transitions and mapping out the door data

[ChipX86/Christian Hammond](http://chipx86.com/) - For helping with some nitty-gritty details surrounding hard-coded game logic and providing us with an invaluable source in his [Faxanadu disassembly](https://chipx86.com/faxanadu/) project
