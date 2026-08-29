set(SOURCES
    src/main.cpp

    src/fi/cli/Cli.cpp
    src/fi/cli/ConfigPaths.cpp

    src/mantra/MantraCli.cpp
)

add_executable(faxiscripts ${SOURCES})

set_target_properties(faxiscripts PROPERTIES
    OUTPUT_NAME "eoe-cli"
)

target_include_directories(faxiscripts
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(faxiscripts
    PRIVATE
        eoe_core
)

add_custom_target(faxiscripts_config ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:faxiscripts>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EOE_CONFIG_SOURCE}"
        "$<TARGET_FILE_DIR:faxiscripts>/eoe_config.xml"
    DEPENDS "${EOE_CONFIG_SOURCE}"
    COMMENT "Copying eoe_config.xml beside eoe-cli"
    VERBATIM
)

add_dependencies(faxiscripts faxiscripts_config)

if(BUILD_TESTING)
    add_test(NAME faxiscripts_config_copy
        COMMAND ${CMAKE_COMMAND} -E compare_files
            "${EOE_CONFIG_SOURCE}"
            "$<TARGET_FILE_DIR:faxiscripts>/eoe_config.xml"
    )

    add_executable(faxiscripts_config_paths_test
        tests/ConfigPathsTest.cpp
        src/fi/cli/ConfigPaths.cpp
    )
    target_include_directories(faxiscripts_config_paths_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    add_executable(faxiscripts_mantra_cli_test
        tests/MantraCliTest.cpp
        src/mantra/MantraCli.cpp
    )
    target_include_directories(faxiscripts_mantra_cli_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(faxiscripts_mantra_cli_test
        PRIVATE
            eoe_core
    )

    add_test(NAME faxiscripts_config_paths COMMAND faxiscripts_config_paths_test)
    add_test(NAME faxiscripts_mantra_cli COMMAND faxiscripts_mantra_cli_test)
    add_test(NAME eoe_cli_help COMMAND $<TARGET_FILE:faxiscripts>)
    add_test(NAME eoe_cli_mantra_success
        COMMAND $<TARGET_FILE:faxiscripts> m -r novice
    )
    add_test(NAME eoe_cli_invalid_command
        COMMAND $<TARGET_FILE:faxiscripts> definitely-not-a-command input output
    )
    add_test(NAME eoe_cli_unknown_short
        COMMAND $<TARGET_FILE:faxiscripts> definitely-not-a-command
    )
    add_test(NAME eoe_cli_missing_input_and_output
        COMMAND $<TARGET_FILE:faxiscripts> extract
    )
    add_test(NAME eoe_cli_missing_output
        COMMAND $<TARGET_FILE:faxiscripts> extract input
    )
    set_tests_properties(
        eoe_cli_invalid_command
        eoe_cli_unknown_short
        eoe_cli_missing_input_and_output
        eoe_cli_missing_output
        PROPERTIES WILL_FAIL TRUE
    )
endif()
