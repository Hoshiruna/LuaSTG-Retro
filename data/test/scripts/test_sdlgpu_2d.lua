-- Fixed coordinates and colors are shared with tools/compare_2d.py.
sdlgpu_2d_fixture = {}

function sdlgpu_2d_fixture:camera(width, height)
    lstg.SetViewport(0, width, 0, height)
    lstg.SetScissorRect(0, width, 0, height)
    lstg.SetOrtho(0, width, height, 0)
    lstg.SetFog()
    lstg.SetZBufferEnable(0)
end

function sdlgpu_2d_fixture:quad(texture, x, y, width, height, color, depth, second_color)
    lstg.Renderer.setTexture(texture)
    lstg.Renderer.drawQuad(
        { x, y, depth or 0.5, 0, 0, color },
        { x + width, y, depth or 0.5, 1, 0, second_color or color },
        { x + width, y + height, depth or 0.5, 1, 1, second_color or color },
        { x, y + height, depth or 0.5, 0, 1, color })
end

function sdlgpu_2d_fixture:onCreate()
    self.frames = 0
    self.capture = true
    self.old_width, self.old_height = window.width, window.height
    window.width, window.height = 640, 480
    require("lstg.SwapChain").getMain():setSize(640, 480)
    require("lstg.SwapChain").getMain():setScalingMode(require("lstg.SwapChain.ScalingMode").aspect_ratio)
    require("lstg.Window").getMain():setSize(960, 640)
    for _, name in ipairs({ "markers", "white", "straight", "premul" }) do
        require("resource_pool").loadTexture("2d:" .. name, "res/sdlgpu-" .. name .. ".qoi")
        lstg.SetTextureSamplerState("2d:" .. name, "point+clamp")
    end
    lstg.SetTexturePreMulAlphaState("2d:premul", true)
    require("resource_pool").createRenderTarget("2d:outer", 64, 48, true)
    require("resource_pool").createRenderTarget("2d:inner", 64, 48, false)
    lstg.SetTextureSamplerState("2d:outer", "point+clamp")
    lstg.SetTextureSamplerState("2d:inner", "point+clamp")
    self.font, self.font_error = require("resource_pool").loadDynamicFont("2d:font", {
        pixelHeight = 20,
        sources = { { path = (os.getenv("WINDIR") or "C:/Windows") .. "/Fonts/segoeui.ttf" } },
    })
    if not self.font then error(self.font_error) end
end

function sdlgpu_2d_fixture:onDestroy()
    for _, name in ipairs({ "markers", "white", "straight", "premul", "outer", "inner" }) do
        require("resource_pool").pool:removeByName(1, "2d:" .. name)
    end
    if self.font then require("resource_pool").pool:remove(self.font) end
    window:setSize(self.old_width, self.old_height)
end

function sdlgpu_2d_fixture:onUpdate()
    if require("imgui").ImGui.Begin("2D comparison") then
        require("imgui").ImGui.Text("640 x 480 canvas. Capture excludes ImGui.")
        require("imgui").ImGui.Text("Rename 2d-capture.png after each backend run.")
        if require("imgui").ImGui.Button("Capture again") then self.capture = true end
    end
    require("imgui").ImGui.End()
end

function sdlgpu_2d_fixture:onRender()
    self:camera(640, 480)
    lstg.RenderClear(lstg.Color(255, 32, 48, 64))
    lstg.Renderer.setVertexColorBlendState(3)
    lstg.Renderer.setBlendState(2)
    lstg.SetTextureSamplerState("2d:markers", "point+clamp")
    self:quad("2d:markers", 16, 16, 96, 96, 0xffffffff)
    -- Changing the texture closes the previous batch before changing its sampler.
    lstg.Renderer.setTexture("2d:white")
    lstg.SetTextureSamplerState("2d:markers", "linear+clamp")
    self:quad("2d:markers", 128, 16, 96, 96, 0xffffffff)
    self:quad("2d:white", 240, 16, 96, 96, 0xffff0000, 0.5, 0xff0000ff)

    -- Eight rows: straight/premultiplied texture, then four vertex-color modes.
    -- Eleven columns cover every IRenderer blend state, including disabled.
    for alpha = 0, 1 do
        for vertex = 0, 3 do
            for blend = 0, 10 do
                self.cell_x = 16 + blend * 56
                self.cell_y = 128 + (alpha * 4 + vertex) * 27
                lstg.Renderer.setVertexColorBlendState(3)
                lstg.Renderer.setBlendState(2)
                self:quad("2d:white", self.cell_x, self.cell_y, 50, 23, 0xff486078)
                lstg.Renderer.setVertexColorBlendState(vertex)
                lstg.Renderer.setBlendState(blend)
                self:quad(alpha == 0 and "2d:straight" or "2d:premul",
                    self.cell_x + 6, self.cell_y + 4, 38, 15, 0xc0a0c080)
            end
        end
    end

    lstg.PushRenderTarget("2d:outer")
    self:camera(64, 48)
    lstg.RenderClear(lstg.Color(255, 24, 32, 40))
    lstg.ClearZBuffer(1)
    lstg.PushRenderTarget("2d:inner")
    self:camera(64, 48)
    lstg.RenderClear(lstg.Color(128, 0, 64, 0))
    lstg.Renderer.setVertexColorBlendState(3)
    lstg.Renderer.setBlendState(1)
    self:quad("2d:straight", 8, 8, 48, 32, 0xffffffff)
    lstg.PopRenderTarget()
    self:camera(64, 48)
    lstg.SetZBufferEnable(1)
    self:quad("2d:inner", 0, 0, 64, 48, 0xffffffff, 0.6)
    self:quad("2d:white", 24, 12, 24, 24, 0xff0000ff, 0.2)
    self:quad("2d:white", 28, 16, 16, 16, 0xffff0000, 0.8)
    lstg.PopRenderTarget()
    self:camera(640, 480)
    self:quad("2d:outer", 16, 360, 64, 48, 0xffffffff)

    lstg.Renderer.setScissorRect(104, 372, 136, 396)
    self:quad("2d:white", 96, 360, 48, 48, 0xffffff00)
    lstg.Renderer.setScissorRect(0, 0, 640, 480)
    for fog = 1, 3 do
        lstg.Renderer.setFogState(fog, 0xff204080, fog == 1 and 0 or 0.002, 640)
        self:quad("2d:white", 160 + (fog - 1) * 64, 360, 48, 48, 0xffffffff)
    end
    lstg.SetFog()
    lstg.Renderer.setViewport(360, 360, 408, 408)
    lstg.SetOrtho(0, 48, 48, 0)
    self:quad("2d:white", 0, 0, 48, 48, 0xff00ffff)
    self:camera(640, 480)

    self.font:draw("FreeType 2D", 16, 432, { verticalAlign = "top", color = lstg.Color(255, 255, 255, 255) })
    if self.frames >= 1 then
        self.font:draw("atlas update: 0123456789", 200, 432,
            { verticalAlign = "top", color = lstg.Color(255, 255, 255, 255) })
    end
    self.frames = self.frames + 1
    if self.capture and self.frames >= 3 then
        lstg.Snapshot("2d-capture.png")
        lstg.SaveTexture("2d:outer", "2d-target.png")
        self.capture = false
    end
end

require("test").registerTest("test.graphics.Core2D", sdlgpu_2d_fixture, "Graphics: deterministic core 2D")
