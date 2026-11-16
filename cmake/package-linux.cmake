if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    return()
endif ()

set_target_properties(LuaSTG PROPERTIES INSTALL_RPATH "$ORIGIN")

install(TARGETS LuaSTG
    RUNTIME_DEPENDENCY_SET luastg_linux_runtime
    RUNTIME DESTINATION . COMPONENT LinuxRuntime
)
install(IMPORTED_RUNTIME_ARTIFACTS DirectXShaderCompiler::dxcompiler
    RUNTIME_DEPENDENCY_SET luastg_linux_runtime
    LIBRARY DESTINATION . COMPONENT LinuxRuntime
)
install(RUNTIME_DEPENDENCY_SET luastg_linux_runtime
    PRE_EXCLUDE_REGEXES
        "^ld-linux.*"
        "^libc\\.so.*"
        "^libm\\.so.*"
        "^libpthread\\.so.*"
        "^libdl\\.so.*"
        "^librt\\.so.*"
        "^libresolv\\.so.*"
        "^libutil\\.so.*"
        "^libstdc\\+\\+\\.so.*"
        "^libgcc_s\\.so.*"
    LIBRARY DESTINATION . COMPONENT LinuxRuntime
)

install(PROGRAMS "${PROJECT_SOURCE_DIR}/tool/run-luastg.sh"
    DESTINATION . RENAME run-luastg COMPONENT LinuxRuntime)
install(DIRECTORY
    "${PROJECT_SOURCE_DIR}/data/example/assets"
    "${PROJECT_SOURCE_DIR}/data/example/scripts"
    DESTINATION . COMPONENT LinuxRuntime PATTERN ".git" EXCLUDE)
install(FILES "${PROJECT_SOURCE_DIR}/data/example/config.json" "${PROJECT_SOURCE_DIR}/License.txt"
    DESTINATION . COMPONENT LinuxRuntime)
install(DIRECTORY
    "${PROJECT_SOURCE_DIR}/data/license/"
    "${LUASTG_SDLGPU_LICENSE_DIRECTORY}/"
    "${PROJECT_SOURCE_DIR}/engine/graphics/sdlgpu/licenses/"
    DESTINATION licenses COMPONENT LinuxRuntime PATTERN ".git" EXCLUDE)
install(FILES "${LUASTG_CURL_NOTICE}" DESTINATION licenses/curl COMPONENT LinuxRuntime)
install(FILES "${LUASTG_EFSW_NOTICE}" DESTINATION licenses/efsw COMPONENT LinuxRuntime)
if (LUASTG_IMAGE_JPEG_ENABLE)
    install(FILES "${libjpeg_turbo_SOURCE_DIR}/LICENSE.md" "${libjpeg_turbo_SOURCE_DIR}/README.ijg"
        DESTINATION licenses/libjpeg-turbo COMPONENT LinuxRuntime)
endif ()
if (LUASTG_IMAGE_PNG_ENABLE)
    install(FILES "${libpng_SOURCE_DIR}/LICENSE" DESTINATION licenses/libpng COMPONENT LinuxRuntime)
endif ()
if (LUASTG_IMAGE_WEBP_ENABLE)
    install(FILES "${libwebp_SOURCE_DIR}/COPYING" "${libwebp_SOURCE_DIR}/PATENTS"
        DESTINATION licenses/libwebp COMPONENT LinuxRuntime)
endif ()

find_file(LUASTG_NOTO_CJK_FONT NAMES NotoSansCJK-Regular.ttc
    PATHS /usr/share/fonts/opentype/noto /usr/share/fonts/noto-cjk
    DOC "Noto Sans CJK Regular font to include in the Linux example")
find_file(LUASTG_NOTO_CJK_LICENSE NAMES copyright LICENSE OFL.txt
    PATHS /usr/share/doc/fonts-noto-cjk /usr/share/licenses/noto-fonts-cjk
    DOC "License text for the bundled Noto Sans CJK font")
if (LUASTG_NOTO_CJK_FONT AND LUASTG_NOTO_CJK_LICENSE)
    install(FILES "${LUASTG_NOTO_CJK_FONT}" DESTINATION assets/fonts COMPONENT LinuxRuntime)
    install(FILES "${LUASTG_NOTO_CJK_LICENSE}" DESTINATION licenses/NotoSansCJK COMPONENT LinuxRuntime)
    add_custom_command(TARGET LuaSTG POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/data/example/assets" "$<TARGET_FILE_DIR:LuaSTG>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/data/example/scripts" "$<TARGET_FILE_DIR:LuaSTG>/scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/data/example/config.json" "$<TARGET_FILE_DIR:LuaSTG>/config.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:LuaSTG>/assets/fonts" "$<TARGET_FILE_DIR:LuaSTG>/licenses/NotoSansCJK"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUASTG_NOTO_CJK_FONT}" "$<TARGET_FILE_DIR:LuaSTG>/assets/fonts/NotoSansCJK-Regular.ttc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUASTG_NOTO_CJK_LICENSE}" "$<TARGET_FILE_DIR:LuaSTG>/licenses/NotoSansCJK/copyright"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/data/example/assets" "${CMAKE_BINARY_DIR}/bin/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/data/example/scripts" "${CMAKE_BINARY_DIR}/bin/scripts"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/data/example/config.json" "${CMAKE_BINARY_DIR}/bin/config.json"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/bin/assets/fonts" "${CMAKE_BINARY_DIR}/bin/licenses/NotoSansCJK"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUASTG_NOTO_CJK_FONT}" "${CMAKE_BINARY_DIR}/bin/assets/fonts/NotoSansCJK-Regular.ttc"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LUASTG_NOTO_CJK_LICENSE}" "${CMAKE_BINARY_DIR}/bin/licenses/NotoSansCJK/copyright"
        VERBATIM)
else ()
    install(CODE [[
        message(FATAL_ERROR "Linux packaging requires NotoSansCJK-Regular.ttc and its license. Install fonts-noto-cjk or set LUASTG_NOTO_CJK_FONT and LUASTG_NOTO_CJK_LICENSE, then configure again.")
    ]] COMPONENT LinuxRuntime)
endif ()

find_file(LUASTG_OPENSSL_LICENSE NAMES copyright LICENSE.txt LICENSE
    PATHS /usr/share/doc/libssl-dev /usr/share/doc/libssl3t64 /usr/share/doc/libssl3
        /usr/share/licenses/openssl
    DOC "License text for the system OpenSSL dependency included in the Linux package")
if (LUASTG_OPENSSL_LICENSE)
    install(FILES "${LUASTG_OPENSSL_LICENSE}" DESTINATION licenses/OpenSSL COMPONENT LinuxRuntime)
else ()
    install(CODE [[
        message(FATAL_ERROR "Linux packaging requires the OpenSSL license text. Set LUASTG_OPENSSL_LICENSE and configure again.")
    ]] COMPONENT LinuxRuntime)
endif ()

set(CPACK_GENERATOR TGZ)
set(CPACK_PACKAGE_NAME LuaSTG-Retro)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "LuaSTG-Retro-v${PROJECT_VERSION}-linux-x64")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/releases")
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};${CMAKE_PROJECT_NAME};LinuxRuntime;/")
include(CPack)
