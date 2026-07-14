#pragma once

#include <string>
#include <string_view>

struct lua_State;

namespace imgui::lua_hot_reload {
	void update(lua_State* vm);
	void shutdown();

	void setEnabled(bool enabled);
	bool isEnabled();

	void setModuleEnabled(std::string_view module_name, bool enabled);
	bool reloadModule(lua_State* vm, std::string_view module_name, std::string* error_message = nullptr);

	void showWindow(lua_State* vm, bool* is_open = nullptr);
}
