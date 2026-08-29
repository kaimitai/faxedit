cmake_minimum_required(VERSION 3.20)

project(echoes_of_eolis LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(EOE_CONFIG_SOURCE "${CMAKE_SOURCE_DIR}/eoe_config.xml")

add_subdirectory(eoe_core)
add_subdirectory(faxedit)
add_subdirectory(faxiscripts)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
