---@diagnostic disable: duplicate-set-field, missing-return, unused-local

---@class imgui
local imgui = {}

---@class imgui.ImVec2
---@field x number
---@field y number

---@return imgui.ImVec2
function imgui.ImVec2()
end

---@param x number
---@param y number
---@return imgui.ImVec2
function imgui.ImVec2(x, y)
end

---@class imgui.ImVec4
---@field x number
---@field y number
---@field z number
---@field w number

---@return imgui.ImVec4
function imgui.ImVec4()
end

---@param x number
---@param y number
---@param z number
---@param w number
---@return imgui.ImVec4
function imgui.ImVec4(x, y, z, w)
end

---@class imgui.backend
imgui.backend = {}

---@param enabled boolean
function imgui.backend.SetLuaHotReloadEnabled(enabled)
end

---@return boolean
function imgui.backend.IsLuaHotReloadEnabled()
end

---@param module_name string
---@param enabled boolean
---Only physical, `require`-loaded modules that return tables are reloadable.
function imgui.backend.SetLuaHotReloadModuleEnabled(module_name, enabled)
end

---@param module_name string
---@return boolean success
---@return string? error_message
---Reloadable modules should avoid engine mutations and shared-table writes at module top level.
function imgui.backend.ReloadLuaModule(module_name)
end

---@param open boolean?
---@return boolean open
function imgui.backend.ShowLuaHotReloadWindow(open)
end

return imgui
