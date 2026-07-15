local test = require("test")
local resources = require("resource_pool")

---@class test.Module.TrueTypeFont : test.Base
local M = {}

function M:onCreate()
    resources.loadTTF("ttf:test1", "res/model/syst_heavy.otf", 0, 26)
    lstg.CacheTTFString("ttf:test1", "你好朋友")
    self.press_key = false
end

function M:onDestroy()
    resources.removeResource("test", 8, "ttf:test1")
end

function M:onUpdate()
end

function M:onRender()
    window:applyCameraV()
    local x, y = window.width / 2, window.height / 2
    lstg.RenderTTF("ttf:test1", "你好朋友", x, x, y, y, 0 + 0, lstg.Color(255, 255, 255, 255), 1)
end

test.registerTest("test.Module.TrueTypeFont", M)
