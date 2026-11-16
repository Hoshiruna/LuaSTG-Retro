# prepare cmake external build install directories

function(luastg_cmake_external_build_prepare_directories dir)
    list(LENGTH CMAKE_CONFIGURATION_TYPES config_count)
    if (config_count GREATER_EQUAL 1)
        #message(STATUS "luastg_cmake_external_build_prepare_directories -- multi-config mode (${config_count} configs)")
        foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
            #message(STATUS "luastg_cmake_external_build_prepare_directories -- -- ${config}")
            file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/install/${config}/${dir})
        endforeach()
    else ()
        #message(STATUS "luastg_cmake_external_build_prepare_directories -- single-config mode")
        #message(STATUS "luastg_cmake_external_build_prepare_directories -- -- ${CMAKE_BUILD_TYPE}")
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/install/${CMAKE_BUILD_TYPE}/${dir})
    endif ()
endfunction()

luastg_cmake_external_build_prepare_directories(include)

# collect cmake external build generator options

set(_options -G ${CMAKE_GENERATOR})
if (APPLE)
    foreach(osx_option IN ITEMS CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT)
        if (DEFINED ${osx_option} AND NOT "${${osx_option}}" STREQUAL "")
            list(APPEND _options "-D${osx_option}=${${osx_option}}")
        endif ()
    endforeach()
endif ()
if (MSVC)
    list(APPEND _options
        -DCMAKE_POLICY_DEFAULT_CMP0091=NEW
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>
        "-DCMAKE_C_FLAGS=/utf-8 /D_WIN32_WINNT=0x0A00"
        -DCMAKE_CXX_FLAGS=/utf-8
    )
else ()
    list(APPEND _options -DCMAKE_DEBUG_POSTFIX=)
endif ()
list(APPEND _options
    -DBUILD_SHARED_LIBS=OFF
    -DCMAKE_INSTALL_LIBDIR=lib
    -DCMAKE_PREFIX_PATH=${CMAKE_BINARY_DIR}/install/$<CONFIG>
)
if (DEFINED CMAKE_GENERATOR_PLATFORM AND (NOT (CMAKE_GENERATOR_PLATFORM STREQUAL "")))
    set(_options ${_options} -A ${CMAKE_GENERATOR_PLATFORM})
endif ()
if (DEFINED CMAKE_GENERATOR_TOOLSET AND (NOT (CMAKE_GENERATOR_TOOLSET STREQUAL "")))
    set(_options ${_options} -T ${CMAKE_GENERATOR_TOOLSET})
endif ()
if (DEFINED CMAKE_C_COMPILER AND (NOT (CMAKE_C_COMPILER STREQUAL "")))
    set(_options ${_options} -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER})
endif ()
if (DEFINED CMAKE_CXX_COMPILER AND (NOT (CMAKE_CXX_COMPILER STREQUAL "")))
    set(_options ${_options} -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER})
endif ()
if (CMAKE_GENERATOR STREQUAL "Ninja")
    set(_options ${_options} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif ()
set(LUASTG_CMAKE_EXTERNAL_BUILD_GENERATOR_OPTIONS ${_options})
unset(_options)

# libraries

set(LUASTG_CMAKE_SUB_BUILD_ROOT ${CMAKE_CURRENT_LIST_DIR})

include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/zlib_ng.cmake)
include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/minizip_ng.cmake)
include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/libogg.cmake)
include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/libvorbis.cmake)
include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/libflac.cmake)
include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/freetype.cmake)
if (LUASTG_IMAGE_JPEG_ENABLE)
    include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/libjpeg_turbo.cmake)
endif ()
if (LUASTG_IMAGE_PNG_ENABLE)
    include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/png.cmake)
endif ()
if (LUASTG_IMAGE_WEBP_ENABLE)
    include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/webp.cmake)
endif ()
if (WIN32 AND NOT LUASTG_USE_SDLGPU)
    include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/directx_tex.cmake)
endif ()
#include(${LUASTG_CMAKE_SUB_BUILD_ROOT}/harfbuzz.cmake)
