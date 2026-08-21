if (NOT WIN32)
    message(FATAL_ERROR "Core.Graphics currently requires the Windows D3D11 adapter")
endif ()

file(GLOB_RECURSE core_graphics_sources CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_LIST_DIR}
    "Core/Graphics/*.hpp"
    "Core/Graphics/*.cpp"
)
list(APPEND core_graphics_sources
    Core/i18n.hpp
    Core/i18n.cpp
)

add_library(Core.Graphics STATIC)
luastg_target_common_options(Core.Graphics)
luastg_target_more_warning(Core.Graphics)
target_include_directories(Core.Graphics
    PUBLIC
        .
    PRIVATE
        ${PROJECT_SOURCE_DIR}/LuaSTG
)
target_sources(Core.Graphics PRIVATE ${core_graphics_sources})
source_group(TREE ${CMAKE_CURRENT_LIST_DIR} FILES ${core_graphics_sources})
target_precompile_headers(Core.Graphics PRIVATE
    ${PROJECT_SOURCE_DIR}/LuaSTG/Core/framework.hpp
)

target_link_libraries(Core.Graphics PUBLIC
    Core.Configuration
    Core.FileSystem
    Core.Math
    Core.ReferenceCounted
    Core.String
    Core.WindowSystem
    DirectX::TextureMini
    Freetype::Freetype
    GeneratedShaderHeaders
    Microsoft.Windows.ImplementationLibrary
    PlatformAPI
    TracyAPI
    beautiful_win32_api
    d3d11.lib
    dxgi.lib
    dxguid.lib
    libqoi
    simdutf::simdutf
    spdlog
    tinygltf
    utf8
    utility
    win32
)

set_target_properties(Core.Graphics PROPERTIES FOLDER engine)
