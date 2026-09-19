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
target_include_directories(Core.Graphics.SDLGPU PUBLIC . PRIVATE "${luastg_dxc_SOURCE_DIR}/inc")
target_link_libraries(Core.Graphics.SDLGPU
    PUBLIC SDL3::SDL3
    PRIVATE SDL3_shadercross::SDL3_shadercross-static DirectXShaderCompiler::dxcompiler Core.Logging nothings_stb
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
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/THIRD_PARTY_NOTICES.md" COPYONLY)
if (LUASTG_USE_SDLGPU)
    file(MAKE_DIRECTORY "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/TinyGLTF")
    configure_file("${tinygltf_SOURCE_DIR}/LICENSE" "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/TinyGLTF/LICENSE" COPYONLY)
endif ()

function(luastg_copy_sdlgpu_runtime target)
    target_link_libraries(${target} PRIVATE delayimp.lib)
    target_link_options(${target} PRIVATE /DELAYLOAD:dxcompiler.dll)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUASTG_SDLGPU_COMPILER_LIBRARIES} "$<TARGET_FILE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUASTG_SDLGPU_LICENSE_DIRECTORY}" "$<TARGET_FILE_DIR:${target}>/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/licenses" "$<TARGET_FILE_DIR:${target}>/licenses"
        VERBATIM
    )
endfunction()

if (LUASTG_USE_SDLGPU)
    add_library(Core.Graphics.SDLGPU.Runtime STATIC
        Device.hpp Device.cpp
        Renderer.hpp Renderer.cpp SpriteShaders.hpp
        Mesh.hpp Mesh.cpp MeshRenderer.cpp MeshShaders.hpp
        ModelData.hpp ModelData.cpp Model.hpp Model.cpp ModelRenderer.cpp ModelShaders.hpp
        ShaderSource.hpp ShaderSource.cpp
        PostEffectShader.hpp PostEffectShader.cpp
        PostEffectReflection.hpp PostEffectReflection.cpp
        PostEffectSampling.hpp PostEffectSampling.cpp
        PostEffectRenderer.cpp
        SwapChain.hpp SwapChain.cpp Runtime.cpp
    )
    luastg_target_common_options(Core.Graphics.SDLGPU.Runtime)
    luastg_target_more_warning(Core.Graphics.SDLGPU.Runtime)
    target_include_directories(Core.Graphics.SDLGPU.Runtime PRIVATE .. "${luastg_dxc_SOURCE_DIR}/inc")
    target_link_libraries(Core.Graphics.SDLGPU.Runtime PUBLIC
        Core.Graphics.SDLGPU Core.Image Core.WindowSystem Core.Logging imgui_impl_sdlgpu3
    )
    target_link_libraries(Core.Graphics.SDLGPU.Runtime PRIVATE spirv-cross-c DirectXShaderCompiler::dxcompiler Core.FileSystem tinygltf)
    set_target_properties(Core.Graphics.SDLGPU.Runtime PROPERTIES FOLDER engine)
    if (LUASTG_BUILD_TESTS)
        add_executable(Core.Graphics.SDLGPU.ShaderTest test/ShaderTest.cpp test/GeometryTest.cpp)
        target_compile_definitions(Core.Graphics.SDLGPU.ShaderTest PRIVATE
            LUASTG_SHADER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures"
            LUASTG_GEOMETRY_FIXTURES="${PROJECT_SOURCE_DIR}/data/test/res/geometry")
        luastg_target_common_options(Core.Graphics.SDLGPU.ShaderTest)
        luastg_target_more_warning(Core.Graphics.SDLGPU.ShaderTest)
        target_include_directories(Core.Graphics.SDLGPU.ShaderTest PRIVATE ..)
        target_link_libraries(Core.Graphics.SDLGPU.ShaderTest PRIVATE
            Core.Graphics.SDLGPU.Runtime Core.FileSystem SDL3_shadercross::SDL3_shadercross-static GTest::gtest_main spdlog
        )
        set_target_properties(Core.Graphics.SDLGPU.ShaderTest PROPERTIES FOLDER engine/test)
        luastg_copy_sdlgpu_runtime(Core.Graphics.SDLGPU.ShaderTest)
        add_test(NAME Core.Graphics.SDLGPU.Shaders COMMAND Core.Graphics.SDLGPU.ShaderTest)
        set_tests_properties(Core.Graphics.SDLGPU.Shaders PROPERTIES
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/test" TIMEOUT 60
        )
    endif ()
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
