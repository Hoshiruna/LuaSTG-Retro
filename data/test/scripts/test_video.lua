local test = require("test")
local resources = require("resource_pool")

---@class test.Module.Video : test.Base
local M = {}

local video_name = "video:test"
local image_name = "video:test:image"
local video_path = "res/video/test.mp4"

function M:onCreate()
    if not lstg.FileManager.FileExist(video_path, true) then
        lstg.Print("skip video test: missing " .. video_path)
        self.skipped = true
        return
    end

    resources.loadVideo(video_name, video_path, false)
    local w, h = lstg.GetTextureSize(video_name)
    assert(w > 0 and h > 0)
    assert(lstg.GetVideoDuration(video_name) >= 0)
    assert(lstg.GetVideoState(video_name) == "stopped")

    resources.createSprite(image_name, video_name, 0, 0, w, h)
    lstg.PlayVideo(video_name)
    assert(lstg.GetVideoState(video_name) == "playing")
    lstg.PauseVideo(video_name)
    assert(lstg.GetVideoState(video_name) == "paused")
    lstg.ResumeVideo(video_name)
    assert(lstg.GetVideoState(video_name) == "playing")
    lstg.SeekVideo(video_name, 0)
    assert(lstg.GetVideoTime(video_name) == 0)

end

function M:onDestroy()
    if self.skipped then
        return
    end
    resources.removeResource("test", 2, image_name)
    resources.removeResource("test", 1, video_name)
end

function M:onUpdate()
end

function M:onRender()
    if self.skipped then
        return
    end
    local window = require("lstg.Window")
    lstg.Render(image_name, window.width / 2, window.height / 2, 0, 1)

    local w, h = lstg.GetTextureSize(video_name)
    lstg.RenderTexture(video_name, "mul+alpha",
        { 0, 0, 0, 0, 0, lstg.Color(255, 255, 255, 255) },
        { w, 0, 0, w, 0, lstg.Color(255, 255, 255, 255) },
        { w, h, 0, w, h, lstg.Color(255, 255, 255, 255) },
        { 0, h, 0, 0, h, lstg.Color(255, 255, 255, 255) })
end

test.registerTest("test.Module.Video", M)
