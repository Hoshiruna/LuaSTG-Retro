
add_executable(Setting WIN32)
luastg_target_common_options(Setting)
luastg_target_more_warning(Setting)
target_precompile_headers(Setting PRIVATE
    SharedHeaders.h
)
target_include_directories(Setting PRIVATE
    .
)
target_sources(Setting PRIVATE
    SharedHeaders.h
    Main.cpp
    Configurer.manifest
)
target_link_libraries(Setting PRIVATE
    Core.Graphics
    spdlog
    utf8
    win32
    beautiful_win32_api
    imgui
    imgui_impl_sdl3
    nlohmann_json
    Core.WindowSystem
    SDL3::SDL3
)

luastg_target_copy_to_bin_directory(Setting Setting)

if (LUASTG_USE_SDLGPU)
    luastg_copy_sdlgpu_runtime(Setting)
    add_custom_command(TARGET Setting POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${LUASTG_SDLGPU_COMPILER_LIBRARIES} "${CMAKE_BINARY_DIR}/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${LUASTG_SDLGPU_LICENSE_DIRECTORY}" "${CMAKE_BINARY_DIR}/bin/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/licenses" "${CMAKE_BINARY_DIR}/bin/licenses"
        VERBATIM
    )
endif ()