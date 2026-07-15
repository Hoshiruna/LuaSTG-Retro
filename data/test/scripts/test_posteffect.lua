local test = require("test")
local resources = require("resource_pool")

---@class test.Module.PostEffect : test.Base
local M = {}

function M:onCreate()
    resources.loadTexture("tex:block", "res/block.png")
    local w, h = lstg.GetTextureSize("tex:block")
    resources.createSprite("img:block", "tex:block", 0, 0, w, h)
    resources.createRenderTarget("rt:target")
    resources.loadFX("fx:rgb_select", "res/rgb_select.hlsl")
    resources.loadFX("fx:rgb_select_new", "res/rgb_select_new.hlsl")
    self.shader = lstg.CreatePostEffectShader("res/shader_mask.hlsl")

    resources.createRenderTarget("rt:background_1")
    resources.createRenderTarget("rt:mask_1")
    do
        resources.loadTexture("tex:mask_1", "res/mask_1.png")
        local ww, hh = lstg.GetTextureSize("tex:mask_1")
        resources.createSprite("img:mask_1", "tex:mask_1", 0, 0, ww, hh)
    end
    do
        resources.loadTexture("tex:image_1", "res/image_1.png")
        local ww, hh = lstg.GetTextureSize("tex:image_1")
        resources.createSprite("img:image_1", "tex:image_1", 0, 0, ww, hh)
    end


    self.timer = -1
    self.r = 0.0
    self.g = 0.0
    self.b = 0.0
    local function wait(f)
        for _ = 1, f do
            coroutine.yield()
        end
    end
    self.task = coroutine.create(function()
        while true do
            -- r
            for i = 1, 60 do
                self.r = i / 60
                wait(1)
            end
            wait(60)
            for i = 59, 0, -1 do
                self.r = i / 60
                wait(1)
            end
            wait(60)
            -- g
            for i = 1, 60 do
                self.g = i / 60
                wait(1)
            end
            wait(60)
            for i = 59, 0, -1 do
                self.g = i / 60
                wait(1)
            end
            wait(60)
            -- b
            for i = 1, 60 do
                self.b = i / 60
                wait(1)
            end
            wait(60)
            for i = 59, 0, -1 do
                self.b = i / 60
                wait(1)
            end
            wait(60)
        end
    end)
end

function M:onDestroy()
    resources.removeResource("test", 2, "img:block")
    resources.removeResource("test", 1, "tex:block")
    resources.removeResource("test", 1, "rt:target")
    resources.removeResource("test", 9, "fx:rgb_select")
    resources.removeResource("test", 9, "fx:rgb_select_new")

    resources.removeResource("test", 2, "img:mask_1")
    resources.removeResource("test", 1, "tex:mask_1")
    resources.removeResource("test", 2, "img:image_1")
    resources.removeResource("test", 1, "tex:image_1")
    resources.removeResource("test", 1, "rt:background_1")
    resources.removeResource("test", 1, "rt:mask_1")
end

function M:onUpdate()
    self.timer = self.timer + 1
    if coroutine.status(self.task) ~= "dead" then
        assert(coroutine.resume(self.task))
    end
end

function M:onRender()
    lstg.PushRenderTarget("rt:target")
    window:applyCameraV()
    lstg.Render("img:block", window.width * 1 / 4, window.height / 2, 0, 1)
    lstg.PopRenderTarget() -- "rt:target"
    lstg.PostEffect("fx:rgb_select", "rt:target", 6, "mul+alpha", -- 着色器名称，屏幕渲染目标，采样器类型，（最终绘制出来的）混合模式
        -- 浮点参数
        {
            -- self.r, self.g, self.b, 0.0 -- test error report
            { self.r, self.g, self.b, 0.0 }, -- channel_factor(r, g, b, X)
        },
        -- 纹理与采样器类型参数
        {}
    )

    lstg.PushRenderTarget("rt:target")
    window:applyCameraV()
    lstg.Render("img:block", window.width * 3 / 4, window.height / 2, 0, 1)
    lstg.PopRenderTarget() -- "rt:target"
    lstg.PostEffect("rt:target", "fx:rgb_select_new", "mul+alpha", -- 屏幕渲染目标，着色器名称，（最终绘制出来的）混合模式
        -- 其他参数
        {
            channel_factor = lstg.Color(255, 255 * self.r, 255 * self.g, 255 * self.b),
        }
    )

    lstg.PushRenderTarget("rt:background_1")
        lstg.RenderClear(lstg.Color(0))
        lstg.Render("img:image_1", 128, 128, 0, 1)
    lstg.PopRenderTarget()
    lstg.PushRenderTarget("rt:mask_1")
        lstg.RenderClear(lstg.Color(0))
        lstg.Render("img:mask_1", 128, 128, 0, 1)
    lstg.PopRenderTarget()
    self.shader:setTexture("g_texture", "rt:mask_1")
    self.shader:setTexture("g_render_target", "rt:background_1")
    self.shader:setFloat2("g_render_target_size", window.width, window.height)
    self.shader:setFloat4("g_viewport", 128 + 128 * lstg.sin(self.timer), 0, window.width, window.height)
    lstg.PostEffect(self.shader, "")
end

test.registerTest("test.Module.PostEffect", M)
