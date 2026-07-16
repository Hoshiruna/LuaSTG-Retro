local test = require("test")
local resources = require("resource_pool")

---@class test.Module.RenderTarget : test.Base
local M = {}

function M:onCreate()
    resources.createRenderTarget("rt:test1")
    resources.createRenderTarget("rt:test2")
    self.press_key = false
end

function M:onDestroy()
    resources.removeResource("test", 1, "rt:test1")
    resources.removeResource("test", 1, "rt:test2")
end

function M:onUpdate()
    local Keyboard = lstg.Input.Keyboard
    if not self.press_key then
        if Keyboard.GetKeyState(Keyboard.D1) then
            window:setSize(1280, 720)
            self.press_key = true
        elseif Keyboard.GetKeyState(Keyboard.D2) then
            window:setSize(1920, 1080)
            self.press_key = true
        end
    else
        if not Keyboard.GetKeyState(Keyboard.D1) and not Keyboard.GetKeyState(Keyboard.D2) then
            self.press_key = false
        end
    end
end

function M:onRender()
end

test.registerTest("test.Module.RenderTarget", M)
