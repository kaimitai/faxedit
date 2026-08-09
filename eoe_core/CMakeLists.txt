add_library(eoe_core STATIC

    src/common/lodepng.cpp

    src/common/klib/Asm6502.cpp
    src/common/klib/Bitreader.cpp
    src/common/klib/Bitwriter.cpp
    src/common/klib/IPS_Patch.cpp
    src/common/klib/Kfile.cpp
    src/common/klib/Kstring.cpp
    src/common/klib/NES_tile.cpp

    src/common/pugixml/pugixml.cpp

    src/fe/ChrStructures.cpp
    src/fe/Chunk.cpp
    src/fe/Config.cpp
    src/fe/Door.cpp
    src/fe/Game.cpp
    src/fe/GfxTilemapManager.cpp
    src/fe/GodAllocator.cpp
    src/fe/InterChunkScroll.cpp
    src/fe/IntraChunkScroll.cpp
    src/fe/Metatile.cpp
    src/fe/ROM_Manager.cpp
    src/fe/Screen.cpp
    src/fe/Sprite.cpp
    src/fe/Sprite_set.cpp
    src/fe/StageManager.cpp
    src/fe/PaletteMusicMap.cpp
    src/fe/EditorSettings.cpp
    src/fe/ScreenRef.cpp
    src/fe/MetatileRef.cpp
    src/fe/Cinematic.cpp
    src/fe/WorldVisualizer.cpp
    src/fe/xml/Xml_helper.cpp

    src/fe/sprite/SpriteAnimationFrame.cpp
    src/fe/sprite/SpriteFrameCollection.cpp
    src/fe/sprite/SpriteGfxManager.cpp
    src/fe/sprite/SpriteGfxSnapshotManager.cpp
    src/fe/sprite/SpriteGUILoader.cpp

    src/fi/FaxString.cpp
    src/fi/IScriptLoader.cpp
    src/fi/Opcode.cpp
    src/fi/Shop.cpp

    # fh
    src/fh/HackManager.cpp
    src/fh/TilemapChanges.cpp
)

target_include_directories(eoe_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(eoe_core PUBLIC cxx_std_20)
