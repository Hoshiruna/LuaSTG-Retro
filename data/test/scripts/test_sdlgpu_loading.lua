posteffect_loading = {}

function posteffect_loading:onCreate()
    self.old_order = lstg.ResourceManager.getLookupOrder()
    self.pool = lstg.ResourceManager.createPool("posteffect-loading")
    lstg.ResourceManager.setLookupOrder({ self.pool })
    self.archive = lstg.LoadPack("res/posteffect-loading.zip")
    if not self.archive then error("Run prepare_posteffect_loading.py to create res/posteffect-loading.zip") end
    self.pool:loadTexture("loading:white", "res/sdlgpu-white.qoi")
    self.pool:createRenderTarget("loading:source", 64, 48, false)
    lstg.SetTextureSamplerState("loading:white", "point+clamp")
    lstg.SetTextureSamplerState("loading:source", "point+clamp")
    self.cases = {
        { name = "loose-sync", path = "res/posteffect_loading/effect.hlsl" },
        { name = "archive-sync", path = "posteffect-archive-only/effect.hlsl" },
        { name = "loose-async", path = "res/posteffect_loading/effect.hlsl", async = true },
        { name = "archive-async", path = "posteffect-archive-only/effect.hlsl", async = true },
        { name = "missing-loose-sync", path = "res/posteffect_loading/missing.hlsl", missing = true },
        { name = "missing-archive-sync", path = "posteffect-archive-only/missing.hlsl", missing = true },
        { name = "missing-loose-async", path = "res/posteffect_loading/missing.hlsl", missing = true, async = true },
        { name = "missing-archive-async", path = "posteffect-archive-only/missing.hlsl", missing = true, async = true },
    }
    for _, case in ipairs(self.cases) do
        case.resource = "loading:" .. case.name
        if not case.missing then
            self.pool:createRenderTarget(case.resource .. ":target", 64, 48, false)
            lstg.SetTextureSamplerState(case.resource .. ":target", "point+clamp")
        end
        if case.async then
            case.job = self.pool:loadFXAsync(case.resource, case.path)
            case.status = "pending"
        else
            case.ok, case.message = pcall(self.pool.loadFX, self.pool, case.resource, case.path)
            self:checkResult(case)
        end
    end
    self.frames, self.capture = 0, true
end

function posteffect_loading:checkResult(case)
    if case.missing then
        case.passed = not case.ok and type(case.message) == "string" and #case.message > 0
            and not self.pool:contains(9, case.resource)
        case.status = case.passed and "PASS: missing include rejected" or "FAIL: missing include was not rejected cleanly"
    else
        case.passed = case.ok and self.pool:contains(9, case.resource)
        case.status = case.passed and "PASS: loaded" or ("FAIL: " .. tostring(case.message))
    end
end

function posteffect_loading:onUpdate()
    self.ready = true
    for _, case in ipairs(self.cases) do
        if case.job and case.job:isDone() then
            case.ok, case.message = case.job:read()
            self:checkResult(case)
            if case.missing and case.job:status() ~= "failed" then
                case.passed, case.status = false, "FAIL: expected failed job status"
            end
            case.job = nil
        end
        if not case.passed then self.ready = false end
    end
    if require("imgui").ImGui.Begin("Shader loading verification (SDL GPU)") then
        for _, case in ipairs(self.cases) do
            require("imgui").ImGui.Text(case.name .. ": " .. case.status)
        end
        require("imgui").ImGui.Text("Expected missing-include errors are written to engine.log.")
        require("imgui").ImGui.Text(self.captured and "Four captures requested. Run compare_posteffect_loading.py." or "Waiting for all eight cases to pass.")
        if require("imgui").ImGui.Button("Capture again") then self.capture = true end
    end
    require("imgui").ImGui.End()
end

function posteffect_loading:onRender()
    if not self.ready then return end
    lstg.PushRenderTarget("loading:source")
    posteffect_fixture.camera(self, 64, 48)
    lstg.Renderer.setVertexColorBlendState(3)
    lstg.Renderer.setBlendState(2)
    posteffect_fixture.quad(self, "loading:white", 0, 0, 32, 24, 0xffff0000)
    posteffect_fixture.quad(self, "loading:white", 32, 0, 32, 24, 0xff00ff00)
    posteffect_fixture.quad(self, "loading:white", 0, 24, 32, 24, 0xff0000ff)
    posteffect_fixture.quad(self, "loading:white", 32, 24, 32, 24, 0xffffffff)
    lstg.PopRenderTarget()
    self.frames = self.frames + 1
    for index, case in ipairs(self.cases) do
        if not case.missing then
            lstg.PushRenderTarget(case.resource .. ":target")
            posteffect_fixture.camera(self, 64, 48)
            lstg.RenderClear(lstg.Color(0))
            lstg.PostEffect("loading:source", case.resource, "one", { tint = lstg.Color(255, 255, 255, 255) })
            lstg.PopRenderTarget()
            if self.capture and self.frames >= 3 then
                lstg.SaveTexture(case.resource .. ":target", "loading-" .. case.name .. ".png")
            end
            posteffect_fixture.camera(self, window.width, window.height)
            lstg.Renderer.setVertexColorBlendState(3)
            lstg.Renderer.setBlendState(2)
            posteffect_fixture.quad(self, case.resource .. ":target", 24 + (index - 1) * 144, 180, 128, 96, 0xffffffff)
        end
    end
    if self.capture and self.frames >= 3 then self.capture, self.captured = false, true end
end

function posteffect_loading:onDestroy()
    for _, case in ipairs(self.cases or {}) do
        if case.job then case.job:cancel() end
    end
    if self.old_order then lstg.ResourceManager.setLookupOrder(self.old_order) end
    if self.pool and self.pool:isValid() then lstg.ResourceManager.destroyPool(self.pool) end
    if self.archive then lstg.UnloadPack("res/posteffect-loading.zip") end
end

require("test").registerTest("test.graphics.PostEffectLoading", posteffect_loading, "Graphics: shader archive / async loading (SDL GPU)")
