set(SOURCES
    src/main.cpp
    src/eoe_test/ScriptRoundTripTests.cpp
)

add_executable(tests ${SOURCES})

target_link_libraries(tests
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

add_custom_target(tests_config
    DEPENDS "${EOE_CONFIG_OUTPUT}"
)

add_dependencies(tests tests_config)

add_custom_command(
    TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/test_data"
        "${CMAKE_CURRENT_BINARY_DIR}/test_data"
    VERBATIM
)
