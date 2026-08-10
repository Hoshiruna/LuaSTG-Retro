local test = require("test")
local resources = require("resource_pool")
local imgui = require("imgui")

local SE_NAME = "啊！"

---@class test.audio.SoundEffect : test.Base
local M = {}

function M:onCreate()
    resources.loadSound(SE_NAME, "res/audio/啊！.wav")
    self.vol = 1.0
    self.pan = 0.0
end

function M:onDestroy()
    if self.voice then
        lstg.Audio.stop(self.voice)
    end
    resources.removeResource("test", 5, SE_NAME)
end

function M:onUpdate()
    ---@diagnostic disable-next-line: undefined-field
    local ImGui = imgui.ImGui
    if ImGui.Begin("Audio: Sound Effect") then
        local state = self.voice and lstg.Audio.getState(self.voice) or "stopped"
        ImGui.Text(("SE State: %s"):format(state))
        _, self.vol = ImGui.SliderFloat("Volume", self.vol, 0.0, 1.0)
        _, self.pan = ImGui.SliderFloat("Pan", self.pan, -1.0, 1.0)
        if ImGui.Button("Play") then
            self.voice = lstg.Audio.playSound(SE_NAME, { volume = self.vol, pan = self.pan })
        end
        if ImGui.Button("Pause") then
            if self.voice then lstg.Audio.pause(self.voice) end
        end
        if ImGui.Button("Resume") then
            if self.voice then lstg.Audio.resume(self.voice) end
        end
        if ImGui.Button("Stop") then
            if self.voice then lstg.Audio.stop(self.voice) end
        end
    end
    ImGui.End()
end

function M:onRender()
end

test.registerTest("test.audio.SoundEffect", M, "Audio: Sound Effect")
