local Display = require("lstg.Display")
local Window = require("lstg.Window")
local FrameStyle = require("lstg.Window.FrameStyle")
local SwapChain = require("lstg.SwapChain")
local ScalingMode = require("lstg.SwapChain.ScalingMode")
local test = require("test")
local resources = require("resource_pool")

---@class test.Module.WindowAndDisplay : test.Base
local M = {}

function M:onCreate()
    local font, font_error = resources.loadDynamicFont("body", {
        pixelWidth = 0,
        pixelHeight = 24,
        sources = { { path = "C:/Windows/Fonts/msyh.ttc", faceIndex = 0 } },
    })
    self.body_font = assert(font, font_error)
    self.main_window = Window.getMain()
    self.main_swap_chain = SwapChain.getMain()
end

function M:onDestroy()
    resources.pool:remove(self.body_font)
    self.body_font = nil
end

function M:onUpdate()
    local displays = Display.getAll()
    local Keyboard = lstg.Input.Keyboard
    local Key = lstg.Input.Key
    if Keyboard.wasPressed(Key.a) then
        self.main_swap_chain:setSize(window.width, window.height)
    elseif Keyboard.wasPressed(Key.s) then
        self.main_swap_chain:setSize(1280, 720)
    elseif Keyboard.wasPressed(Key.d) then
        self.main_swap_chain:setSize(640, 360)
    elseif Keyboard.wasPressed(Key.q) then
        self.main_window:setWindowed(window.width, window.height, FrameStyle.borderless)
    elseif Keyboard.wasPressed(Key.w) then
        self.main_window:setWindowed(window.width, window.height, FrameStyle.fixed)
    elseif Keyboard.wasPressed(Key.e) then
        self.main_window:setWindowed(window.width, window.height, FrameStyle.normal)
    elseif Keyboard.wasPressed(Key.digit1) then
        --self.main_window:setWindowed(window.width, window.height, FrameStyle.normal, displays[1])
        self.main_window:setFullscreen(displays[1])
    elseif Keyboard.wasPressed(Key.digit2) then
        if displays[2] then
            --self.main_window:setWindowed(window.width, window.height, FrameStyle.normal, displays[2])
            self.main_window:setFullscreen(displays[2])
        end
    elseif Keyboard.wasPressed(Key.n) then
        self.main_window:setCursorVisibility(true)
    elseif Keyboard.wasPressed(Key.m) then
        self.main_window:setCursorVisibility(false)
    elseif Keyboard.wasPressed(Key.k) then
        self.main_swap_chain:setScalingMode(ScalingMode.stretch)
    elseif Keyboard.wasPressed(Key.l) then
        self.main_swap_chain:setScalingMode(ScalingMode.aspect_ratio)
    elseif Keyboard.wasPressed(Key.v) then
        self.main_swap_chain:setVSyncPreference(not self.main_swap_chain:getVSyncPreference())
    end
end

function M:onRender()
    window:applyCameraV()
    local message = ""
    local function info(fmt, ...)
        message = message .. string.format(fmt, ...) .. "\n"
    end
    info("main window:")
    local logical_size = self.main_window:getSize()
    local pixel_size = self.main_window:getPixelSize()
    info("    logical size: [%d x %d]", logical_size.width, logical_size.height)
    info("    pixel size: [%d x %d]", pixel_size.width, pixel_size.height)
    info("    display scale: %.2f", self.main_window:getDisplayScale())
    info("    cursor visibility: %s", tostring(self.main_window:getCursorVisibility()))
    info("main swap chain:")
    info("    vsync: %s", tostring(self.main_swap_chain:getVSyncPreference()))
    local list = Display.getAll()
    for i, display in ipairs(list) do
        info("display %d:", i)
        info("    friendly name: %s", display:getFriendlyName())
        local sz1 = display:getSize()
        local pos1 = display:getPosition()
        local rc1 = display:getRect()
        info("    size: [%d x %d]", sz1.width, sz1.height)
        info("    position: (%d, %d)", pos1.x, pos1.y)
        info("    rect: [%d, %d, %d, %d] (%d, %d)", rc1.left, rc1.top, rc1.right, rc1.bottom, rc1.right - rc1.left, rc1.bottom - rc1.top)
        local sz2 = display:getWorkAreaSize()
        local pos2 = display:getWorkAreaPosition()
        local rc2 = display:getWorkAreaRect()
        info("    work area size: [%d x %d]", sz2.width, sz2.height)
        info("    work area position: (%d, %d)", pos2.x, pos2.y)
        info("    work area rect: [%d, %d, %d, %d] (%d, %d)", rc2.left, rc2.top, rc2.right, rc2.bottom, rc2.right - rc2.left, rc2.bottom - rc2.top)
        info("    display scale: %.2f", display:getDisplayScale())
        info("    primary: %s", tostring(display:isPrimary()))
    end
    self.body_font:draw(message, 0, window.height, {
        color = lstg.Color(255, 0, 0, 0),
    })
end

test.registerTest("test.Module.WindowAndDisplay", M)
