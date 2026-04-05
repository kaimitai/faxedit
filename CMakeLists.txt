cmake_minimum_required(VERSION 3.20)
project(faxedit)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED)

set(SOURCES
    faxedit/src/main.cpp

    faxedit/src/common/imgui/imgui.cpp
    faxedit/src/common/imgui/imgui_draw.cpp
    faxedit/src/common/imgui/imgui_impl_sdl3.cpp
    faxedit/src/common/imgui/imgui_impl_sdlrenderer3.cpp
    faxedit/src/common/imgui/imgui_tables.cpp
    faxedit/src/common/imgui/imgui_widgets.cpp

    faxedit/src/common/imguifiledialog/ImGuiFileDialog.cpp

    faxedit/src/common/klib/Bitreader.cpp
    faxedit/src/common/klib/Bitwriter.cpp
    faxedit/src/common/klib/IPS_Patch.cpp
    faxedit/src/common/klib/Kfile.cpp
    faxedit/src/common/klib/NES_tile.cpp
    faxedit/src/common/klib/SDL_WindowConfig.cpp
	faxedit/src/common/klib/Kstring.cpp

    faxedit/src/common/pugixml/pugixml.cpp

    faxedit/src/fe/ChrStructures.cpp
    faxedit/src/fe/Chunk.cpp
    faxedit/src/fe/Config.cpp
    faxedit/src/fe/Door.cpp
    faxedit/src/fe/Game.cpp
    faxedit/src/fe/GfxTilemapManager.cpp
    faxedit/src/fe/GodAllocator.cpp
    faxedit/src/fe/InterChunkScroll.cpp
    faxedit/src/fe/IntraChunkScroll.cpp
    faxedit/src/fe/Metatile.cpp
    faxedit/src/fe/ROM_Manager.cpp
    faxedit/src/fe/Screen.cpp
    faxedit/src/fe/Sprite.cpp
    faxedit/src/fe/Sprite_set.cpp
    faxedit/src/fe/StageManager.cpp
	faxedit/src/fe/PaletteMusicMap.cpp
	faxedit/src/fe/EditorSettings.cpp
    faxedit/src/fe/xml/Xml_helper.cpp

    faxedit/src/fe/sprite/SpriteAnimationFrame.cpp
    faxedit/src/fe/sprite/SpriteFrameCollection.cpp
    faxedit/src/fe/sprite/SpriteGfxManager.cpp
    faxedit/src/fe/sprite/SpriteGfxSnapshotManager.cpp
    faxedit/src/fe/sprite/SpriteGUILoader.cpp

    faxedit/src/fi/FaxString.cpp
    faxedit/src/fi/IScriptLoader.cpp
    faxedit/src/fi/Opcode.cpp
    faxedit/src/fi/Shop.cpp

    faxedit/src/windows/ControlWindow.cpp
    faxedit/src/windows/gfx.cpp
    faxedit/src/windows/GfxWindow.cpp
    faxedit/src/windows/imgui_helper.cpp
    faxedit/src/windows/IScriptWindow.cpp
    faxedit/src/windows/MainWindow.cpp
    faxedit/src/windows/MetaDataWindow.cpp
    faxedit/src/windows/TilemapWindow.cpp
    faxedit/src/windows/SpriteGfxWindow.cpp
    faxedit/src/windows/UndoInterface.cpp
    faxedit/src/windows/ClipBoardManager.cpp
)

add_executable(faxedit ${SOURCES})

target_include_directories(faxedit PRIVATE
    faxedit/src
    faxedit/src/common
    faxedit/src/common/imgui
    faxedit/src/common/imguifiledialog
    faxedit/src/common/klib
    faxedit/src/common/pugixml
    faxedit/src/fe
    faxedit/src/fe/sprite
    faxedit/src/fi
    faxedit/src/windows
)

target_link_libraries(faxedit PRIVATE SDL3::SDL3)