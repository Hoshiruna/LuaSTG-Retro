local test = require("test")
local resources = require("resource_pool")

local function load_texture(f)
    resources.loadTexture("tex:" .. f, "res/" .. f, false)
    local w, h = lstg.GetTextureSize("tex:" .. f)
    resources.createSprite("img:" .. f, "tex:" .. f, 0, 0, w, h)
end
local function unload_texture(f)
    resources.removeResource("test", 2, "img:" .. f)
    resources.removeResource("test", 1, "tex:" .. f)
end

---@class test.Module.Texture : test.Base
local M = {}

function M:onCreate()
    load_texture("sRGB.png")
    load_texture("linear.png")
    load_texture("block.png")
    load_texture("block.qoi")

end

function M:onDestroy()
    unload_texture("sRGB.png")
    unload_texture("linear.png")
    unload_texture("block.png")
    unload_texture("block.qoi")
end

function M:onUpdate()
end

function M:onRender()
    window:applyCameraV()
    local scale = 0.5
    lstg.Render("img:sRGB.png", window.width / 4 * 1, window.height / 2, 0, scale)
    lstg.Render("img:linear.png", window.width / 4 * 3, window.height / 2, 0, scale)
    scale = 0.5
    lstg.Render("img:block.png", window.width / 4 * 2, window.height / 4 * 1, 0, scale)
    lstg.Render("img:block.qoi", window.width / 4 * 2, window.height / 4 * 3, 0, scale)
end

test.registerTest("test.Module.Texture", M)
