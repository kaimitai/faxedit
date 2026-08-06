cmake_minimum_required(VERSION 3.20)

project(echoes_of_eolis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(eoe_core)
add_subdirectory(faxedit)
