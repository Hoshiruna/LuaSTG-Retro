# Build the same DXC release used by the Windows and Linux backends.
CPMAddPackage(
    NAME luastg_dxc
    GIT_REPOSITORY https://github.com/microsoft/DirectXShaderCompiler.git
    GIT_TAG v1.9.2607
    GIT_SHALLOW YES
    GIT_SUBMODULES_RECURSE YES
    DOWNLOAD_ONLY YES
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
include(ExternalProject)

set(luastg_dxc_install "${CMAKE_BINARY_DIR}/dxc-install")
set(luastg_dxc_library "${luastg_dxc_install}/lib/libdxcompiler.dylib")
set(luastg_dxc_osx_options)
foreach(osx_option IN ITEMS CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT)
    if (DEFINED ${osx_option} AND NOT "${${osx_option}}" STREQUAL "")
        list(APPEND luastg_dxc_osx_options "-D${osx_option}=${${osx_option}}")
    endif ()
endforeach()

ExternalProject_Add(luastg_dxc_build
    SOURCE_DIR "${luastg_dxc_SOURCE_DIR}"
    BINARY_DIR "${CMAKE_BINARY_DIR}/dxc-build"
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    CMAKE_GENERATOR "${CMAKE_GENERATOR}"
    CMAKE_ARGS
        -C "${luastg_dxc_SOURCE_DIR}/cmake/caches/PredefinedParams.cmake"
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_PREFIX=${luastg_dxc_install}
        -DCMAKE_INSTALL_LIBDIR=lib
        -DCMAKE_INSTALL_NAME_DIR=@rpath
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DPYTHON_EXECUTABLE=${Python3_EXECUTABLE}
        -DPython3_EXECUTABLE=${Python3_EXECUTABLE}
        -DHLSL_INCLUDE_TESTS=OFF
        -DLLVM_INCLUDE_TESTS=OFF
        -DSPIRV_BUILD_TESTS=OFF
        -DENABLE_SPIRV_CODEGEN=ON
        -DENABLE_METAL_CODEGEN=OFF
        ${luastg_dxc_osx_options}
    BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release --target dxcompiler
    INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR> --config Release --component dxcompiler
        COMMAND ${CMAKE_INSTALL_NAME_TOOL} -id @rpath/libdxcompiler.dylib "${luastg_dxc_library}"
    INSTALL_BYPRODUCTS "${luastg_dxc_library}"
)
set_target_properties(luastg_dxc_build PROPERTIES FOLDER external)

add_library(DirectXShaderCompiler::dxcompiler SHARED IMPORTED GLOBAL)
set_target_properties(DirectXShaderCompiler::dxcompiler PROPERTIES
    IMPORTED_LOCATION "${luastg_dxc_library}"
    IMPORTED_SONAME "@rpath/libdxcompiler.dylib"
)
add_dependencies(DirectXShaderCompiler::dxcompiler luastg_dxc_build)
set(LUASTG_DXC_INCLUDE_DIRECTORIES "${luastg_dxc_SOURCE_DIR}/include/dxc")
set(LUASTG_SDLGPU_COMPILER_LIBRARIES "${luastg_dxc_library}")
