local test = require("test")
local resources = require("resource_pool")
local Clipboard = require("lstg.Clipboard")
local Keyboard = lstg.Input.Keyboard

---@class test.input.Clipboard : test.Base
local M = {}

function M:onCreate()
    self.text = "..."
    self.timer = 0

    local font_error
    self.font, font_error = resources.loadDynamicFont("sans", {
        pixelWidth = 32,
        pixelHeight = 32,
        sources = { { path = "C:\\Windows\\Fonts\\msyh.ttc", faceIndex = 0 } },
    })
    if not self.font then
        self.font, font_error = resources.loadDynamicFont("sans", {
            pixelWidth = 32,
            pixelHeight = 32,
            sources = { { path = "C:\\Windows\\Fonts\\msyh.ttf", faceIndex = 0 } },
        })
    end
    assert(self.font, font_error)
end

function M:onDestroy()
    resources.pool:remove(self.font)
    self.font = nil
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
    local ctrl_down = Keyboard.GetKeyState(Keyboard.LeftControl) or Keyboard.GetKeyState(Keyboard.RightControl) or Keyboard.GetKeyState(Keyboard.Control)
    if ctrl_down and Keyboard.GetKeyState(Keyboard.V) and has_text then
        local result = Clipboard.getText()
        if result then
            table.insert(buffer, result)
        end
    elseif ctrl_down and Keyboard.GetKeyState(Keyboard.C) then
        Clipboard.setText("Hello world! 你好世界！" .. self.timer)
    end
    table.insert(buffer, "\n")

    self.text = table.concat(buffer)
end

function M:onRender()
    window:applyCameraV()
    local edge = 16
    self.font:drawInRect(self.text, edge, window.width - edge, edge, window.height - edge, {
        color = lstg.Color(255, 0, 0, 0),
        verticalAlign = "bottom",
    })
end

test.registerTest("test.input.Clipboard", M, "Input: Clipboard")
