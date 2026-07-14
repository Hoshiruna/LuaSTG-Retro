#include "LuaHotReload.hpp"

#include "imgui.h"
#include "lua.hpp"

#include "LuaBinding/LuaCustomLoader.hpp"
#include "core/Configuration.hpp"
#include "core/FileSystem.hpp"
#include "core/FileSystemWatcher.hpp"
#include "core/Logger.hpp"
#include "core/SmartReference.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <algorithm>
#include <ranges>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using std::string_view_literals::operator ""sv;

namespace {
	constexpr auto reload_debounce = std::chrono::milliseconds(150);

	std::filesystem::path toPath(std::string_view const path) {
		return std::filesystem::path(std::u8string_view(reinterpret_cast<char8_t const*>(path.data()), path.size()));
	}

	std::string toString(std::filesystem::path const& path) {
		auto const text = path.generic_u8string();
		return { reinterpret_cast<char const*>(text.data()), text.size() };
	}

	std::string normalizePhysicalPath(std::string_view const path) {
		std::error_code ec;
		auto normalized = std::filesystem::weakly_canonical(toPath(path), ec);
		if (ec) {
			ec.clear();
			normalized = std::filesystem::absolute(toPath(path), ec);
		}
		if (ec) {
			normalized = toPath(path).lexically_normal();
		}
		return toString(normalized.lexically_normal());
	}

	bool isLuaFile(std::string_view const path) {
		auto extension = toPath(path).extension().u8string();
		if (extension.size() != 4) {
			return false;
		}
		for (auto& c : extension) {
			if (c >= u8'A' && c <= u8'Z') {
				c = static_cast<char8_t>(c - u8'A' + u8'a');
			}
		}
		return extension == u8".lua"sv;
	}

	bool getFileSignature(std::string_view const path, uint64_t& signature) {
		core::SmartReference<core::IData> source;
		if (!core::FileSystemManager::readFile(path, source.put())) {
			return false;
		}

		constexpr uint64_t offset_basis = UINT64_C(14695981039346656037);
		constexpr uint64_t prime = UINT64_C(1099511628211);
		auto hash = offset_basis;
		auto const bytes = static_cast<uint8_t const*>(source->data());
		for (size_t i = 0; i < source->size(); ++i) {
			hash ^= bytes[i];
			hash *= prime;
		}
		signature = hash;
		return true;
	}

	std::string getLuaError(lua_State* const vm, int const index) {
		if (auto const message = lua_tostring(vm, index)) {
			return message;
		}
		return "(error object is not a string)";
	}

	int traceback(lua_State* const vm) {
		auto const message = lua_tostring(vm, 1);
		lua_getfield(vm, LUA_GLOBALSINDEX, "debug");
		if (!lua_istable(vm, -1)) {
			lua_pop(vm, 1);
			lua_pushstring(vm, message != nullptr ? message : "(error object is not a string)");
			return 1;
		}
		lua_getfield(vm, -1, "traceback");
		if (!lua_isfunction(vm, -1)) {
			lua_pop(vm, 2);
			lua_pushstring(vm, message != nullptr ? message : "(error object is not a string)");
			return 1;
		}
		lua_pushstring(vm, message != nullptr ? message : "(error object is not a string)");
		lua_pushinteger(vm, 2);
		lua_call(vm, 2, 1);
		return 1;
	}

	int stagedRequire(lua_State* const vm) {
		auto const module_name = luaL_checkstring(vm, 1);
		lua_getfield(vm, LUA_REGISTRYINDEX, "_LOADED");
		lua_pushvalue(vm, 1);
		lua_rawget(vm, -2);
		if (lua_isnil(vm, -1)) {
			return luaL_error(vm, "hot reload cannot load new dependency '%s' during staging", module_name);
		}
		return 1;
	}

	int absoluteIndex(lua_State* const vm, int const index) {
		return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(vm) + index + 1;
	}

	void copyTable(lua_State* const vm, int const source_index, int const destination_index) {
		auto const source = absoluteIndex(vm, source_index);
		auto const destination = absoluteIndex(vm, destination_index);
		lua_pushnil(vm);
		while (lua_next(vm, source) != 0) {
			lua_pushvalue(vm, -2);
			lua_pushvalue(vm, -2);
			lua_rawset(vm, destination);
			lua_pop(vm, 1);
		}
	}

