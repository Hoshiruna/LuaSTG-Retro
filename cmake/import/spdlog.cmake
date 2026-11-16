# spdlog

set(LUASTG_SPDLOG_OPTIONS
    "SPDLOG_DISABLE_DEFAULT_LOGGER ON"
)
if(WIN32)
    list(APPEND LUASTG_SPDLOG_OPTIONS
        "SPDLOG_WCHAR_SUPPORT ON"
        "SPDLOG_WCHAR_FILENAMES ON"
        "SPDLOG_WCHAR_CONSOLE ON"
    )
endif()

CPMAddPackage(
    NAME spdlog
    VERSION 1.17.0
    GITHUB_REPOSITORY gabime/spdlog
    OPTIONS
    ${LUASTG_SPDLOG_OPTIONS}
)
unset(LUASTG_SPDLOG_OPTIONS)

if(TARGET spdlog)
    if(MSVC)
        target_compile_options(spdlog PUBLIC
            "/DSPDLOG_SHORT_LEVEL_NAMES={\"V\",\"D\",\"I\",\"W\",\"E\",\"F\",\"O\"}"
        )
    endif()
    set_target_properties(spdlog PROPERTIES FOLDER external)
endif()
