local test = require("test")
local resources = require("resource_pool")

---@class test.Module.HGEFont : test.Base
local M = {}

function M:onCreate()
    resources.loadSpriteFont("hgefont:hgefont", "res/hgefont.fnt", false)
end

function M:onDestroy()
    resources.removeResource("test", 7, "hgefont:hgefont")
end

function M:onUpdate()
end

function M:onRender()
    window:applyCameraV()
    lstg.RenderText("hgefont:hgefont", "114514AABB", window.width / 2, window.height / 2, 1, 0 + 0)
end

test.registerTest("test.Module.HGEFont", M)