	using TableReferenceMap = std::unordered_map<void const*, int>;
	using OldTableOwnerMap = std::unordered_map<void const*, void const*>;

	void pushMappedValue(lua_State* const vm, int const value_index, TableReferenceMap const& references) {
		auto const value = absoluteIndex(vm, value_index);
		if (lua_istable(vm, value)) {
			if (auto const found = references.find(lua_topointer(vm, value)); found != references.end()) {
				lua_rawgeti(vm, LUA_REGISTRYINDEX, found->second);
				return;
			}
		}
		lua_pushvalue(vm, value);
	}

	void collectTableReferences(lua_State* const vm, int old_index, int new_index, TableReferenceMap& references, OldTableOwnerMap& owners) {
		old_index = absoluteIndex(vm, old_index);
		new_index = absoluteIndex(vm, new_index);

		auto const new_identity = lua_topointer(vm, new_index);
		if (references.contains(new_identity)) {
			return;
		}
		auto const old_identity = lua_topointer(vm, old_index);
		if (auto const found = owners.find(old_identity); found != owners.end() && found->second != new_identity) {
			return;
		}
		owners[old_identity] = new_identity;

		lua_pushvalue(vm, old_index);
		references.emplace(new_identity, luaL_ref(vm, LUA_REGISTRYINDEX));

		if (lua_getmetatable(vm, new_index)) {
			auto const new_metatable = absoluteIndex(vm, -1);
			if (lua_getmetatable(vm, old_index)) {
				auto const old_metatable = absoluteIndex(vm, -1);
				collectTableReferences(vm, old_metatable, new_metatable, references, owners);
				lua_pop(vm, 1);
			}
			lua_pop(vm, 1);
		}

		lua_pushnil(vm);
		while (lua_next(vm, new_index) != 0) {
			auto const key_index = absoluteIndex(vm, -2);
			auto const new_value_index = absoluteIndex(vm, -1);
			pushMappedValue(vm, key_index, references);
			lua_rawget(vm, old_index);
			auto const old_value_index = absoluteIndex(vm, -1);
			if (lua_istable(vm, new_value_index) && lua_istable(vm, old_value_index)) {
				collectTableReferences(vm, old_value_index, new_value_index, references, owners);
			}
			lua_pop(vm, 2);
		}
	}

	void rebindFunctionUpvalues(lua_State* const vm, int function_index, TableReferenceMap const& references, std::unordered_set<void const*>& updated_functions) {
		function_index = absoluteIndex(vm, function_index);
		if (lua_iscfunction(vm, function_index)) {
			return;
		}

		auto const identity = lua_topointer(vm, function_index);
		if (!updated_functions.emplace(identity).second) {
			return;
		}

		for (int upvalue = 1; lua_getupvalue(vm, function_index, upvalue) != nullptr; ++upvalue) {
			if (lua_istable(vm, -1)) {
				auto const found = references.find(lua_topointer(vm, -1));
				if (found != references.end()) {
					lua_pop(vm, 1);
					lua_rawgeti(vm, LUA_REGISTRYINDEX, found->second);
					std::ignore = lua_setupvalue(vm, function_index, upvalue);
					continue;
				}
			}
			lua_pop(vm, 1);
		}
	}

	void rebindTableUpvalues(lua_State* const vm, int table_index, TableReferenceMap const& references, std::unordered_set<void const*>& updated_tables, std::unordered_set<void const*>& updated_functions) {
		table_index = absoluteIndex(vm, table_index);
		auto const identity = lua_topointer(vm, table_index);
		if (!updated_tables.emplace(identity).second) {
			return;
		}

		if (lua_getmetatable(vm, table_index)) {
			rebindTableUpvalues(vm, -1, references, updated_tables, updated_functions);
			lua_pop(vm, 1);
		}

		lua_pushnil(vm);
		while (lua_next(vm, table_index) != 0) {
			if (lua_isfunction(vm, -1)) {
				rebindFunctionUpvalues(vm, -1, references, updated_functions);
			}
			else if (lua_istable(vm, -1)) {
				rebindTableUpvalues(vm, -1, references, updated_tables, updated_functions);
			}
			lua_pop(vm, 1);
		}
	}

