# ==================== beautiful win32 api ====================

add_subdirectory(beautiful-win32-api)
set_target_properties(beautiful_win32_api PROPERTIES FOLDER external)

# ==================== luajit ====================

add_subdirectory(luajit2)
set_target_properties(minilua PROPERTIES FOLDER luajit)
set_target_properties(buildvm PROPERTIES FOLDER luajit)
set_target_properties(lua51_static PROPERTIES FOLDER luajit)
set_target_properties(lua51 PROPERTIES FOLDER luajit)
set_target_properties(luajit PROPERTIES FOLDER luajit)

# ==================== lua filesystem ====================

add_library(lua_filesystem STATIC)
luastg_target_common_options(lua_filesystem)
luastg_target_more_warning(lua_filesystem)
target_include_directories(lua_filesystem PUBLIC
    lua-filesystem-lite
)
target_sources(lua_filesystem PRIVATE
    lua-filesystem-lite/lfs.h
    lua-filesystem-lite/lfs.cpp
)
target_link_libraries(lua_filesystem PUBLIC
    lua51_static
)

set_target_properties(lua_filesystem PROPERTIES FOLDER lualib)

# ==================== lua cjson ====================

add_library(lua_cjson STATIC)
luastg_target_common_options(lua_cjson)
target_compile_options(lua_cjson PRIVATE
    "/wd4090" # 不同的const限定符（指针传递）
)
target_compile_definitions(lua_cjson PRIVATE
    _CRT_SECURE_NO_WARNINGS
    DISABLE_INVALID_NUMBERS
    ENABLE_CJSON_GLOBAL
)
target_include_directories(lua_cjson PUBLIC
    lua-cjson-patch
)
target_include_directories(lua_cjson PRIVATE
    lua-cjson
)
target_sources(lua_cjson PRIVATE
    lua-cjson-patch/lua_cjson.h
    lua-cjson/lua_cjson.c
    lua-cjson/strbuf.h
    lua-cjson/strbuf.c
    lua-cjson/fpconv.h
    lua-cjson/fpconv.c
)
target_link_libraries(lua_cjson PUBLIC
    lua51_static
)

set_target_properties(lua_cjson PROPERTIES FOLDER lualib)

# ==================== lua sqlite ====================

add_library(lua_sqlite3 STATIC)
luastg_target_common_options(lua_sqlite3)
target_compile_definitions(lua_sqlite3
PRIVATE
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_THREADSAFE=1
)
target_include_directories(lua_sqlite3 PUBLIC
    lua-sqlite3
)
target_sources(lua_sqlite3 PRIVATE
    lua-sqlite3/sqlite3.c
    lua-sqlite3/sqlite3.h
    lua-sqlite3/sqlite3ext.h
)
if(MSVC)
    target_compile_options(lua_sqlite3 PRIVATE
        /wd4018 # signed/unsigned mismatch
        /wd4055 # data pointer to function pointer
        /wd4100 # unreferenced formal parameter
        /wd4127 # conditional expression is constant
        /wd4132 # const object should be initialized
        /wd4146 # unary minus applied to unsigned type
        /wd4244 # conversion, possible loss of data
        /wd4701 # potentially uninitialized local variable
        /wd4702 # unreachable code
        /wd4706 # assignment within conditional expression
    )
endif()
set_target_properties(lua_sqlite3 PROPERTIES FOLDER external)

add_library(lua_sqlite STATIC)
luastg_target_common_options(lua_sqlite)
target_sources(lua_sqlite PRIVATE
    ../LuaSTG/LuaSTG/LuaBinding/external/lua_sqlite.hpp
    ../LuaSTG/LuaSTG/LuaBinding/external/lua_sqlite.cpp
)
target_link_libraries(lua_sqlite PUBLIC
    lua51_static
    lua_sqlite3
)
set_target_properties(lua_sqlite PROPERTIES FOLDER lualib)

# ==================== lua csv ====================

#add_library(lua_xlsx_csv STATIC)
#luastg_target_common_options(lua_xlsx_csv)
#target_include_directories(lua_xlsx_csv PUBLIC
#    lua-csv
#)
#target_sources(lua_xlsx_csv PRIVATE
#    lua-csv/lua_xlsx_csv.h
#    lua-csv/lua_xlsx.cpp
#    lua-csv/lua_csv.cpp
#)
#target_link_libraries(lua_xlsx_csv PUBLIC
#    luajit
#    zip
#    pugixml
#)
#
#set_target_properties(lua_xlsx_csv PROPERTIES FOLDER external)

# ==================== xmath ====================

add_library(xmath STATIC)
luastg_target_common_options(xmath)
target_include_directories(xmath PUBLIC
    xmath-patch
    xmath
    .
)
target_sources(xmath PRIVATE
    xmath-patch/math/Vec2.h
    xmath-patch/math/Vec2.cpp
    xmath/meow_fft.c
    xmath/meow_fft.h
    xmath/XCollision.cpp
    xmath/XCollision.h
    xmath/XComplex.cpp
    xmath/XComplex.h
    xmath/XConstant.h
    xmath/XDistance.cpp
    xmath/XDistance.h
    xmath/XEquation.cpp
    xmath/XEquation.h
    xmath/XFFT.cpp
    xmath/XFFT.h
    xmath/XIntersect.cpp
    xmath/XIntersect.h
    xmath/XMath.h
    xmath/XRandom.cpp
    xmath/XRandom.h
    xmath/XSpline.cpp
    xmath/XSpline.h
    xmath/XTween.cpp
    xmath/XTween.h
)

set_target_properties(xmath PROPERTIES FOLDER external)

# ==================== image qoi ====================

add_library(libqoi STATIC)
target_compile_definitions(libqoi PRIVATE
    QOI_NO_STDIO
    QOI_IMPLEMENTATION
)
target_include_directories(libqoi PUBLIC
    image.qoi
    image.qoi-patch
)
target_sources(libqoi PRIVATE
    image.qoi/qoi.h
    image.qoi-patch/qoi.c
    image.qoi-patch/QOITextureLoader11.h
    image.qoi-patch/QOITextureLoader11.cpp
)

set_target_properties(libqoi PROPERTIES FOLDER external)

# ==================== steam api ====================

add_subdirectory(steam_api)

# ==================== discord rpc ====================

set(BUILD_EXAMPLES OFF CACHE BOOL "Build discord-rpc examples" FORCE)
set(CLANG_FORMAT_CMD "" CACHE FILEPATH "Disable discord-rpc clang-format target" FORCE)
add_subdirectory(discord-rpc)
set_target_properties(discord-rpc PROPERTIES FOLDER external)

add_subdirectory(tracy-patch)
