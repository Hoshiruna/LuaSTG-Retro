local test = require("test")
local resources = require("resource_pool")

---@class test.audio.BadWav : test.Base
local M = {}

function M:onCreate()
    pcall(function()
        resources.loadSound("se:ok00", "assets/se/se_ok00.wav")
    end)
    resources.loadSound("se:ok00", "assets/se/se_ok00_fixed.wav")
end

function M:onDestroy()
    resources.removeResource("test", 5, "se:ok00")
end

function M:onUpdate()
end

function M:onRender()
end

test.registerTest("test.audio.BadWav", M, "Audio: Bad WAV")