	void releaseTableReferences(lua_State* const vm, TableReferenceMap const& references) {
		for (auto const reference : references | std::views::values) {
			luaL_unref(vm, LUA_REGISTRYINDEX, reference);
		}
	}

	bool validateTableKeys(lua_State* const vm, int table_index, std::unordered_set<void const*>& visited) {
		table_index = absoluteIndex(vm, table_index);
		if (!visited.emplace(lua_topointer(vm, table_index)).second) {
			return true;
		}

		if (lua_getmetatable(vm, table_index)) {
			bool const valid = validateTableKeys(vm, -1, visited);
			lua_pop(vm, 1);
			if (!valid) {
				return false;
			}
		}

		lua_pushnil(vm);
		while (lua_next(vm, table_index) != 0) {
			auto const key_type = lua_type(vm, -2);
			if (key_type == LUA_TTABLE || key_type == LUA_TFUNCTION || key_type == LUA_TUSERDATA || key_type == LUA_TTHREAD) {
				lua_pop(vm, 2);
				return false;
			}
			if (lua_istable(vm, -1) && !validateTableKeys(vm, -1, visited)) {
				lua_pop(vm, 2);
				return false;
			}
			lua_pop(vm, 1);
		}
		return true;
	}

	// Derived from rxi/lume's hotswap table updater.
	// Copyright (c) 2020 rxi, MIT License; see data/license/lume/LICENSE.
	// The update is raw and keeps keys absent from the candidate as runtime state.
	void patchTable(lua_State* const vm, int old_index, int new_index, TableReferenceMap const& references, std::unordered_set<void const*>& updated) {
		old_index = absoluteIndex(vm, old_index);
		new_index = absoluteIndex(vm, new_index);

		auto const identity = lua_topointer(vm, old_index);
		if (updated.contains(identity)) {
			return;
		}
		updated.emplace(identity);

		if (lua_getmetatable(vm, new_index)) {
			auto const new_metatable = absoluteIndex(vm, -1);
			if (lua_getmetatable(vm, old_index)) {
				auto const old_metatable = absoluteIndex(vm, -1);
				auto const found = references.find(lua_topointer(vm, new_metatable));
				if (found != references.end()) {
					lua_rawgeti(vm, LUA_REGISTRYINDEX, found->second);
					if (lua_rawequal(vm, old_metatable, -1)) {
						lua_pop(vm, 1);
						patchTable(vm, old_metatable, new_metatable, references, updated);
					}
					else {
						lua_pushvalue(vm, -1);
						lua_setmetatable(vm, old_index);
						lua_pop(vm, 1);
					}
				}
				else {
					lua_pushvalue(vm, new_metatable);
					lua_setmetatable(vm, old_index);
				}
				lua_pop(vm, 1);
			}
			else {
				pushMappedValue(vm, new_metatable, references);
				lua_setmetatable(vm, old_index);
			}
			lua_pop(vm, 1);
		}

		lua_pushnil(vm);
		while (lua_next(vm, new_index) != 0) {
			auto const key_index = absoluteIndex(vm, -2);
			auto const value_index = absoluteIndex(vm, -1);

			pushMappedValue(vm, key_index, references);
			lua_rawget(vm, old_index);
			auto const old_value_index = absoluteIndex(vm, -1);

			if (lua_istable(vm, value_index)) {
				auto const found = references.find(lua_topointer(vm, value_index));
				if (found != references.end()) {
					lua_rawgeti(vm, LUA_REGISTRYINDEX, found->second);
					bool const same_table = lua_istable(vm, old_value_index) && lua_rawequal(vm, old_value_index, -1);
					if (same_table) {
						lua_pop(vm, 1);
						patchTable(vm, old_value_index, value_index, references, updated);
						lua_pop(vm, 1);
					}
					else {
						lua_remove(vm, old_value_index);
						pushMappedValue(vm, key_index, references);
						lua_pushvalue(vm, -2);
						lua_rawset(vm, old_index);
						lua_pop(vm, 1);
					}
				}
				else {
					lua_pop(vm, 1);
					pushMappedValue(vm, key_index, references);
					lua_pushvalue(vm, value_index);
					lua_rawset(vm, old_index);
				}
			}
			else {
				lua_pop(vm, 1);
				pushMappedValue(vm, key_index, references);
				lua_pushvalue(vm, value_index);
				lua_rawset(vm, old_index);
			}

			lua_pop(vm, 1);
		}
	}

