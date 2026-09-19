if (NOT WIN32)
    message(FATAL_ERROR "Core.Graphics currently requires the Windows D3D11 adapter")
endif ()

set(core_graphics_common_sources
    Core/Graphics/Common/FreeTypeGlyphManager.cpp
    Core/Graphics/Common/FreeTypeGlyphManager.hpp
    Core/Graphics/Common/Sprite.cpp
    Core/Graphics/Common/Sprite.hpp
    Core/Graphics/Common/SpriteRenderer.cpp
    Core/Graphics/Common/SpriteRenderer.hpp
    Core/Graphics/Common/TextRenderer.cpp
    Core/Graphics/Common/TextRenderer.hpp
    Core/Graphics/Device.hpp
    Core/Graphics/Font.hpp
    Core/Graphics/Format.hpp
    Core/Graphics/Image.hpp
    Core/Graphics/Mesh.hpp
    Core/Graphics/Renderer.hpp
    Core/Graphics/Runtime.hpp
    Core/Graphics/Sprite.hpp
    Core/Graphics/SwapChain.hpp
)

set(core_graphics_d3d11_sources
    Core/Graphics/Common/MemoryImage.cpp
    Core/Graphics/Common/MemoryImage.hpp
    Core/Graphics/Direct3D11/Buffer.cpp
    Core/Graphics/Direct3D11/Buffer.hpp
    Core/Graphics/Direct3D11/Constants.hpp
    Core/Graphics/Direct3D11/DepthStencilBuffer.cpp
    Core/Graphics/Direct3D11/DepthStencilBuffer.hpp
    Core/Graphics/Direct3D11/Device.cpp
    Core/Graphics/Direct3D11/Device.hpp
    Core/Graphics/Direct3D11/FrameQuery.cpp
    Core/Graphics/Direct3D11/FrameQuery.hpp
    Core/Graphics/Direct3D11/LetterBoxingRenderer.cpp
    Core/Graphics/Direct3D11/LetterBoxingRenderer.hpp
    Core/Graphics/Direct3D11/Mesh.cpp
    Core/Graphics/Direct3D11/Mesh.hpp
    Core/Graphics/Direct3D11/MeshRenderer.cpp
    Core/Graphics/Direct3D11/MeshRenderer.hpp
    Core/Graphics/Direct3D11/RenderTarget.cpp
    Core/Graphics/Direct3D11/RenderTarget.hpp
    Core/Graphics/Direct3D11/Runtime.cpp
    Core/Graphics/Direct3D11/SamplerState.cpp
    Core/Graphics/Direct3D11/SamplerState.hpp
    Core/Graphics/Direct3D11/Texture2D.cpp
    Core/Graphics/Direct3D11/Texture2D.hpp
    Core/Graphics/Model_D3D11.cpp
    Core/Graphics/Model_D3D11.hpp
    Core/Graphics/Model_Shader_D3D11.cpp
    Core/Graphics/Renderer_D3D11.cpp
    Core/Graphics/Renderer_D3D11.hpp
    Core/Graphics/Renderer_Shader_D3D11.cpp
    Core/Graphics/SwapChain_D3D11.cpp
    Core/Graphics/SwapChain_D3D11.hpp
    Core/Graphics/WindowsImageComponent/WindowsImageComponentImage.cpp
)

set(core_graphics_sources ${core_graphics_common_sources})
if (NOT LUASTG_USE_SDLGPU)
    list(APPEND core_graphics_sources ${core_graphics_d3d11_sources})
endif ()

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
    Freetype::Freetype
    Microsoft.Windows.ImplementationLibrary
    PlatformAPI
    TracyAPI
    beautiful_win32_api
    libqoi
    simdutf::simdutf
    spdlog
    utf8
    utility
    win32
)

set_target_properties(Core.Graphics PROPERTIES FOLDER engine)

if (LUASTG_BUILD_SDLGPU_SMOKE OR LUASTG_USE_SDLGPU)
    add_subdirectory(sdlgpu)
endif ()
if (LUASTG_USE_SDLGPU)
    target_compile_definitions(Core.Graphics PUBLIC LUASTG_GRAPHICS_SDLGPU)
    target_link_libraries(Core.Graphics PUBLIC Core.Graphics.SDLGPU.Runtime)
else ()
    target_link_libraries(Core.Graphics PUBLIC
        DirectX::TextureMini imgui_impl_dx11 GeneratedShaderHeaders
        d3d11.lib dxgi.lib dxguid.lib tinygltf
    )
endif ()

if (LUASTG_BUILD_TESTS)
    add_executable(Core.Graphics.GeometryCapture test/GeometryCapture.cpp)
    luastg_target_common_options(Core.Graphics.GeometryCapture)
    luastg_target_more_warning(Core.Graphics.GeometryCapture)
    target_link_libraries(Core.Graphics.GeometryCapture PRIVATE Core.Graphics SDL3::SDL3 ole32)
    target_compile_definitions(Core.Graphics.GeometryCapture PRIVATE
        LUASTG_GEOMETRY_FIXTURES="${PROJECT_SOURCE_DIR}/data/test/res/geometry")
    set_target_properties(Core.Graphics.GeometryCapture PROPERTIES FOLDER engine/test)
    add_custom_command(TARGET Core.Graphics.GeometryCapture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>" "$<TARGET_FILE_DIR:Core.Graphics.GeometryCapture>"
        VERBATIM)
    if (LUASTG_USE_SDLGPU)
        luastg_copy_sdlgpu_runtime(Core.Graphics.GeometryCapture)
    else ()
        add_custom_command(TARGET Core.Graphics.GeometryCapture POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Microsoft.D3DCompiler.Redist>" "$<TARGET_FILE_DIR:Core.Graphics.GeometryCapture>"
            VERBATIM)
    endif ()
    # GPU captures are launched explicitly by the user, outside the CPU CTest suite.
endif ()
