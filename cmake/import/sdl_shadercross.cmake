# SDL_shadercross (zlib), SPIRV-Cross (Apache-2.0), and the official DXC binaries.
# Keep these dependencies opt-in; the D3D11 runtime does not use them.
if (APPLE)
    include(${CMAKE_CURRENT_LIST_DIR}/dxc-macos.cmake)
else ()
if (WIN32)
    set(luastg_dxc_archive dxc_2026_07_29.zip)
    set(luastg_dxc_archive_hash a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241)
else ()
    set(luastg_dxc_archive linux_dxc_2026_07_29.x86_x64.tar.gz)
    set(luastg_dxc_archive_hash 55665c87824051ed4774ff3280a79ccbbb7d39243b9736ca5e98222134112d54)
endif ()
CPMAddPackage(
    NAME luastg_dxc
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/${luastg_dxc_archive}
    URL_HASH SHA256=${luastg_dxc_archive_hash}
    DOWNLOAD_ONLY YES
)
endif ()

CPMAddPackage(
    NAME luastg_spirv_cross
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Cross
    GIT_TAG 1a6169566c73d3da552748fc372fe2bbb856e46e
    OPTIONS
        "SPIRV_CROSS_STATIC ON"
        "SPIRV_CROSS_SHARED OFF"
        "SPIRV_CROSS_CLI OFF"
        "SPIRV_CROSS_ENABLE_TESTS OFF"
        "SPIRV_CROSS_SKIP_INSTALL ON"
)

if (NOT APPLE)
add_library(DirectXShaderCompiler::dxcompiler SHARED IMPORTED GLOBAL)
if (WIN32)
set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES
    IMPORTED_LOCATION "${luastg_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
    IMPORTED_IMPLIB "${luastg_dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib"
)
    set(LUASTG_DXC_INCLUDE_DIRECTORIES "${luastg_dxc_SOURCE_DIR}/inc")
    set(LUASTG_SDLGPU_COMPILER_LIBRARIES
        "${luastg_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
        "${luastg_dxc_SOURCE_DIR}/bin/x64/dxil.dll"
    )
else ()
    find_path(luastg_dxc_include dxcapi.h
        PATHS "${luastg_dxc_SOURCE_DIR}/include/dxc" "${luastg_dxc_SOURCE_DIR}/include"
        NO_DEFAULT_PATH REQUIRED)
    find_library(luastg_dxc_library dxcompiler
        PATHS "${luastg_dxc_SOURCE_DIR}/lib" "${luastg_dxc_SOURCE_DIR}/lib64"
        NO_DEFAULT_PATH REQUIRED)
    # The July Linux package omits WinAdapter.h. Use the matching source header.
    set(luastg_dxc_headers "${CMAKE_BINARY_DIR}/dxc-headers")
    file(MAKE_DIRECTORY "${luastg_dxc_headers}")
    if (NOT EXISTS "${luastg_dxc_headers}/WinAdapter.h")
        file(DOWNLOAD
            "https://raw.githubusercontent.com/microsoft/DirectXShaderCompiler/v1.9.2607/include/dxc/WinAdapter.h"
            "${luastg_dxc_headers}/WinAdapter.h"
            STATUS luastg_dxc_header_status TLS_VERIFY ON)
        list(GET luastg_dxc_header_status 0 luastg_dxc_header_code)
        if (NOT luastg_dxc_header_code EQUAL 0)
            file(REMOVE "${luastg_dxc_headers}/WinAdapter.h")
            message(FATAL_ERROR "Cannot download the matching DXC WinAdapter.h: ${luastg_dxc_header_status}")
        endif ()
    endif ()
    set(LUASTG_DXC_INCLUDE_DIRECTORIES "${luastg_dxc_include}" "${luastg_dxc_headers}")
    set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES IMPORTED_LOCATION "${luastg_dxc_library}")
    set(LUASTG_SDLGPU_COMPILER_LIBRARIES "${luastg_dxc_library}")
endif ()
endif ()

CPMAddPackage(
    NAME luastg_shadercross
    # The archive excludes upstream's unused vendored LLVM submodules.
    URL https://github.com/libsdl-org/SDL_shadercross/archive/1ff05bec573988a98ef9e0260b4da44f512b8367.tar.gz
    DOWNLOAD_ONLY YES
)

# Build the unmodified library source directly. Upstream's CMake project emits
# package exports even with installation disabled, requiring exports for the
# parent's separately built static dependencies as well.
add_library(SDL3_shadercross-static STATIC
    "${luastg_shadercross_SOURCE_DIR}/src/SDL_shadercross.c"
    "${luastg_shadercross_SOURCE_DIR}/include/SDL3_shadercross/SDL_shadercross.h"
)
add_library(SDL3_shadercross::SDL3_shadercross-static ALIAS SDL3_shadercross-static)
luastg_target_common_options(SDL3_shadercross-static)
target_compile_definitions(SDL3_shadercross-static PRIVATE SDL_SHADERCROSS_DXC)
target_include_directories(SDL3_shadercross-static PUBLIC "${luastg_shadercross_SOURCE_DIR}/include" PRIVATE ${LUASTG_DXC_INCLUDE_DIRECTORIES})
target_link_libraries(SDL3_shadercross-static
    PUBLIC SDL3::SDL3
    PRIVATE spirv-cross-c DirectXShaderCompiler::dxcompiler
)
set_target_properties(SDL3_shadercross-static PROPERTIES FOLDER external)

# Use a staging directory to preserve each dependency's original notice filename.
set(LUASTG_SDLGPU_LICENSE_DIRECTORY "${CMAKE_BINARY_DIR}/sdlgpu-licenses")
file(MAKE_DIRECTORY
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SDL_shadercross"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SPIRV-Cross"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SDL"
)
configure_file("${luastg_shadercross_SOURCE_DIR}/LICENSE.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SDL_shadercross/LICENSE.txt" COPYONLY)
configure_file("${luastg_spirv_cross_SOURCE_DIR}/LICENSE" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SPIRV-Cross/LICENSE" COPYONLY)
if (APPLE)
    configure_file("${luastg_dxc_SOURCE_DIR}/LICENSE.TXT" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/LICENSE.TXT" COPYONLY)
    foreach(spirv_dependency IN ITEMS SPIRV-Headers SPIRV-Tools)
        file(MAKE_DIRECTORY "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/${spirv_dependency}")
        configure_file("${luastg_dxc_SOURCE_DIR}/external/${spirv_dependency}/LICENSE"
            "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/${spirv_dependency}/LICENSE" COPYONLY)
    endforeach()
else ()
    configure_file("${luastg_dxc_SOURCE_DIR}/LICENSE-LLVM.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/LICENSE-LLVM.txt" COPYONLY)
    configure_file("${luastg_dxc_SOURCE_DIR}/LICENSE-MS.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/LICENSE-MS.txt" COPYONLY)
endif ()
configure_file("${SDL3_SOURCE_DIR}/LICENSE.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SDL/LICENSE.txt" COPYONLY)
