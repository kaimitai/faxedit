add_library(eoe_core STATIC

    src/common/klib/Asm6502.cpp
    src/common/klib/Bitreader.cpp
    src/common/klib/Bitwriter.cpp
    src/common/klib/IPS_Patch.cpp
    src/common/klib/Kfile.cpp
    src/common/klib/Kstring.cpp
    src/common/klib/NES_tile.cpp

    src/common/midifile/Binasc.cpp
    src/common/midifile/MidiEvent.cpp
    src/common/midifile/MidiEventList.cpp
    src/common/midifile/MidiFile.cpp
    src/common/midifile/MidiMessage.cpp
    src/common/midifile/Options.cpp

    src/common/pugixml/pugixml.cpp

    src/common/lodepng.cpp

    #fe
    src/fe/AtlasMovieAssets.cpp
    src/fe/AtlasMovieBundle.cpp
    src/fe/AtlasMovieCompatibility.cpp
    src/fe/AtlasMovieEditor.cpp
    src/fe/AtlasMovieEngine.cpp
    src/fe/AtlasMovieLayout.cpp
    src/fe/AtlasMoviePreview.cpp
    src/fe/AtlasMovieRuntime.cpp
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

    src/fe/script/ScriptManager.cpp

    #fi
    src/fi/AsmReader.cpp
    src/fi/AsmReaderStaticLinker.cpp
    src/fi/AsmWriter.cpp
    src/fi/FaxString.cpp
    src/fi/IScriptLoader.cpp
    src/fi/Opcode.cpp
    src/fi/Shop.cpp

    # fb
    src/fb/BScriptLoader.cpp
    src/fb/BScriptOpcode.cpp
    src/fb/BScriptReader.cpp
    src/fb/BScriptWriter.cpp

    # fm
    src/fm/MMLReader.cpp
    src/fm/MMLWriter.cpp
    src/fm/MScriptLoader.cpp
    src/fm/MusicOpcode.cpp
    src/fm/fm_util.cpp

    src/fm/song/Fraction.cpp
    src/fm/song/LilyPond.cpp
    src/fm/song/MMLChannel.cpp
    src/fm/song/MMLSong.cpp
    src/fm/song/MMLSongCollection.cpp
    src/fm/song/Parser.cpp
    src/fm/song/Tokenizer.cpp

    # fh
    src/fh/HackManager.cpp
    src/fh/TilemapChanges.cpp

    # fv
    src/fv/MiscWriter.cpp

    # mantra
    src/mantra/Mantra.cpp
    src/mantra/mantra_math.cpp
)

target_include_directories(eoe_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(eoe_core PUBLIC cxx_std_20)

if(EOE_BUILD_TESTS)
    add_executable(atlas_movie_tests
        tests/AtlasMovieProjectMetadataTests.cpp
    )
    target_link_libraries(atlas_movie_tests PRIVATE eoe_core)
    add_test(NAME atlas_movie_tests COMMAND atlas_movie_tests)

	find_package(Python3 COMPONENTS Interpreter REQUIRED)
	find_program(CA65_EXECUTABLE ca65 REQUIRED)
	find_program(LD65_EXECUTABLE ld65 REQUIRED)
	add_test(NAME atlas_movie_runtime_reproducible
		COMMAND ${Python3_EXECUTABLE}
			${CMAKE_SOURCE_DIR}/util/generate_atlas_movie_runtime.py)

	if(EOE_TEST_USA_ROM)
		if(NOT EXISTS "${EOE_TEST_USA_ROM}")
			message(FATAL_ERROR "EOE_TEST_USA_ROM does not exist: ${EOE_TEST_USA_ROM}")
		endif()
		add_test(NAME atlas_movie_install_usa_rev0
			COMMAND atlas_movie_tests --usa-rom "${EOE_TEST_USA_ROM}")
		add_executable(atlas_movie_safety_tests
			tests/AtlasMovieSafetyTests.cpp
		)
		target_link_libraries(atlas_movie_safety_tests PRIVATE eoe_core)
		add_test(NAME atlas_movie_safety_usa_rev0
			COMMAND atlas_movie_safety_tests
				"${EOE_CONFIG_SOURCE}"
				"${EOE_TEST_USA_ROM}"
				"${CMAKE_CURRENT_BINARY_DIR}/atlas-movie-safety-override.xml")
	endif()
endif()
