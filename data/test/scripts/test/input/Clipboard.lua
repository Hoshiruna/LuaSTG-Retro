local test = require("test")
local resources = require("resource_pool")
local Clipboard = require("lstg.Clipboard")
local Keyboard = lstg.Input.Keyboard
local Key = lstg.Input.Key

---@class test.input.Clipboard : test.Base
local M = {}

function M:onCreate()
    self.text = "..."
    self.timer = 0

    if not resources.loadTTF("sans", "C:\\Windows\\Fonts\\msyh.ttc", 32, 32) then
        resources.loadTTF("sans", "C:\\Windows\\Fonts\\msyh.ttf", 32, 32)
    end
end

function M:onDestroy()
    resources.removeResource("test", 8, "sans")
end

function M:onUpdate()
    self.timer = self.timer + 1
    local buffer = {}

    table.insert(buffer, "has text: ")
    local has_text = Clipboard.hasText()
    if has_text then
        table.insert(buffer, "true")
    else
        table.insert(buffer, "false")
    end
    table.insert(buffer, "\n")

    table.insert(buffer, "text: ")
    local ctrl_down = Keyboard.isDown(Key.left_control) or Keyboard.isDown(Key.right_control)
    if ctrl_down and Keyboard.isDown(Key.v) and has_text then
        local result = Clipboard.getText()
        if result then
            table.insert(buffer, result)
        end
    elseif ctrl_down and Keyboard.isDown(Key.c) then
        Clipboard.setText("Hello world! 你好世界！" .. self.timer)
    end
    table.insert(buffer, "\n")

    self.text = table.concat(buffer)
end

function M:onRender()
    window:applyCameraV()
    local edge = 16
    lstg.RenderTTF("sans", self.text, edge, window.width - edge, edge, window.height - edge, 0 + 8, lstg.Color(255, 0, 0, 0), 2)
end

test.registerTest("test.input.Clipboard", M, "Input: Clipboard")
