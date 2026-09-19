posteffect_fixture = {}

function posteffect_fixture:camera(width, height)
    lstg.SetViewport(0, width, 0, height)
    lstg.SetScissorRect(0, width, 0, height)
    lstg.SetOrtho(0, width, height, 0)
    lstg.SetFog()
    lstg.SetZBufferEnable(0)
end

function posteffect_fixture:quad(texture, x, y, width, height, color)
    lstg.Renderer.setTexture(texture)
    lstg.Renderer.drawQuad(
        { x, y, 0.5, 0, 0, color }, { x + width, y, 0.5, 1, 0, color },
        { x + width, y + height, 0.5, 1, 1, color }, { x, y + height, 0.5, 0, 1, color })
end

function posteffect_fixture:onCreate()
    self.frames, self.capture = 0, true
    self.old_width, self.old_height = window.width, window.height
    window.width, window.height = 640, 480
    require("lstg.SwapChain").getMain():setSize(640, 480)
    require("lstg.SwapChain").getMain():setScalingMode(require("lstg.SwapChain.ScalingMode").aspect_ratio)
    require("lstg.Window").getMain():setSize(960, 640)
    for _, name in ipairs({ "white", "straight" }) do
        require("resource_pool").loadTexture("fx:" .. name, "res/sdlgpu-" .. name .. ".qoi")
        lstg.SetTextureSamplerState("fx:" .. name, "point+clamp")
    end
    for _, name in ipairs({ "source", "tile", "chain" }) do
        require("resource_pool").createRenderTarget("fx:" .. name, 64, 48, name == "tile")
        lstg.SetTextureSamplerState("fx:" .. name, "point+clamp")
    end
    require("resource_pool").loadFX("fx:legacy", "res/posteffect/legacy.hlsl")
    require("resource_pool").loadFX("fx:named", "res/posteffect/named.hlsl")
    self.shader = lstg.CreatePostEffectShader("res/posteffect/sparse.hlsl")
    self.shader:setTexture("source_texture", "fx:source")
    self.shader:setTexture("mask_texture", "fx:white")
    self.shader:setFloat("gain", 1)
    self.shader:setFloat2("shift", 0, 0)
    self.shader:setFloat3("tint", 0.2126, 0.7152, 0.0722)
    self.shader:setFloat4("color", 1, 1, 1, 1)
    self.blends = { "mul+alpha", "mul+add", "mul+sub", "mul+rev", "mul+min",
        "mul+max", "mul+mul", "mul+screen", "one", "alpha+bal" }
end

function posteffect_fixture:onDestroy()
    self.shader = nil
    for _, name in ipairs({ "white", "straight", "source", "tile", "chain" }) do
        require("resource_pool").pool:removeByName(1, "fx:" .. name)
    end
    require("resource_pool").pool:removeByName(9, "fx:legacy")
    require("resource_pool").pool:removeByName(9, "fx:named")
    window:setSize(self.old_width, self.old_height)
end

function posteffect_fixture:onUpdate()
    if require("imgui").ImGui.Begin("Post-effect comparison") then
        require("imgui").ImGui.Text("Row 1: eight samplers. Row 2: calling styles and chaining.")
        require("imgui").ImGui.Text("Rows 3-4: blending. Bottom: original and grayscale markers.")
        if require("imgui").ImGui.Button("Capture again") then self.capture = true end
    end
    require("imgui").ImGui.End()
end

function posteffect_fixture:beginTile()
    lstg.PushRenderTarget("fx:tile")
    self:camera(64, 48)
    lstg.RenderClear(lstg.Color(255, 32, 48, 64))
    lstg.ClearZBuffer(1)
end

function posteffect_fixture:endTile(x, y)
    lstg.PopRenderTarget()
    self:camera(640, 480)
    lstg.Renderer.setVertexColorBlendState(3)
    lstg.Renderer.setBlendState(2)
    self:quad("fx:tile", x, y, 64, 48, 0xffffffff)
end

