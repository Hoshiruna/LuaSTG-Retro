#option(LUASTG_RESDIR "Custom configurations" "res")

option(LUASTG_LINK_LUASOCKET "Link to luasocket" OFF)
option(LUASTG_LINK_TRACY_CLIENT "Link to Tracy client" OFF)
set(LUASTG_RESDIR "${CMAKE_SOURCE_DIR}/LuaSTG/LuaSTG/Custom" CACHE PATH "LuaSTG custom build configuration")

if (WIN32)
    set(luastg_runtime_default ON)
    set(luastg_tests_default ON)
else ()
    set(luastg_runtime_default OFF)
    set(luastg_tests_default OFF)
endif ()
option(LUASTG_BUILD_RUNTIME "Build LuaSTG and Setting" ${luastg_runtime_default})
option(LUASTG_BUILD_TESTS "Build engine tests" ${luastg_tests_default})
unset(luastg_runtime_default)
unset(luastg_tests_default)

function(luastg_cmake_option)
    set(option_args      FORCE)
    set(one_value_args   NAME TYPE HELP)
    set(multi_value_args VALUE)
    cmake_parse_arguments(PARSE_ARGV 0 arg "${option_args}" "${one_value_args}" "${multi_value_args}")
    if (CMAKE_VERSION VERSION_GREATER_EQUAL "4.2.0")
        set(CACHE{${arg_NAME}} TYPE ${arg_TYPE} HELP ${arg_HELP} VALUE ${arg_VALUE})
    else ()
        set(${arg_NAME} ${arg_VALUE} CACHE ${arg_TYPE} ${arg_HELP})
    endif ()
endfunction()

# LuaSTG - Configuration

luastg_cmake_option(
    NAME LUASTG_CONFIGURATION_DEFAULT_FILE_PATH
    TYPE STRING
    HELP "LuaSTG: Configuration: Default file path"
    VALUE "config.json"
)
luastg_cmake_option(
    NAME LUASTG_CONFIGURATION_LUA_SCRIPT_ENABLE
    TYPE BOOL
    HELP "LuaSTG: Configuration: Enable lua script"
    VALUE TRUE
)
luastg_cmake_option(
    NAME LUASTG_CONFIGURATION_LUA_SCRIPT_PATHS
    TYPE STRING
    HELP "LuaSTG: Configuration: Lua script paths"
    VALUE "launch;launch.lua"
)

# LuaSTG - Logging

luastg_cmake_option(
    NAME LUASTG_LOGGING_DEFAULT_FILE_PATH
    TYPE STRING
    HELP "LuaSTG: Logging: Default file path"
    VALUE "engine.log"
)

luastg_cmake_option(
    NAME LUASTG_LOGGING_CONSOLE_WINDOW_ALLOWED
    TYPE BOOL
    HELP "LuaSTG: Logging: Allow open an additional console window to print logs"
    VALUE ON
)

# LuaSTG - Discord RPC

luastg_cmake_option(
    NAME LUASTG_DISCORD_RPC_ENABLE
    TYPE BOOL
    HELP "LuaSTG: Discord RPC: Enable Discord Rich Presence support"
    VALUE TRUE
)
luastg_cmake_option(
    NAME LUASTG_DISCORD_RPC_APP_ID
    TYPE STRING
    HELP "LuaSTG: Discord RPC: Application (Client) ID"
    VALUE "0"
)

# LuaSTG - Steam API

luastg_cmake_option(
    NAME LUASTG_STEAM_API_ENABLE
    TYPE BOOL
    HELP "LuaSTG: Steam API: Enable and link to steam_api[64].dll"
    VALUE FALSE
)
luastg_cmake_option(
    NAME LUASTG_STEAM_API_APP_ID
    TYPE STRING
    HELP "LuaSTG: Steam API: APP ID"
    VALUE "0"
)
luastg_cmake_option(
    NAME LUASTG_STEAM_API_FORCE_LAUNCH_BY_STEAM
    TYPE BOOL
    HELP "LuaSTG: Steam API: Force launch by Steam"
    VALUE FALSE
)
