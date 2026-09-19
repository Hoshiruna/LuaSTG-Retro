local test = require("test")
local resources = require("resource_pool")

---@class test.Module.HGEFont : test.Base
local M = {}

function M:onCreate()
    resources.loadSpriteFont("hgefont:hgefont", "res/hgefont.fnt", false)
    self.frames = 0
    self.capture = true
end

function M:onDestroy()
    resources.removeResource("test", 7, "hgefont:hgefont")
end

function M:onUpdate()
    if require("imgui").ImGui.Begin("HGE font check") then
        require("imgui").ImGui.Text("Expect white text at the center and near the bottom.")
        require("imgui").ImGui.Text("hgefont-capture.png excludes the ImGui windows.")
        if require("imgui").ImGui.Button("Capture again") then
            self.capture = true
        end
    end
    require("imgui").ImGui.End()
end

function M:onRender()
    window:applyCameraV()
    lstg.RenderText("hgefont:hgefont", "114514AABB", window.width / 2, window.height / 2, 1, 0 + 0)
    lstg.RenderText("hgefont:hgefont", "HGE FONT 0123456789", window.width / 2, window.height * 0.2, 3, 0)
    lstg.RenderText("hgefont:hgefont", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", window.width / 2, window.height * 0.1, 2, 0)
    self.frames = self.frames + 1
    if self.capture and self.frames >= 3 then
        lstg.Snapshot("hgefont-capture.png")
        self.capture = false
    end
end

test.registerTest("test.Module.HGEFont", M)