	struct WatchRoot {
		std::string path;
		core::SmartReference<core::IMessageQueueBasedFileSystemWatcher> watcher;
		std::string error;
	};

	enum class AttemptState : uint8_t {
		idle,
		applied,
		rejected,
		missing,
		disabled,
	};

	struct ModuleStatus {
		std::string physical_path;
		std::string error;
		uint64_t active_revision{};
		uint64_t attempted_revision{};
		uint64_t automatic_signature{};
		bool has_automatic_signature{};
		bool enabled{ true };
		AttemptState state{ AttemptState::idle };
	};

	struct PendingChange {
		std::string physical_path;
		std::chrono::steady_clock::time_point last_event;
	};

	char const* getAttemptStateName(AttemptState const state) {
		switch (state) {
		case AttemptState::idle:
			return "Initial";
		case AttemptState::applied:
			return "Applied";
		case AttemptState::rejected:
			return "Rejected";
		case AttemptState::missing:
			return "Missing";
		case AttemptState::disabled:
			return "Disabled";
		default:
			return "Unknown";
		}
	}

	class LuaHotReloadManager {
	public:
		void shutdown() {
			m_enabled = false;
			m_watch_roots.clear();
			m_pending.clear();
			m_modules.clear();
			m_revision = 0;
		}

		void setEnabled(bool const enabled) {
			if (m_enabled == enabled) {
				return;
			}
			m_enabled = enabled;
			m_pending.clear();
			m_watch_roots.clear();
			if (m_enabled) {
				openConfiguredWatchRoots();
			}
		}

		bool isEnabled() const noexcept {
			return m_enabled;
		}

		void setModuleEnabled(std::string_view const module_name, bool const enabled) {
			auto& status = m_modules[std::string(module_name)];
			status.enabled = enabled;
			if (!enabled) {
				status.state = AttemptState::disabled;
			}
			else if (status.state == AttemptState::disabled) {
				status.state = AttemptState::idle;
			}
		}

		void update(lua_State* const vm) {
			if (!m_enabled || vm == nullptr) {
				return;
			}

			refreshModules(vm);
			collectChanges();

			auto const now = std::chrono::steady_clock::now();
			for (auto it = m_pending.begin(); it != m_pending.end();) {
				if ((now - it->second.last_event) < reload_debounce) {
					++it;
					continue;
				}

				auto const change = std::move(it->second);
				it = m_pending.erase(it);
				processChange(vm, change);
			}
		}

		bool reloadModule(lua_State* const vm, std::string_view const module_name, std::string* const error_message) {
			refreshModules(vm);
			auto const found = m_modules.find(std::string(module_name));
			if (found == m_modules.end() || found->second.physical_path.empty()) {
				return reject(module_name, "module is not a tracked physical, table-returning Lua module", error_message);
			}

			auto& status = found->second;
			status.attempted_revision = ++m_revision;
			uint64_t current_signature{};
			if (getFileSignature(status.physical_path, current_signature)) {
				status.automatic_signature = current_signature;
				status.has_automatic_signature = true;
			}

			std::string error;
			if (!stageAndCommit(vm, module_name, status.physical_path, error)) {
				status.error = std::move(error);
				status.state = AttemptState::rejected;
				core::Logger::error("[lua-hot-reload] rejected module '{}': {}"sv, module_name, status.error);
				if (error_message != nullptr) {
					*error_message = status.error;
				}
				return false;
			}

			status.active_revision = status.attempted_revision;
			status.error.clear();
			status.state = AttemptState::applied;
			core::Logger::info("[lua-hot-reload] applied module '{}' from '{}'"sv, module_name, status.physical_path);
			if (error_message != nullptr) {
				error_message->clear();
			}
			return true;
		}