function posteffect_fixture:named(source, amount)
    lstg.PostEffect(source, "fx:named", "one", {
        channel_factor = lstg.Color(255, 54, 183, 18), amount = amount or 0, extra_texture = "fx:straight",
    })
end

function posteffect_fixture:onRender()
    self:camera(640, 480)
    lstg.RenderClear(lstg.Color(255, 32, 48, 64))
    lstg.PushRenderTarget("fx:source")
    self:camera(64, 48)
    lstg.Renderer.setVertexColorBlendState(3)
    lstg.Renderer.setBlendState(2)
    self:quad("fx:white", 0, 0, 32, 24, 0xffff0000)
    self:quad("fx:white", 32, 0, 32, 24, 0xff00ff00)
    self:quad("fx:white", 0, 24, 32, 24, 0xff0000ff)
    self:quad("fx:white", 32, 24, 32, 24, 0xffffffff)
    lstg.PopRenderTarget()

    for sampler = 0, 7 do
        self:beginTile()
        lstg.PostEffect("fx:legacy", "fx:source", sampler, "one",
            { { 1, 1, 1, 1 }, { 1.5, 1.5, -0.25, -0.25 }, { 0, 0, 0, 0 } }, { { "fx:white", 1 } })
        self:endTile(16 + sampler * 72, 16)
    end

    for variant = 0, 7 do
        self:beginTile()
        if variant == 0 then
            self:named("fx:source")
        elseif variant == 1 then
            lstg.PostEffect("fx:legacy", "fx:source", 1, "one",
                { { 0.5, 0.5, 0.5, 0.5 }, { 1, 1, 0, 0 }, { 0, 0, 0, 0 } }, { { "fx:white", 1 } })
        elseif variant == 2 then
            lstg.PostEffect(self.shader, "one")
        elseif variant == 3 then
            self:named("fx:source", 0.25)
        elseif variant == 4 then
            lstg.PushRenderTarget("fx:chain")
            self:camera(64, 48)
            self:named("fx:source")
            lstg.PopRenderTarget()
            self:camera(64, 48)
            self:named("fx:chain")
        elseif variant == 5 then
            lstg.Renderer.setViewport(8, 8, 56, 40)
            self:named("fx:source")
        elseif variant == 6 then
            self.shader:setFloat("gain", 0.5)
            lstg.PostEffect(self.shader, "one")
            self.shader:setFloat("gain", 1)
        else
            lstg.Renderer.setViewport(8, 8, 56, 40)
            lstg.Renderer.setScissorRect(16, 12, 48, 36)
            lstg.Renderer.setVertexColorBlendState(3)
            lstg.Renderer.setBlendState(2)
            self:quad("fx:white", 0, 0, 64, 48, 0xffff00ff)
            lstg.PostEffect(self.shader, "one")
            -- No state reset: the following sprite must retain the viewport and scissor.
            self:quad("fx:white", 0, 0, 64, 48, 0xff00ffff)
        end
        self:endTile(16 + variant * 72, 80)
    end

    for index, blend in ipairs(self.blends) do
        self:beginTile()
        lstg.PostEffect("fx:legacy", "fx:source", 1, blend,
            { { 0.25, 0.25, 0.25, 0.5 }, { 1, 1, 0, 0 }, { 0.125, 0.125, 0.125, 0 } }, { { "fx:straight", 1 } })
        self:endTile(16 + ((index - 1) % 8) * 72, 160 + math.floor((index - 1) / 8) * 64)
    end

    self:camera(640, 480)
    lstg.Renderer.setBlendState(2)
    self:quad("fx:source", 16, 352, 64, 48, 0xffffffff)
    self:beginTile()
    self:named("fx:source")
    self:endTile(88, 352)
    self.frames = self.frames + 1
    if self.capture and self.frames >= 3 then
        lstg.Snapshot("posteffect-capture.png")
        lstg.SaveTexture("fx:tile", "posteffect-target.png")
        self.capture = false
    end
end

require("test").registerTest("test.graphics.PostEffect", posteffect_fixture, "Graphics: deterministic post-effects")
