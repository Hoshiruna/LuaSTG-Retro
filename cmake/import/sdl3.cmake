# SDL3 (zlib license)

set(SDL_SHARED OFF CACHE BOOL "Build the SDL3 shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Build the SDL3 static library" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3 test library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Build the SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Build the SDL3 examples" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "Disable SDL3 install targets" FORCE)
set(SDL_DISABLE_INSTALL_DOCS ON CACHE BOOL "Disable SDL3 documentation install targets" FORCE)

CPMAddPackage(
    NAME SDL3
    VERSION 3.4.8
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.8
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_DISABLE_INSTALL ON"
        "SDL_DISABLE_INSTALL_DOCS ON"
)

if(TARGET SDL3-static)
    set_target_properties(SDL3-static PROPERTIES FOLDER external)
endif()
