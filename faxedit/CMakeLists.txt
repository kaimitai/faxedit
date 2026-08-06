find_package(SDL3 REQUIRED)

set(SOURCES
    src/main.cpp

    src/common/lodepng.cpp

    src/common/imgui/imgui.cpp
    src/common/imgui/imgui_draw.cpp
    src/common/imgui/imgui_impl_sdl3.cpp
    src/common/imgui/imgui_impl_sdlrenderer3.cpp
    src/common/imgui/imgui_tables.cpp
    src/common/imgui/imgui_widgets.cpp

    src/common/imguifiledialog/ImGuiFileDialog.cpp

    # SDL-specific, so it remains in the FaxEdit target.
    src/common/klib/SDL_WindowConfig.cpp

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

    src/windows/ControlWindow.cpp
    src/windows/gfx.cpp
    src/windows/GfxWindow.cpp
    src/windows/imgui_helper.cpp
    src/windows/IScriptWindow.cpp
    src/windows/MainWindow.cpp
    src/windows/MetaDataWindow.cpp
    src/windows/TilemapWindow.cpp
    src/windows/SpriteGfxWindow.cpp
    src/windows/UndoInterface.cpp
    src/windows/ClipBoardManager.cpp
    src/windows/CinematicWindow.cpp
    src/windows/WorldVisualizationWindow.cpp
    src/windows/SettingsWindow.cpp
)

add_executable(faxedit ${SOURCES})

target_include_directories(faxedit
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/imguifiledialog
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/pugixml
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fe
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fe/sprite
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fi
        ${CMAKE_CURRENT_SOURCE_DIR}/src/windows
)

target_link_libraries(faxedit
    PRIVATE
        eoe_core
        SDL3::SDL3
)
