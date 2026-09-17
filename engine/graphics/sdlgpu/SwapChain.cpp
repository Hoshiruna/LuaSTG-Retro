#include "SwapChain.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cmath>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        constexpr char presentation_vertex[] = R"hlsl(
struct Output { float4 position : SV_Position; float2 uv : TEXCOORD0; };
Output main(uint id : SV_VertexID) {
    Output output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.position = float4(output.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}
)hlsl";
        constexpr char presentation_fragment[] = R"hlsl(
Texture2D<float4> canvas : register(t0, space2);
SamplerState canvas_sampler : register(s0, space2);
float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0 {
    return canvas.Sample(canvas_sampler, uv);
}
)hlsl";
    }

    SwapChain::SwapChain(Device* device)
        : m_device(device)
    {
        m_canvas.attach(new RenderTarget(device, { 640, 480 }));
        m_depth.attach(new DepthBuffer(device, { 640, 480 }));
        m_vertex_shader = device->compiler().compile(device->gpu(), presentation_vertex, SDL_GPU_SHADERSTAGE_VERTEX, "runtime presentation vertex");
        m_fragment_shader = device->compiler().compile(device->gpu(), presentation_fragment, SDL_GPU_SHADERSTAGE_FRAGMENT, "runtime presentation fragment");
        SDL_GPUColorTargetDescription target{};
        target.format = device->context().swapchainFormat();
        SDL_GPUGraphicsPipelineCreateInfo pipeline{};
        pipeline.vertex_shader = m_vertex_shader.get();
        pipeline.fragment_shader = m_fragment_shader.get();
        pipeline.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pipeline.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipeline.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipeline.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        pipeline.target_info.color_target_descriptions = &target;
        pipeline.target_info.num_color_targets = 1;
        m_pipeline = { require(SDL_CreateGPUGraphicsPipeline(device->gpu(), &pipeline), "Create presentation pipeline"), { device->gpu() } };
        SDL_GPUSamplerCreateInfo sampler{};
        sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_NEAREST;
        sampler.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sampler.address_mode_u = sampler.address_mode_v = sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        m_point = { require(SDL_CreateGPUSampler(device->gpu(), &sampler), "Create point presentation sampler"), { device->gpu() } };
        sampler.min_filter = sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
        m_linear = { require(SDL_CreateGPUSampler(device->gpu(), &sampler), "Create linear presentation sampler"), { device->gpu() } };
    }
    void SwapChain::addEventListener(ISwapChainEventListener* listener)
    {
        removeEventListener(listener);
        if(listener)
            m_listeners.push_back(listener);
    }
    void SwapChain::removeEventListener(ISwapChainEventListener* listener)
    {
        std::erase(m_listeners, listener);
    }
    bool SwapChain::setCanvasSize(Vector2U size)
    {
        if(!size.x || !size.y)
            return false;
        if(size == getCanvasSize())
            return true;
        try {
            m_device->beforeTransfer();
            SmartReference<IRenderTarget> canvas;
            SmartReference<IDepthStencilBuffer> depth;
            canvas.attach(new RenderTarget(m_device.get(), size));
            depth.attach(new DepthBuffer(m_device.get(), size));
            const auto listeners = m_listeners;
            for(auto* listener : listeners) {
                if(std::find(m_listeners.begin(), m_listeners.end(), listener) != m_listeners.end())
                    listener->onSwapChainDestroy();
            }
            m_canvas = std::move(canvas);
            m_depth = std::move(depth);
            applyRenderAttachment();
            for(auto* listener : listeners) {
                if(std::find(m_listeners.begin(), m_listeners.end(), listener) != m_listeners.end())
                    listener->onSwapChainCreate();
            }
            return true;
        } catch(const std::exception& error) {
            m_device->fail(error);
            return false;
        }
    }
    void SwapChain::applyRenderAttachment()
    {
        if(m_renderer)
            m_renderer->setDefaultAttachment(m_canvas.get(), m_depth.get());
    }
    void SwapChain::clearRenderAttachment()
    {
        if(m_renderer) {
            m_renderer->clearRenderTarget(Color4B::black());
            m_renderer->clearDepthBuffer(1.0f);
        }
    }
    void SwapChain::waitFrameLatency()
    {
        // Frame acquisition uses SDL_WaitAndAcquireGPUSwapchainTexture.
    }
    void SwapChain::prepareFrame()
    {
        if(m_vsync != m_requested_vsync) {
            if(m_device->context().setVSync(m_requested_vsync))
                m_vsync = m_requested_vsync;
            else
                m_requested_vsync = m_vsync;
        }
    }
    bool SwapChain::present()
    {
        try {
            m_device->beforeTransfer();
            auto* frame = require(m_device->frame(), "Present outside a frame");
            SDL_GPUColorTargetInfo target{};
            target.texture = frame->texture();
            target.load_op = SDL_GPU_LOADOP_CLEAR;
            target.store_op = SDL_GPU_STOREOP_STORE;
            target.clear_color = { 0, 0, 0, 1 };
            auto* pass = frame->beginRenderPass({ &target, 1 });
            SDL_GPUViewport viewport{ 0, 0, float(frame->width()), float(frame->height()), 0, 1 };
            bool point{};
            if(m_scaling != SwapChainScalingMode::Stretch) {
                const auto size = getCanvasSize();
                float scale = (std::min)(viewport.w / size.x, viewport.h / size.y);
                if(m_scaling == SwapChainScalingMode::IntegerAspectRatio && scale >= 1) {
                    scale = std::floor(scale);
                    point = true;
                }
                viewport.w = size.x * scale;
                viewport.h = size.y * scale;
                viewport.x = (frame->width() - viewport.w) * 0.5f;
                viewport.y = (frame->height() - viewport.h) * 0.5f;
            }
            SDL_SetGPUViewport(pass, &viewport);
            SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());
            const SDL_GPUTextureSamplerBinding binding{ canvas(), point ? m_point.get() : m_linear.get() };
            SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            return true;
        } catch(const std::exception& error) {
            m_device->fail(error);
            return false;
        }
    }
    bool SwapChain::saveSnapshotToFile(StringView path)
    {
        return m_device->saveTexture(canvas(), getCanvasSize(), path);
    }
}

namespace core::Graphics
{
    bool ISwapChain::create(IWindow* window, IDevice* device, ISwapChain** output)
    {
        if(!window || !device || !output)
            return false;
        *output = nullptr;
        try {
            *output = new SDLGPU::SwapChain(window, static_cast<SDLGPU::Device*>(device));
            return true;
        } catch(const std::exception& error) {
            Logger::error("[sdlgpu] Create swapchain: {}", error.what());
            return false;
        }
    }
}
