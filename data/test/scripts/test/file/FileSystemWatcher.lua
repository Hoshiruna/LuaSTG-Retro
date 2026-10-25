local test = require("test")
local imgui = require("imgui")
local FileSystemWatcher = require("lstg.FileSystemWatcher")
local FileAction = require("lstg.FileSystemWatcher.FileAction")

local TEST_NAME = "File: FileSystemWatcher"

---@class test.file.FileSystemWatcher : test.Base
local M = {}

function M:onCreate()
    local home = os.getenv("USERPROFILE") or "."
    self.watcher = FileSystemWatcher.create(home .. "\\Downloads");
    self.changes = {}
end

function M:onDestroy()
    self.watcher:close()
end

local function translateFileAction(action)
    if FileAction.added == action then return "Added" end
    if FileAction.removed == action then return "Removed" end
    if FileAction.modified == action then return "Modified" end
    if FileAction.renamed_old_name == action then return "Renamed (old name)" end
    if FileAction.renamed_new_name == action then return "Renamed (new name)" end
    return "Unknown"
end

function M:onUpdate()
    local change = { file_name = "", action = 0 }
    while self.watcher:read(change) do
        table.insert(self.changes, { file_name = change.file_name, action = change.action })
    end

    ---@diagnostic disable-next-line: undefined-field
    local ImGui = imgui.ImGui
    if ImGui.Begin(TEST_NAME) then
        for i, v in ipairs(self.changes) do
            ImGui.Text(string.format("%d. %s %s", i, translateFileAction(v.action), v.file_name))
        end
    end
    ImGui.End()
end

function M:onRender()
end

test.registerTest("test.file.FileSystemWatcher", M, TEST_NAME)
