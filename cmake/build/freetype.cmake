if (WIN32)
    set(freetype_library_name "freetype$<$<CONFIG:Debug>:d>.lib")
    set(freetype_release_library_name freetype.lib)
    set(freetype_debug_library_name freetyped.lib)
else ()
    set(freetype_library_name libfreetype.a)
    set(freetype_release_library_name libfreetype.a)
    set(freetype_debug_library_name libfreetype.a)
endif ()
# freetype

CPMAddPackage(
    NAME libfreetype
    VERSION 2.14.1
    GITHUB_REPOSITORY freetype/freetype
    GIT_TAG VER-2-14-1
    DOWNLOAD_ONLY YES
)

if (NOT libfreetype_ADDED)
    message(FATAL_ERROR "freetype is requied")
endif ()

# prepare

set(freetype_source_directory  ${libfreetype_SOURCE_DIR})
set(freetype_build_directory   ${CMAKE_BINARY_DIR}/build/freetype/$<CONFIG>)
set(freetype_install_directory ${CMAKE_BINARY_DIR}/install/$<CONFIG>)
set(freetype_library_file      ${freetype_install_directory}/lib/${freetype_library_name})

luastg_cmake_external_build_prepare_directories(include/freetype2)

# external cmake build

add_custom_command(
    OUTPUT ${freetype_library_file}
    COMMAND ${CMAKE_COMMAND} -E echo ${CMAKE_GENERATOR} ${CMAKE_GENERATOR_PLATFORM} ${CMAKE_GENERATOR_TOOLSET} $<CONFIG>
    COMMAND ${CMAKE_COMMAND}
        # basic
        -S ${freetype_source_directory}
        -B ${freetype_build_directory}
        # generator
        ${LUASTG_CMAKE_EXTERNAL_BUILD_GENERATOR_OPTIONS}
        # msvc runtime library
        # install location
        -DCMAKE_INSTALL_PREFIX=${freetype_install_directory}
        # force utf-8
        # options
        -DFT_DISABLE_ZLIB=ON
        -DFT_DISABLE_BZIP2=ON
        -DFT_DISABLE_PNG=ON
        -DFT_DISABLE_HARFBUZZ=ON
        -DFT_DISABLE_BROTLI=ON
    COMMAND ${CMAKE_COMMAND}
        --build ${freetype_build_directory}
        --config $<CONFIG>
    COMMAND ${CMAKE_COMMAND}
        --install ${freetype_build_directory}
        --config $<CONFIG>
        --prefix ${freetype_install_directory}
    VERBATIM
)
add_custom_target(freetype_build ALL
    DEPENDS ${freetype_library_file}
)
set_target_properties(freetype_build PROPERTIES FOLDER external/freetype)

# external cmake build clean

add_custom_target(freetype_clean
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${freetype_build_directory}
    COMMAND ${CMAKE_COMMAND} -E rm -f  ${freetype_library_file}
    VERBATIM
)
set_target_properties(freetype_clean PROPERTIES FOLDER external/freetype)

# import

add_library(freetype STATIC IMPORTED GLOBAL)
add_library(Freetype::Freetype ALIAS freetype)
target_include_directories(freetype
INTERFACE
    ${freetype_install_directory}/include
    ${freetype_install_directory}/include/freetype2
)
set_target_properties(freetype PROPERTIES
    IMPORTED_LOCATION       ${CMAKE_BINARY_DIR}/install/Release/lib/${freetype_release_library_name}
    IMPORTED_LOCATION_DEBUG ${CMAKE_BINARY_DIR}/install/Debug/lib/${freetype_debug_library_name}
)
add_dependencies(freetype freetype_build)
