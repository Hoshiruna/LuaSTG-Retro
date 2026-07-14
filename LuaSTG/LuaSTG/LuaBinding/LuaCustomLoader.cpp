#include "LuaBinding/LuaCustomLoader.hpp"
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include <algorithm>
#include <cwchar>
#include <filesystem>

namespace {
    char loaded_module_physical_paths_registry_key;

    std::u8string_view as_utf8_path(std::string_view const path) {
        return { reinterpret_cast<char8_t const*>(path.data()), path.size() };
    }

    std::string from_utf8_path(std::u8string_view const path) {
        return { reinterpret_cast<char const*>(path.data()), path.size() };
    }

    bool canonicalize_physical_path(std::string_view const path, std::string& result) noexcept {
        try {
            std::error_code error;
            auto absolute_path = std::filesystem::absolute(std::filesystem::path(as_utf8_path(path)), error);
            if (error) {
                return false;
            }

            auto canonical_path = std::filesystem::weakly_canonical(absolute_path, error);
            if (error) {
                canonical_path = absolute_path.lexically_normal();
            }

            result = from_utf8_path(canonical_path.generic_u8string());
            return true;
        }
        catch (...) {
            return false;
        }
    }

    bool physical_paths_equal(std::string_view const left, std::string_view const right) noexcept {
        try {
#ifdef _WIN32
            auto const left_path = std::filesystem::path(as_utf8_path(left));
            auto const right_path = std::filesystem::path(as_utf8_path(right));
            return _wcsicmp(left_path.c_str(), right_path.c_str()) == 0;
#else
            return left == right;
#endif
        }
        catch (...) {
            return false;
        }
    }

