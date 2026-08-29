find_package(SDL3 REQUIRED)

set(SOURCES
    src/main.cpp

    src/common/imgui/imgui.cpp
    src/common/imgui/imgui_draw.cpp
    src/common/imgui/imgui_impl_sdl3.cpp
    src/common/imgui/imgui_impl_sdlrenderer3.cpp
    src/common/imgui/imgui_tables.cpp
    src/common/imgui/imgui_widgets.cpp

    src/common/imguifiledialog/ImGuiFileDialog.cpp

    # SDL-specific, so it remains in the FaxEdit target.
    src/common/klib/SDL_WindowConfig.cpp

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
    src/windows/ScriptingWindow.cpp
)

add_executable(faxedit ${SOURCES})

set_target_properties(faxedit PROPERTIES
    OUTPUT_NAME "eoe"
)

target_include_directories(faxedit
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(faxedit
    PRIVATE
        eoe_core
        SDL3::SDL3
)

add_custom_target(faxedit_config ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:faxedit>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EOE_CONFIG_SOURCE}"
        "$<TARGET_FILE_DIR:faxedit>/eoe_config.xml"
    DEPENDS "${EOE_CONFIG_SOURCE}"
    COMMENT "Copying eoe_config.xml beside eoe"
    VERBATIM
)

add_dependencies(faxedit faxedit_config)

if(BUILD_TESTING)
    add_test(NAME faxedit_config_copy
        COMMAND ${CMAKE_COMMAND} -E compare_files
            "${EOE_CONFIG_SOURCE}"
            "$<TARGET_FILE_DIR:faxedit>/eoe_config.xml"
    )
endif()
