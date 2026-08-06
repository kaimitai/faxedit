add_library(eoe_core STATIC
    src/common/klib/Asm6502.cpp
    src/common/klib/Bitreader.cpp
    src/common/klib/Bitwriter.cpp
    src/common/klib/IPS_Patch.cpp
    src/common/klib/Kfile.cpp
    src/common/klib/Kstring.cpp
    src/common/klib/NES_tile.cpp
)

target_include_directories(eoe_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(eoe_core PUBLIC cxx_std_20)
