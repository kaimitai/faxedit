cmake_minimum_required(VERSION 3.20)

project(echoes_of_eolis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(EOE_CONFIG_SOURCE "${CMAKE_SOURCE_DIR}/eoe_config.xml")

option(EOE_BUILD_TESTS "Build Echoes of Eolis regression tests" OFF)
set(EOE_TEST_USA_ROM "" CACHE FILEPATH
	"Optional clean USA Rev 0 ROM used by the Atlas Movie install test")
if(EOE_BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(eoe_core)
add_subdirectory(faxedit)
add_subdirectory(faxiscripts)
