# GoogleTest (BSD-3-Clause)

set(BUILD_GMOCK OFF CACHE BOOL "Build GoogleMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Install GoogleTest" FORCE)
set(gtest_force_shared_crt OFF CACHE BOOL "Use the shared MSVC runtime" FORCE)

CPMAddPackage(
    NAME googletest
    VERSION 1.18.0
    URL https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz
    URL_HASH SHA256=6E3191C1455468B3FC35A417FB565C1C5071AEE1B7E7F85E30CF48A98D37D8B5
    # CPM 0.43.1 can mistake an empty shared-cache directory for a populated
    # package when it runs under CMake 4.3. FetchContent's build-local cache
    # does not have that ambiguity.
    NO_CACHE YES
)

if (TARGET gtest AND NOT TARGET GTest::gtest)
    add_library(GTest::gtest ALIAS gtest)
endif ()

if (TARGET gtest_main AND NOT TARGET GTest::gtest_main)
    add_library(GTest::gtest_main ALIAS gtest_main)
endif ()

if (NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR "GoogleTest 1.18.0 did not provide GTest::gtest_main")
endif ()

if (TARGET gtest AND TARGET gtest_main)
    set_target_properties(gtest gtest_main PROPERTIES FOLDER external/gtest)
endif ()
