local test = require("test")
local resources = require("resource_pool")

---@class test.Module.RenderTarget : test.Base
local M = {}

function M:onCreate()
    resources.createRenderTarget("rt:test1")
    resources.createRenderTarget("rt:test2")
end

function M:onDestroy()
    resources.removeResource("test", 1, "rt:test1")
    resources.removeResource("test", 1, "rt:test2")
end

function M:onUpdate()
    local Keyboard = lstg.Input.Keyboard
    local Key = lstg.Input.Key
    if Keyboard.wasPressed(Key.digit1) then
        window:setSize(1280, 720)
    elseif Keyboard.wasPressed(Key.digit2) then
        window:setSize(1920, 1080)
    end
end

function M:onRender()
end

test.registerTest("test.Module.RenderTarget", M)