		void showWindow(lua_State* const vm, bool* const is_open) {
			refreshModules(vm);
			if (!ImGui::Begin("Lua Hot Reload", is_open)) {
				ImGui::End();
				return;
			}

			bool enabled = m_enabled;
			if (ImGui::Checkbox("Enabled", &enabled)) {
				setEnabled(enabled);
			}
			ImGui::SameLine();
			ImGui::Text("Watched roots: %zu | Pending: %zu", m_watch_roots.size(), m_pending.size());

			if (ImGui::CollapsingHeader("Watch Roots", ImGuiTreeNodeFlags_DefaultOpen)) {
				for (auto const& root : m_watch_roots) {
					ImGui::BulletText("%s", root.path.c_str());
					if (!root.error.empty()) {
						ImGui::SameLine();
						ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", root.error.c_str());
					}
				}
			}

			if (ImGui::BeginTable("##LuaHotReloadModules", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
				ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Module");
				ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Attempt", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableHeadersRow();

				std::vector<std::string_view> names;
				names.reserve(m_modules.size());
				for (auto const& name : m_modules | std::views::keys) {
					names.emplace_back(name);
				}
				std::ranges::sort(names);

				for (auto const name : names) {
					auto& status = m_modules.at(std::string(name));
					ImGui::PushID(name.data());
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					bool module_enabled = status.enabled;
					if (ImGui::Checkbox("##enabled", &module_enabled)) {
						setModuleEnabled(name, module_enabled);
					}

					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(name.data(), name.data() + name.size());
					if (!status.error.empty()) {
						ImGui::TextWrapped("%s", status.error.c_str());
					}

					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(getAttemptStateName(status.state));

					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%llu", static_cast<unsigned long long>(status.active_revision));

					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%llu", static_cast<unsigned long long>(status.attempted_revision));

					ImGui::TableSetColumnIndex(5);
					if (ImGui::SmallButton("Reload")) {
						std::string unused;
						reloadModule(vm, name, &unused);
					}

					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			ImGui::End();
		}

	private:
		void openConfiguredWatchRoots() {
			using Resource = core::ConfigurationLoader::FileSystem::ResourceFileSystem;
			auto const& resources = core::ConfigurationLoader::getInstance().getFileSystem().getResources();
			std::unordered_set<std::string> paths;

			for (auto const& resource : resources) {
				if (resource.getType() != Resource::Type::directory) {
					continue;
				}

				auto path = normalizePhysicalPath(resource.getPath());
				if (!paths.emplace(path).second) {
					continue;
				}

				auto& root = m_watch_roots.emplace_back();
				root.path = std::move(path);
				if (!core::IMessageQueueBasedFileSystemWatcher::create(root.path, root.watcher.put())) {
					root.error = "failed to open watcher";
					core::Logger::error("[lua-hot-reload] failed to watch '{}'"sv, root.path);
				}
			}
		}

		void refreshModules(lua_State* const vm) {
			std::vector<std::pair<std::string, std::string>> modules;
			luastg::lua_get_loaded_module_physical_paths(vm, modules);
			std::unordered_set<std::string> current_modules;
			for (auto& [name, path] : modules) {
				current_modules.emplace(name);
				auto& status = m_modules[name];
				status.physical_path = std::move(path);
			}
			for (auto it = m_modules.begin(); it != m_modules.end();) {
				if (!it->second.physical_path.empty() && !current_modules.contains(it->first)) {
					it = m_modules.erase(it);
				}
				else {
					++it;
				}
			}
		}

		void collectChanges() {
			for (auto& root : m_watch_roots) {
				if (!root.watcher) {
					continue;
				}

				core::FileNotifyInformation info;
				while (root.watcher->next(&info)) {
					if (info.file_name == nullptr) {
						continue;
					}

					std::string_view const relative(info.file_name->c_str(), info.file_name->length());
					if (!isLuaFile(relative)) {
						continue;
					}

					switch (info.action) {
					case core::FileAction::added:
					case core::FileAction::removed:
					case core::FileAction::modified:
					case core::FileAction::renamed_old_name:
					case core::FileAction::renamed_new_name:
						auto physical_path = normalizePhysicalPath(toString(toPath(root.path) / toPath(relative)));
						auto& change = m_pending[physical_path];
						change.physical_path = std::move(physical_path);
						change.last_event = std::chrono::steady_clock::now();
						break;
					default:
						break;
					}
				}
			}
		}

		void processChange(lua_State* const vm, PendingChange const& change) {
			std::vector<std::string> modules;
			luastg::lua_get_loaded_modules_by_physical_path(vm, change.physical_path, modules);
			std::error_code file_error;
			bool const source_exists = std::filesystem::is_regular_file(toPath(change.physical_path), file_error) && !file_error;
			uint64_t signature{};
			bool const has_signature = source_exists && getFileSignature(change.physical_path, signature);
			for (auto const& module_name : modules) {
				auto& status = m_modules[module_name];
				status.physical_path = change.physical_path;
				if (!status.enabled) {
					status.state = AttemptState::disabled;
					continue;
				}
				if (!source_exists) {
					if (status.state == AttemptState::missing) {
						continue;
					}
					status.has_automatic_signature = false;
					status.automatic_signature = 0;
					status.attempted_revision = ++m_revision;
					status.state = AttemptState::missing;
					status.error = "source file was removed; active module retained";
					core::Logger::warn("[lua-hot-reload] source removed for module '{}'; active module retained"sv, module_name);
					continue;
				}
				if (has_signature && status.has_automatic_signature && status.automatic_signature == signature) {
					continue;
				}
				if (has_signature) {
					status.automatic_signature = signature;
					status.has_automatic_signature = true;
				}
				std::string unused;
				reloadModule(vm, module_name, &unused);
			}
		}

		bool stageAndCommit(lua_State* const vm, std::string_view const module_name, std::string_view const physical_path, std::string& error) {
			auto const base = lua_gettop(vm);
			core::SmartReference<core::IData> source;
			if (!core::FileSystemManager::readFile(physical_path, source.put())) {
				error = "failed to read source file";
				return false;
			}

			lua_pushcfunction(vm, &traceback);
			auto const traceback_index = absoluteIndex(vm, -1);

			lua_newtable(vm);
			auto const environment_index = absoluteIndex(vm, -1);
			lua_pushvalue(vm, environment_index);
			lua_setfield(vm, environment_index, "_G");
			lua_pushcfunction(vm, &stagedRequire);
			lua_setfield(vm, environment_index, "require");

			lua_getglobal(vm, "package");
			if (lua_istable(vm, -1)) {
				auto const package_index = absoluteIndex(vm, -1);
				lua_newtable(vm);
				auto const package_copy_index = absoluteIndex(vm, -1);
				copyTable(vm, package_index, package_copy_index);

				lua_getfield(vm, package_index, "loaded");
				if (lua_istable(vm, -1)) {
					lua_newtable(vm);
					copyTable(vm, -2, -1);
					lua_setfield(vm, package_copy_index, "loaded");
				}
				lua_pop(vm, 1);

				lua_pushvalue(vm, package_copy_index);
				lua_setfield(vm, environment_index, "package");
				lua_pop(vm, 1);
			}
			lua_pop(vm, 1);

			lua_newtable(vm);
			lua_pushvalue(vm, LUA_GLOBALSINDEX);
			lua_setfield(vm, -2, "__index");
			lua_setmetatable(vm, environment_index);

			auto const chunk_name = std::string("@") + std::string(physical_path);
			if (luaL_loadbuffer(vm, static_cast<char const*>(source->data()), source->size(), chunk_name.c_str()) != 0) {
				error = getLuaError(vm, -1);
				lua_settop(vm, base);
				return false;
			}

			lua_pushvalue(vm, environment_index);
			lua_setfenv(vm, -2);
			lua_pushlstring(vm, module_name.data(), module_name.size());
			if (lua_pcall(vm, 1, 1, traceback_index) != 0) {
				error = getLuaError(vm, -1);
				lua_settop(vm, base);
				return false;
			}

			auto const candidate_index = absoluteIndex(vm, -1);
			if (!lua_istable(vm, candidate_index)) {
				error = "module must return a table";
				lua_settop(vm, base);
				return false;
			}

			lua_pushnil(vm);
			while (lua_next(vm, environment_index) != 0) {
				bool allowed = false;
				if (lua_type(vm, -2) == LUA_TSTRING) {
					auto const key = std::string_view(lua_tostring(vm, -2));
					allowed = key == "_G"sv || key == "require"sv || key == "package"sv;
				}
				lua_pop(vm, 1);
				if (!allowed) {
					error = "module wrote to a global during staging";
					lua_settop(vm, base);
					return false;
				}
			}

			lua_getfield(vm, LUA_REGISTRYINDEX, "_LOADED");
			auto const loaded_index = absoluteIndex(vm, -1);
			lua_pushlstring(vm, module_name.data(), module_name.size());
			lua_rawget(vm, loaded_index);
			auto const active_index = absoluteIndex(vm, -1);
			if (!lua_istable(vm, active_index)) {
				error = "active package.loaded entry is not a table";
				lua_settop(vm, base);
				return false;
			}

			std::unordered_set<void const*> validated_tables;
			if (!validateTableKeys(vm, candidate_index, validated_tables)) {
				error = "modules with reference-valued table keys are not safely reloadable";
				lua_settop(vm, base);
				return false;
			}

			TableReferenceMap table_references;
			OldTableOwnerMap table_owners;
			collectTableReferences(vm, active_index, candidate_index, table_references, table_owners);
			std::unordered_set<void const*> updated_tables;
			std::unordered_set<void const*> updated_functions;
			rebindTableUpvalues(vm, candidate_index, table_references, updated_tables, updated_functions);

			std::unordered_set<void const*> updated;
			patchTable(vm, active_index, candidate_index, table_references, updated);
			releaseTableReferences(vm, table_references);

			lua_pushnil(vm);
			lua_setfield(vm, environment_index, "require");
			lua_pushnil(vm);
			lua_setfield(vm, environment_index, "package");
			lua_pushvalue(vm, LUA_GLOBALSINDEX);
			lua_setfield(vm, environment_index, "_G");
			if (lua_getmetatable(vm, environment_index)) {
				lua_pushvalue(vm, LUA_GLOBALSINDEX);
				lua_setfield(vm, -2, "__newindex");
				lua_pop(vm, 1);
			}

			lua_settop(vm, base);
			return true;
		}

		bool reject(std::string_view const module_name, std::string message, std::string* const error_message) {
			auto& status = m_modules[std::string(module_name)];
			status.attempted_revision = ++m_revision;
			status.error = std::move(message);
			status.state = AttemptState::rejected;
			core::Logger::error("[lua-hot-reload] rejected module '{}': {}"sv, module_name, status.error);
			if (error_message != nullptr) {
				*error_message = status.error;
			}
			return false;
		}

		bool m_enabled{};
		uint64_t m_revision{};
		std::vector<WatchRoot> m_watch_roots;
		std::unordered_map<std::string, PendingChange> m_pending;
		std::unordered_map<std::string, ModuleStatus> m_modules;
	};

	LuaHotReloadManager& getManager() {
		static LuaHotReloadManager manager;
		return manager;
	}
}

namespace imgui::lua_hot_reload {
	void update(lua_State* const vm) {
		getManager().update(vm);
	}

	void shutdown() {
		getManager().shutdown();
	}

	void setEnabled(bool const enabled) {
		getManager().setEnabled(enabled);
	}

	bool isEnabled() {
		return getManager().isEnabled();
	}

	void setModuleEnabled(std::string_view const module_name, bool const enabled) {
		getManager().setModuleEnabled(module_name, enabled);
	}

	bool reloadModule(lua_State* const vm, std::string_view const module_name, std::string* const error_message) {
		return getManager().reloadModule(vm, module_name, error_message);
	}

	void showWindow(lua_State* const vm, bool* const is_open) {
		getManager().showWindow(vm, is_open);
	}
}