    bool push_loaded_module_physical_paths(lua_State* const L, bool const create) {
        lua_pushlightuserdata(L, &loaded_module_physical_paths_registry_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        if (lua_istable(L, -1)) {
            return true;
        }
        lua_pop(L, 1);
        if (!create) {
            return false;
        }

        lua_newtable(L);                              // state
        lua_newtable(L);                              // state paths
        lua_rawseti(L, -2, 1);                        // state

        lua_newtable(L);                              // state modules
        lua_newtable(L);                              // state modules mt
        lua_pushliteral(L, "v");                     // state modules mt "v"
        lua_setfield(L, -2, "__mode");               // state modules mt
        lua_setmetatable(L, -2);                      // state modules
        lua_rawseti(L, -2, 2);                        // state

        lua_pushlightuserdata(L, &loaded_module_physical_paths_registry_key);
        lua_pushvalue(L, -2);
        lua_rawset(L, LUA_REGISTRYINDEX);
        return true;
    }

    void record_loaded_module_physical_path(lua_State* const L, int const module_index) {
        push_loaded_module_physical_paths(L, true);   // ... state

        lua_rawgeti(L, -1, 1);                        // ... state paths
        lua_pushvalue(L, lua_upvalueindex(2));         // ... state paths name
        lua_pushvalue(L, lua_upvalueindex(3));         // ... state paths name path
        lua_rawset(L, -3);                             // ... state paths
        lua_pop(L, 1);                                 // ... state

        lua_rawgeti(L, -1, 2);                         // ... state modules
        lua_pushvalue(L, lua_upvalueindex(2));         // ... state modules name
        lua_pushvalue(L, module_index);                // ... state modules name module
        lua_rawset(L, -3);                             // ... state modules
        lua_pop(L, 2);                                 // ...
    }

    int package_loader_tracked(lua_State* const L) {
        int const argument_count = lua_gettop(L);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_call(L, argument_count, LUA_MULTRET);

        if (lua_istable(L, 1)) {
            record_loaded_module_physical_path(L, 1);
        }
        return lua_gettop(L);
    }

    bool is_current_loaded_module(lua_State* const L, int const modules_index, int const loaded_index, int const key_index) {
        lua_pushvalue(L, key_index);
        lua_rawget(L, modules_index);                  // ... tracked_module
        lua_pushvalue(L, key_index);
        lua_rawget(L, loaded_index);                   // ... tracked_module loaded_module
        bool const current = lua_istable(L, -2) && lua_rawequal(L, -2, -1);
        lua_pop(L, 2);
        return current;
    }
}

static int readable(const char* filename) {
    try {
        return core::FileSystemManager::hasFile(filename) ? 1 : 0;
    }
    catch(...) {}
    return 0;
}

static const char* pushnexttemplate(lua_State* L, const char* path) {
    const char* l;
    while (*path == *LUA_PATHSEP) path++;  /* skip separators */
    if (*path == '\0') return NULL;  /* no more templates */
    l = strchr(path, *LUA_PATHSEP);  /* find next separator */
    if (l == NULL) l = path + strlen(path);
    lua_pushlstring(L, path, (size_t)(l - path));  /* template */
    return l;
}

static const char* searchpath(lua_State* L, const char* name, const char* path, const char* sep, const char* dirsep) {
    luaL_Buffer msg;  /* to build error message */
    luaL_buffinit(L, &msg);
    if (*sep != '\0')  /* non-empty separator? */
        name = luaL_gsub(L, name, sep, dirsep);  /* replace it by 'dirsep' */
    while ((path = pushnexttemplate(L, path)) != NULL) {
        const char* filename = luaL_gsub(L, lua_tostring(L, -1),
            LUA_PATH_MARK, name);
        lua_remove(L, -2);  /* remove path template */
        if (readable(filename))  /* does file exist and is readable? */
            return filename;  /* return that file name */
        lua_pushfstring(L, "\n\tno file " LUA_QS, filename);
        lua_remove(L, -2);  /* remove file name */
        luaL_addvalue(&msg);  /* concatenate error msg. entry */
    }
    luaL_pushresult(&msg);  /* create error message */
    return NULL;  /* not found */
}

static const char* findfile(lua_State* L, const char* name, const char* pname) {
    std::string path;
    lua_getglobal(L, "package");                                               // ??? t
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "path");                                           // ??? t s
        if (lua_isstring(L, -1)) {
            path = lua_tostring(L, -1);
        }
        else {
            luaL_error(L, LUA_QL("package.%s") " must be a string", pname);
        }
        lua_pop(L, 1);                                                         // ??? t
    }
    else {
        luaL_error(L, LUA_QL("package") " must be a table");
    }
    lua_pop(L, 1);                                                             // ???
    return searchpath(L, name, path.c_str(), ".", "/");
}

static void loaderror(lua_State* L, const char* filename) {
    luaL_error(L, "error loading module " LUA_QS " from file " LUA_QS ":\n\t%s",
        lua_tostring(L, 1), filename, lua_tostring(L, -1));
}

static int package_loader_luastg(lua_State* L) {
    const char* filename;
    const char* name = luaL_checkstring(L, 1);
    filename = findfile(L, name, "path");
    if (filename == NULL) return 1;  /* library not found in this path */
    //if (luaL_loadfile(L, filename) != 0)
        //loaderror(L, filename);
    core::SmartReference<core::IData> src;
    if (!core::FileSystemManager::readFile(filename, src.put()))
        loaderror(L, filename);
    else {
#ifndef NDEBUG
        spdlog::info(R"(require "{}" from {})", name, filename);
#endif
        if (luaL_loadbuffer(L,
            (char*)src->data(),
            src->size(),
            filename) != 0)
            loaderror(L, filename);

        std::string physical_path;
        std::string canonical_path;
        try {
            if (core::FileSystemManager::resolvePhysicalPath(filename, physical_path)
                && canonicalize_physical_path(physical_path, canonical_path)) {
                lua_pushlstring(L, name, strlen(name));
                lua_pushlstring(L, canonical_path.data(), canonical_path.size());
                lua_pushcclosure(L, package_loader_tracked, 3);
            }
        }
        catch (...) {
            // Source tracking must not affect normal module loading.
        }
    }
    return 1;  /* library loaded successfully */
}

