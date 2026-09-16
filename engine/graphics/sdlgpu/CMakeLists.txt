add_library(Core.Graphics.SDLGPU STATIC
    Resources.hpp
    GpuContext.hpp
    GpuContext.cpp
    ShaderCompiler.hpp
    ShaderCompiler.cpp
)
luastg_target_common_options(Core.Graphics.SDLGPU)
luastg_target_more_warning(Core.Graphics.SDLGPU)
target_include_directories(Core.Graphics.SDLGPU PUBLIC .)
target_link_libraries(Core.Graphics.SDLGPU
    PUBLIC SDL3::SDL3
    PRIVATE SDL3_shadercross::SDL3_shadercross-static Core.Logging
)
set_target_properties(Core.Graphics.SDLGPU PROPERTIES FOLDER engine)

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

file(MAKE_DIRECTORY
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/LuaSTG"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/ImGui"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/FreeType"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/spdlog"
)
configure_file("${PROJECT_SOURCE_DIR}/License.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/LuaSTG/License.txt" COPYONLY)
configure_file("${PROJECT_SOURCE_DIR}/imgui/imgui/LICENSE.txt" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/ImGui/LICENSE.txt" COPYONLY)
configure_file("${libfreetype_SOURCE_DIR}/docs/FTL.TXT" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/FreeType/FTL.TXT" COPYONLY)
configure_file("${libfreetype_SOURCE_DIR}/LICENSE.TXT" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/FreeType/LICENSE.TXT" COPYONLY)
configure_file("${spdlog_SOURCE_DIR}/LICENSE" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/spdlog/LICENSE" COPYONLY)
# Check and load DXC explicitly at startup before the first delay-loaded call.
target_link_options(Core.Graphics.SDLGPU.Smoke PRIVATE /DELAYLOAD:dxcompiler.dll)
set_target_properties(Core.Graphics.SDLGPU.Smoke PROPERTIES
    FOLDER engine/test
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sdlgpu-smoke/$<CONFIG>"
)

add_custom_command(TARGET Core.Graphics.SDLGPU.Smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUASTG_SDLGPU_COMPILER_LIBRARIES} "$<TARGET_FILE_DIR:Core.Graphics.SDLGPU.Smoke>"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUASTG_SDLGPU_LICENSE_DIRECTORY}" "$<TARGET_FILE_DIR:Core.Graphics.SDLGPU.Smoke>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/licenses" "$<TARGET_FILE_DIR:Core.Graphics.SDLGPU.Smoke>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/README.md" "${CMAKE_CURRENT_SOURCE_DIR}/MIGRATION.md" "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "$<TARGET_FILE_DIR:Core.Graphics.SDLGPU.Smoke>"
    VERBATIM
)
