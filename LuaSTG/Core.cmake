# Core

add_library(Core STATIC)

luastg_target_common_options(Core)
luastg_target_more_warning(Core)
target_compile_definitions(Core PRIVATE
    LUASTG_CORE_USING_IMGUI
)
target_include_directories(Core PUBLIC
    .
)

set(Core_SRC
    Core/framework.hpp
    Core/framework.cpp

    Core/FrameRateController.hpp
    Core/FrameRateController.cpp

    Core/ApplicationModel.hpp
    Core/ApplicationModel_Win32.hpp
    Core/ApplicationModel_Win32.cpp
)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${Core_SRC})
target_precompile_headers(Core PRIVATE
    Core/framework.hpp
)
target_sources(Core PRIVATE
    ${Core_SRC}
)

target_link_libraries(Core PUBLIC
    Core.Graphics
    # debug
    spdlog
    TracyAPI
    imgui
    # util
    utility
    utf8
    simdutf::simdutf
    PlatformAPI
    beautiful_win32_api
    GeneratedShaderHeaders
    # win32
    winmm.lib
    imm32.lib
    # dx
    dxguid.lib
    dxgi.lib
    d3d11.lib
    Microsoft.Windows.ImplementationLibrary
    # math
    xmath
    # database
    nlohmann_json
    Core.Math
    Core.String
    Core.Configuration
    Core.ReferenceCounted
    Core.FileSystem
    Core.WindowSystem
    Core.InputSystem
    win32
)
