local test = require("test")

local Input = lstg.Input
local Keyboard = Input.Keyboard
local Mouse = Input.Mouse
local Key = Input.Key
local MouseButton = Input.MouseButton

local TEST_NAME = "Input: SDL3"

---@class test.input.SDL3 : test.Base
local M = {}

function M:onCreate()
    assert(type(Key.a) == "number")
    assert(type(MouseButton.left) == "number")
    assert(type(Input.GamepadButton.south) == "number")
    assert(type(Input.GamepadAxis.left_x) == "number")
    assert(type(Input.JoystickHat.centered) == "number")
end

function M:onUpdate()
    local ImGui = require("imgui").ImGui
    if ImGui.Begin(TEST_NAME) then
        local x, y = Mouse.getPosition()
        local canvas_x, canvas_y = Mouse.getCanvasPosition()
        local dx, dy = Mouse.getMovement()
        local wheel_x, wheel_y = Mouse.getWheel()
        ImGui.Text(string.format("Window: %.2f, %.2f", x, y))
        ImGui.Text(string.format("Canvas: %.2f, %.2f", canvas_x, canvas_y))
        ImGui.Text(string.format("Movement: %.2f, %.2f", dx, dy))
        ImGui.Text(string.format("Wheel: %.2f, %.2f", wheel_x, wheel_y))
        ImGui.Text(string.format("Space: down=%s pressed=%s released=%s",
            tostring(Keyboard.isDown(Key.space)),
            tostring(Keyboard.wasPressed(Key.space)),
            tostring(Keyboard.wasReleased(Key.space))))

        for index, gamepad in ipairs(Input.Gamepad.getAll()) do
            ImGui.Text(string.format("Gamepad %d: %s (player %d)", index, gamepad:getName(), gamepad:getPlayerIndex()))
        end
        for index, joystick in ipairs(Input.Joystick.getAll()) do
            ImGui.Text(string.format("Joystick %d: %s (%d axes, %d buttons, %d hats)",
                index,
                joystick:getName(),
                joystick:getAxisCount(),
                joystick:getButtonCount(),
                joystick:getHatCount()))
        end
    end
    ImGui.End()
end

function M:onDestroy()
end

function M:onRender()
end

test.registerTest("test.input.SDL3", M, TEST_NAME)

