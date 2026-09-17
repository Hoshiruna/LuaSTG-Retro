#pragma once
#include "Renderer.hpp"
#include "Core/Graphics/SwapChain.hpp"

namespace core::Graphics::SDLGPU
{
    class SwapChain final : public implement::ReferenceCounted<ISwapChain>
    {
    public:
        // The device already owns and has claimed the window.
        explicit SwapChain(Device* device);
        void setRenderer(Renderer* renderer) noexcept { m_renderer = renderer; }
        void addEventListener(ISwapChainEventListener* listener) override;
        void removeEventListener(ISwapChainEventListener* listener) override;
        bool setWindowMode(Vector2U size) override { return setCanvasSize(size); }
        bool setCanvasSize(Vector2U size) override;
        Vector2U getCanvasSize() override { return m_canvas->getTexture()->getSize(); }
        void setScalingMode(SwapChainScalingMode mode) override { m_scaling = mode; }
        SwapChainScalingMode getScalingMode() override { return m_scaling; }
        void clearRenderAttachment() override;
        void applyRenderAttachment() override;
        void waitFrameLatency() override;
        void setVSync(bool enabled) override { m_requested_vsync = enabled; }
        bool getVSync() override { return m_vsync; }
        bool present() override;
        bool saveSnapshotToFile(StringView path) override;
        void prepareFrame();
        SDL_GPUTexture* canvas() const noexcept { return static_cast<Texture2D*>(m_canvas->getTexture())->handle(); }

    private:
        SmartReference<Device> m_device;
        SmartReference<IRenderTarget> m_canvas;
        SmartReference<IDepthStencilBuffer> m_depth;
        Renderer* m_renderer{};
        Shader m_vertex_shader;
        Shader m_fragment_shader;
        Pipeline m_pipeline;
        Sampler m_point;
        Sampler m_linear;
        std::vector<ISwapChainEventListener*> m_listeners;
        SwapChainScalingMode m_scaling{ SwapChainScalingMode::AspectRatio };
        bool m_vsync{ true };
        bool m_requested_vsync{ true };
    };
}
