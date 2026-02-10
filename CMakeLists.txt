cmake_minimum_required(VERSION 3.22)
project(LuastgRetro)

# CMake 4.x removed compatibility with policy versions older than 3.5.
# Some third-party subprojects (e.g. discord-rpc) still declare very old
# minima, so raise the policy floor for subdirectory processing.
if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM "3.5")
endif ()

include(cmake/luastg_options.cmake)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
include(cmake/TargetCommonOptions.cmake)
include(cmake/options.cmake)
include(cmake/msvc.crt.cmake)
#msvc_hybrid_runtime_library()
msvc_static_runtime_library()

include(cmake/import/all.cmake)
include(cmake/build/all.cmake)
add_subdirectory(external)
include(cmake/packages.lua.cmake)
include(LuaSTG/shaders.cmake)
add_subdirectory(imgui)

add_subdirectory(engine)
target_link_libraries(lua_imgui PUBLIC lua_plus) # TODO: 丑不拉几的
add_subdirectory(tool)
add_subdirectory(LuaSTG)

if (CMAKE_GENERATOR MATCHES "Visual Studio")
    message(STATUS "Current CMake generator is Visual Studio, set startup project to LuaSTG")
    set_directory_properties(PROPERTY VS_STARTUP_PROJECT LuaSTG)
endif ()
