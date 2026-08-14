set(SOURCES
    src/main.cpp

    src/fi/cli/Cli.cpp

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

set(EOE_CONFIG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/eoe_config.xml")

add_custom_command(
    OUTPUT "${EOE_CONFIG_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EOE_CONFIG_SOURCE}"
        "${EOE_CONFIG_OUTPUT}"
    DEPENDS "${EOE_CONFIG_SOURCE}"
    VERBATIM
)

add_custom_target(faxiscripts_config ALL
    DEPENDS "${EOE_CONFIG_OUTPUT}"
)

add_dependencies(faxiscripts faxiscripts_config)
