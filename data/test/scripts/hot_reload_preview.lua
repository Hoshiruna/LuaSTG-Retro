local M = {}
M.revision = "initial"

function M.draw(ImGui)
    M.frame_count = (M.frame_count or 0) + 1
    if ImGui.Begin("Lua Hot Reload Preview") then
        ImGui.Text("Live preview")
        ImGui.Separator()
        ImGui.Text("Revision: " .. M.revision)
        ImGui.Text(string.format("Frames: %d", M.frame_count))
    end
    ImGui.End()
end

return M
