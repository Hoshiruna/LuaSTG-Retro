add_library(Core.Graphics.SDLGPU STATIC
    Resources.hpp
    GpuContext.hpp
    GpuContext.cpp
    ShaderCompiler.hpp
    ShaderCompiler.cpp
    TextureTransfer.hpp
    TextureTransfer.cpp
    PngWriter.hpp
    PngWriter.cpp
)
luastg_target_common_options(Core.Graphics.SDLGPU)
luastg_target_more_warning(Core.Graphics.SDLGPU)
target_include_directories(Core.Graphics.SDLGPU PUBLIC .)
target_link_libraries(Core.Graphics.SDLGPU
    PUBLIC SDL3::SDL3
    PRIVATE SDL3_shadercross::SDL3_shadercross-static Core.Logging nothings_stb
)
set_target_properties(Core.Graphics.SDLGPU PROPERTIES FOLDER engine)

file(MAKE_DIRECTORY
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/LuaSTG"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/ImGui"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/FreeType"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/spdlog"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/stb"
)
configure_file("${PROJECT_SOURCE_DIR}/License.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/LuaSTG/License.txt" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/imgui/imgui/LICENSE.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/ImGui/LICENSE.txt" COPYONLY)
configure_file("${libfreetype_SOURCE_DIR}/docs/FTL.TXT" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/FreeType/FTL.TXT" COPYONLY)
configure_file("${libfreetype_SOURCE_DIR}/LICENSE.TXT" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/FreeType/LICENSE.TXT" COPYONLY)
configure_file("${spdlog_SOURCE_DIR}/LICENSE" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/spdlog/LICENSE" COPYONLY)
configure_file("${nothings_stb_SOURCE_DIR}/LICENSE" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/stb/LICENSE" COPYONLY)

function(luastg_copy_sdlgpu_runtime target)
    target_link_libraries(${target} PRIVATE delayimp.lib)
    target_link_options(${target} PRIVATE /DELAYLOAD:dxcompiler.dll)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUASTG_SDLGPU_COMPILER_LIBRARIES} "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUASTG_SDLGPU_LICENSE_DIRECTORY}" "$<TARGET_FILE_DIR:${target}>/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/licenses" "$<TARGET_FILE_DIR:${target}>/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/README.md"
            "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/MIGRATION.md"
            "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/THIRD_PARTY_NOTICES.md"
            "$<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
endfunction()

if (LUASTG_USE_SDLGPU)
    add_library(Core.Graphics.SDLGPU.Runtime STATIC
        Device.hpp Device.cpp
        Renderer.hpp Renderer.cpp SpriteShaders.hpp
        SwapChain.hpp SwapChain.cpp Runtime.cpp
    )
    luastg_target_common_options(Core.Graphics.SDLGPU.Runtime)
    luastg_target_more_warning(Core.Graphics.SDLGPU.Runtime)
    target_include_directories(Core.Graphics.SDLGPU.Runtime PRIVATE ..)
    target_link_libraries(Core.Graphics.SDLGPU.Runtime PUBLIC
        Core.Graphics.SDLGPU Core.Image Core.WindowSystem Core.Logging imgui_impl_sdlgpu3
    )
    set_target_properties(Core.Graphics.SDLGPU.Runtime PROPERTIES FOLDER engine)
endif ()

if (LUASTG_BUILD_SDLGPU_SMOKE)
add_executable(Core.Graphics.SDLGPU.Smoke
    Scene.hpp
    Scene.cpp
    Shaders.hpp
    smoke/Main.cpp
    smoke/Options.hpp
    smoke/Options.cpp
    smoke/Verification.hpp
    smoke/Verification.cpp
)
luastg_target_common_options(Core.Graphics.SDLGPU.Smoke)
luastg_target_more_warning(Core.Graphics.SDLGPU.Smoke)
target_link_libraries(Core.Graphics.SDLGPU.Smoke PRIVATE
    Core.Graphics.SDLGPU
    Core.WindowSystem
    Core.InputSystem
    Core.Logging
    spdlog
    imgui_impl_sdl3
    imgui_impl_sdlgpu3
    delayimp.lib
)

set_target_properties(Core.Graphics.SDLGPU.Smoke PROPERTIES
    FOLDER engine/test
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sdlgpu-smoke/$<CONFIG>"
)
luastg_copy_sdlgpu_runtime(Core.Graphics.SDLGPU.Smoke)
endif ()