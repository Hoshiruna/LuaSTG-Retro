# SDL_shadercross (zlib), SPIRV-Cross (Apache-2.0), and the official DXC binaries.
# Keep these dependencies opt-in; the D3D11 runtime does not use them.
CPMAddPackage(
    NAME luastg_dxc
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip
    URL_HASH SHA256=a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241
    DOWNLOAD_ONLY YES
)

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

add_library(DirectXShaderCompiler::dxcompiler SHARED IMPORTED GLOBAL)
set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES
    IMPORTED_LOCATION "${luastg_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
    IMPORTED_IMPLIB "${luastg_dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib"
)

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
target_include_directories(SDL3_shadercross-static PUBLIC "${luastg_shadercross_SOURCE_DIR}/include")
target_link_libraries(SDL3_shadercross-static
    PUBLIC SDL3::SDL3
    PRIVATE spirv-cross-c DirectXShaderCompiler::dxcompiler
)
set_target_properties(SDL3_shadercross-static PROPERTIES FOLDER external)

set(LUASTG_SDLGPU_COMPILER_LIBRARIES
    "${luastg_dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll"
    "${luastg_dxc_SOURCE_DIR}/bin/x64/dxil.dll"
)

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
configure_file("${luastg_dxc_SOURCE_DIR}/LICENSE-LLVM.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/LICENSE-LLVM.txt" COPYONLY)
configure_file("${luastg_dxc_SOURCE_DIR}/LICENSE-MS.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/DXC/LICENSE-MS.txt" COPYONLY)
configure_file("${SDL3_SOURCE_DIR}/LICENSE.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/SDL/LICENSE.txt" COPYONLY)
