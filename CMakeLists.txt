cmake_minimum_required(VERSION 3.20)

project(echoes_of_eolis LANGUAGES CXX)

option(EOE_BUILD_TESTS "Build the test suite" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(EOE_CONFIG_SOURCE "${CMAKE_SOURCE_DIR}/eoe_config.xml")

add_subdirectory(eoe_core)
add_subdirectory(faxedit)
add_subdirectory(faxiscripts)

if(EOE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