namespace luastg
{
	void lua_register_custom_loader(lua_State* L) {
        lua_getglobal(L, "package");                         // ??? t
        if (lua_istable(L, -1)) {
            lua_getfield(L, -1, "loaders");                  // ??? t t
            if (lua_istable(L, -1)) {
                lua_pushinteger(L, lua_objlen(L, -1) + 1);   // ??? t t i
                lua_pushcfunction(L, package_loader_luastg); // ??? t t i f
                lua_settable(L, -3);                         // ??? t t
            }
            lua_pop(L, 1);                                   // ??? t
        }
        lua_pop(L, 1);                                       // ???
	}

	bool lua_get_loaded_module_physical_path(lua_State* const L, std::string_view const module_name, std::string& path) {
        path.clear();
        int const top = lua_gettop(L);
        bool found = false;

        if (push_loaded_module_physical_paths(L, false)) {    // state
            lua_rawgeti(L, -1, 1);                            // state paths
            int const paths_index = lua_gettop(L);
            lua_rawgeti(L, -2, 2);                            // state paths modules
            int const modules_index = lua_gettop(L);
            lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");  // state paths modules loaded
            int const loaded_index = lua_gettop(L);

            if (lua_istable(L, loaded_index)) {
                lua_pushlstring(L, module_name.data(), module_name.size());
                int const key_index = lua_gettop(L);
                if (is_current_loaded_module(L, modules_index, loaded_index, key_index)) {
                    lua_pushvalue(L, key_index);
                    lua_rawget(L, paths_index);
                    if (lua_isstring(L, -1)) {
                        size_t length = 0;
                        char const* const value = lua_tolstring(L, -1, &length);
                        path.assign(value, length);
                        found = true;
                    }
                }
            }
        }

        lua_settop(L, top);
        return found;
	}

	void lua_get_loaded_module_physical_paths(lua_State* const L, std::vector<std::pair<std::string, std::string>>& modules) {
        modules.clear();
        int const top = lua_gettop(L);

        if (push_loaded_module_physical_paths(L, false)) {    // state
            lua_rawgeti(L, -1, 1);                            // state paths
            int const paths_index = lua_gettop(L);
            lua_rawgeti(L, -2, 2);                            // state paths modules
            int const modules_index = lua_gettop(L);
            lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");  // state paths modules loaded
            int const loaded_index = lua_gettop(L);

            if (lua_istable(L, loaded_index)) {
                lua_pushnil(L);
                while (lua_next(L, paths_index) != 0) {       // ... key path
                    int const key_index = lua_gettop(L) - 1;
                    if (lua_isstring(L, key_index) && lua_isstring(L, -1)
                        && is_current_loaded_module(L, modules_index, loaded_index, key_index)) {
                        size_t name_length = 0;
                        size_t path_length = 0;
                        char const* const name = lua_tolstring(L, key_index, &name_length);
                        char const* const path = lua_tolstring(L, -1, &path_length);
                        modules.emplace_back(
                            std::string(name, name_length),
                            std::string(path, path_length));
                    }
                    lua_pop(L, 1);
                }
            }
        }

        lua_settop(L, top);
        std::ranges::sort(modules, {}, &std::pair<std::string, std::string>::first);
	}

	void lua_get_loaded_modules_by_physical_path(lua_State* const L, std::string_view const path, std::vector<std::string>& modules) {
        modules.clear();

        std::string canonical_path;
        if (!canonicalize_physical_path(path, canonical_path)) {
            return;
        }

        std::vector<std::pair<std::string, std::string>> loaded_modules;
        lua_get_loaded_module_physical_paths(L, loaded_modules);
        for (auto const& [module_name, module_path] : loaded_modules) {
            if (physical_paths_equal(canonical_path, module_path)) {
                modules.emplace_back(module_name);
            }
        }
	}
};
