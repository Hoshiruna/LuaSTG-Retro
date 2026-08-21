set(lib_name "Core.InputSystem")

file(GLOB_RECURSE lib_src RELATIVE ${CMAKE_CURRENT_LIST_DIR} core/*.hpp core/*.cpp sdl/*.hpp sdl/*.cpp)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${lib_src})

add_library(${lib_name} STATIC)
luastg_target_common_options(${lib_name})
luastg_target_more_warning(${lib_name})
target_include_directories(${lib_name} PUBLIC .)
target_sources(${lib_name} PRIVATE ${lib_src})
target_link_libraries(${lib_name} PUBLIC SDL3::SDL3 Core.Logging Core.Math)

set_target_properties(${lib_name} PROPERTIES FOLDER engine)

if (LUASTG_BUILD_TESTS)
    set(test_name "Core.InputSystem.Test")

    add_executable(${test_name})
    luastg_target_common_options(${test_name})
    luastg_target_more_warning(${test_name})
    target_compile_features(${test_name} PRIVATE cxx_std_23)
    target_sources(${test_name} PRIVATE test/InputSystem.cpp)
    target_link_libraries(${test_name} PRIVATE options_compile_utf8 ${lib_name} GTest::gtest_main)

    set_target_properties(${test_name} PROPERTIES FOLDER engine/test)
endif ()
