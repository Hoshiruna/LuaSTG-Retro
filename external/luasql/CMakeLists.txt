# LuaSQL

CPMAddPackage(
    NAME luasql
    VERSION 2.8.1
    GITHUB_REPOSITORY lunarmodules/luasql
    GIT_TAG 2.8.1
    DOWNLOAD_ONLY YES
)

set(LUASTG_SQLITE3_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../luasql-sqlite3")

<<<<<<< HEAD
set_source_files_properties(
    ${luasql_SOURCE_DIR}/src/luasql.c
    PROPERTIES
        COMPILE_DEFINITIONS luaL_setfuncs=luasql_luaL_setfuncs
)

=======
>>>>>>> c0a1187dfc23bb78fed3d6d861ad65714896ff4f
add_library(lua_luasql_sqlite3 STATIC)
luastg_target_common_options(lua_luasql_sqlite3)
target_compile_definitions(lua_luasql_sqlite3 PRIVATE
    _CRT_SECURE_NO_WARNINGS
    LUASQL_VERSION_NUMBER="2.8.1"
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_THREADSAFE=1
)
target_include_directories(lua_luasql_sqlite3 PRIVATE
    ${LUASTG_SQLITE3_SOURCE_DIR}
    ${luasql_SOURCE_DIR}/src
)
target_sources(lua_luasql_sqlite3 PRIVATE
    ${LUASTG_SQLITE3_SOURCE_DIR}/sqlite3.c
    ${LUASTG_SQLITE3_SOURCE_DIR}/sqlite3.h
    ${LUASTG_SQLITE3_SOURCE_DIR}/sqlite3ext.h
    ${luasql_SOURCE_DIR}/src/luasql.c
    ${luasql_SOURCE_DIR}/src/luasql.h
    ${luasql_SOURCE_DIR}/src/ls_sqlite3.c
)
target_link_libraries(lua_luasql_sqlite3 PUBLIC
    lua51_static
)
if(MSVC)
    target_compile_options(lua_luasql_sqlite3 PRIVATE
        /wd4018 # signed/unsigned mismatch
        /wd4055 # data pointer to function pointer
        /wd4090 # different const qualifiers
        /wd4100 # unreferenced formal parameter
        /wd4127 # conditional expression is constant
        /wd4132 # const object should be initialized
        /wd4146 # unary minus applied to unsigned type
        /wd4244 # conversion, possible loss of data
        /wd4267 # conversion from size_t
        /wd4701 # potentially uninitialized local variable
        /wd4702 # unreachable code
        /wd4706 # assignment within conditional expression
    )
endif()
set_target_properties(lua_luasql_sqlite3 PROPERTIES FOLDER lualib)
