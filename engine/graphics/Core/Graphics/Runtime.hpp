#pragma once

#include "Core/Graphics/Device.hpp"
#include "Core/Graphics/Renderer.hpp"
#include "Core/Graphics/SwapChain.hpp"
#include <memory>

struct ImDrawData;

namespace core::Graphics
{
    enum class FrameStatus
    {
        Ready,
        Skipped,
        Failed,
    };

    struct GpuFrameStatistics
    {
        double render_time{};
        bool available{};
    };

    // One runtime owns the graphics objects and frame state for one window.
    class IGraphicsRuntime
    {
    public:
        virtual ~IGraphicsRuntime() = default;
        virtual IDevice* device() const noexcept = 0;
        virtual IRenderer* renderer() const noexcept = 0;
        virtual ISwapChain* swapChain() const noexcept = 0;
        virtual FrameStatus beginFrame() noexcept = 0;
        virtual bool submitFrame(bool present) noexcept = 0;
        virtual GpuFrameStatistics statistics() const noexcept = 0;

        // The caller owns the ImGui context and SDL platform backend.
        virtual bool initializeImGui() = 0;
        virtual void shutdownImGui() noexcept = 0;
        virtual void newImGuiFrame() = 0;
        virtual void renderImGui(ImDrawData* data) = 0;
        virtual void drawImGuiImage(ITexture2D* texture, Vector2F size, Vector2F uv0, Vector2F uv1) = 0;

        // renderer_driver names a backend driver, or is "auto" to let the backend pick one.
        // An unavailable named driver is an error; "auto" falls back to another driver.
        static std::unique_ptr<IGraphicsRuntime> create(IWindow* window, StringView renderer_driver);
    };
}
