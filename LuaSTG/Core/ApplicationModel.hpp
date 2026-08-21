#pragma once
#include "Core/FrameRateController.hpp"
#include "core/Window.hpp"
#include "Core/Graphics/Device.hpp"
#include "Core/Graphics/SwapChain.hpp"
#include "Core/Graphics/Renderer.hpp"
#include "core/ReferenceCounted.hpp"

namespace core
{
    struct IApplicationEventListener
    {
        // These callbacks run on the main thread.
        virtual bool onUpdate() { return true; }
        virtual bool onRender() { return true; }
    };

    struct FrameStatistics
    {
        double total_time{};
        double wait_time{};
        double update_time{};
        double render_time{};
        double present_time{};
    };

    struct FrameRenderStatistics
    {
        double render_time{};
    };

    struct IApplicationModel : public IReferenceCounted
    {
        // Unless noted otherwise, this interface must be used from the main thread.
        virtual IFrameRateController* getFrameRateController() = 0;
        virtual IWindow* getWindow() = 0;
        virtual Graphics::IDevice* getDevice() = 0;
        virtual Graphics::ISwapChain* getSwapChain() = 0;
        virtual Graphics::IRenderer* getRenderer() = 0;
        virtual FrameStatistics getFrameStatistics() = 0;
        virtual FrameRenderStatistics getFrameRenderStatistics() = 0;

        // This is the only thread-safe operation on the interface.
        virtual void requestExit() = 0;
        virtual bool run() = 0;

        static bool create(IApplicationEventListener* p_app, IApplicationModel** pp_model);
    };

    // UUID v5
    // ns:URL
    // https://www.luastg-sub.com/core.IApplicationModel
    template<>
    constexpr InterfaceId getInterfaceId<IApplicationModel>()
    {
        return UUID::parse("42313368-4b16-511f-895f-ee43f0e10713");
    }
}
