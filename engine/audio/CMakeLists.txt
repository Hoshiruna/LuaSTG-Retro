# Audio

file(GLOB_RECURSE lib_src RELATIVE ${CMAKE_CURRENT_LIST_DIR} core/*.hpp core/*.cpp backend/*.hpp backend/*.cpp)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${lib_src})

if (NOT LUASTG_AUDIO_PMDMINI_ENABLE)
    list(FILTER lib_src EXCLUDE REGEX ".*/AudioDecoderPMD\\.cpp$")
    list(FILTER lib_src EXCLUDE REGEX ".*/AudioDecoderPMD\\.hpp$")
endif ()

set(lib_name "Core.Audio")

add_library(${lib_name})
luastg_target_common_options(${lib_name})
luastg_target_more_warning(${lib_name})
target_include_directories(${lib_name} PUBLIC .)
target_sources(${lib_name} PRIVATE ${lib_src})
target_link_libraries(${lib_name} PRIVATE options_compile_utf8)
set(audio_public_libs
    Core.ReferenceCounted
    Core.Configuration
    Core.FileSystem
    Core.Logging
    dr_libs
    Ogg::ogg
    Vorbis::vorbis
    Vorbis::vorbisfile
    FLAC::FLAC
    Microsoft.XAudio2.Redist
    win32
    xmath
)

if (LUASTG_AUDIO_PMDMINI_ENABLE)
    list(APPEND audio_public_libs pmdmini)
    target_compile_definitions(${lib_name} PRIVATE LUASTG_ENABLE_PMDMINI=1)
else ()
    target_compile_definitions(${lib_name} PRIVATE LUASTG_ENABLE_PMDMINI=0)
endif ()

target_link_libraries(${lib_name} PUBLIC ${audio_public_libs})

set_target_properties(${lib_name} PROPERTIES FOLDER engine)

# Test

set(test_name "Core.Audio.Test")

add_executable(${test_name})
luastg_target_common_options(${test_name})
luastg_target_more_warning(${test_name})
target_compile_features(${test_name} PRIVATE cxx_std_23)
target_sources(${test_name} PRIVATE test/main.cpp)
target_link_libraries(${test_name} PRIVATE
    options_compile_utf8
    ${lib_name}
    #GTest::gtest_main
)

add_custom_command(TARGET ${test_name} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Microsoft.XAudio2.Redist> $<TARGET_FILE_DIR:${test_name}>
    VERBATIM
)

set_target_properties(${test_name} PROPERTIES FOLDER engine/test)
