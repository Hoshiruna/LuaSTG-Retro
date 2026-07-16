local test = require("test")
local resources = require("resource_pool")

---@class test.Module.Fancy2DFont : test.Base
local M = {}

function M:onCreate()
    resources.loadSpriteFont("f2dfont:f2dfont", "res/f2dfont.xml", "res/f2dfont.png", false)
end

function M:onDestroy()
    resources.removeResource("test", 7, "f2dfont:f2dfont")
end

function M:onUpdate()
end

function M:onRender()
    window:applyCameraV()
    lstg.RenderText("f2dfont:f2dfont", "114514AABB", window.width / 2, window.height / 2, 1, 0 + 0)
end

test.registerTest("test.Module.Fancy2DFont", M)
