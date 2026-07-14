#pragma once
#include "lua.hpp"
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace luastg
{
	void lua_register_custom_loader(lua_State* L);

	bool lua_get_loaded_module_physical_path(lua_State* L, std::string_view module_name, std::string& path);
	void lua_get_loaded_modules_by_physical_path(lua_State* L, std::string_view path, std::vector<std::string>& modules);
	void lua_get_loaded_module_physical_paths(lua_State* L, std::vector<std::pair<std::string, std::string>>& modules);
};
