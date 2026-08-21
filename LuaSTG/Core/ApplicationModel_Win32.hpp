#pragma once

#include "Core/ApplicationModel.hpp"
#include "Core/Graphics/Direct3D11/Device.hpp"
#include "Core/Graphics/Direct3D11/FrameQuery.hpp"
#include "Core/Graphics/Renderer_D3D11.hpp"
#include "Core/Graphics/SwapChain_D3D11.hpp"
#include "core/SdlRuntime.hpp"
#include "core/Window.hpp"
#include "core/implement/ReferenceCounted.hpp"

#include <SDL3/SDL_events.h>

#include <atomic>
#include <vector>

namespace core
{
    class ApplicationModel_Win32 final : public implement::ReferenceCounted<IApplicationModel>
    {
    public:
        explicit ApplicationModel_Win32(IApplicationEventListener* listener);
        ~ApplicationModel_Win32();

        IWindow* getWindow() override { return *m_window; }
        void requestExit() override;

        IFrameRateController* getFrameRateController() override { return &m_frame_rate_controller; }
        Graphics::IDevice* getDevice() override { return *m_device; }
        Graphics::ISwapChain* getSwapChain() override { return *m_swapchain; }
        Graphics::IRenderer* getRenderer() override { return *m_renderer; }
        FrameStatistics getFrameStatistics() override;
        FrameRenderStatistics getFrameRenderStatistics() override;

        bool run() override;

    private:
        bool runSingleThread();
        void runFrame();
        void renderExposedFrame();
        static bool SDLCALL sdlEventWatch(void* userdata, SDL_Event* event);

        SdlRuntime m_sdl_runtime;
        SmartReference<IWindow> m_window;
        std::atomic_bool m_exit_flag{};
        bool m_running{};
        bool m_updating{};
        bool m_rendering{};

        SmartReference<Graphics::Direct3D11::Device> m_device;
        SmartReference<Graphics::SwapChain_D3D11> m_swapchain;
        SmartReference<Graphics::Renderer_D3D11> m_renderer;
        FrameRateController m_frame_rate_controller;
        IApplicationEventListener* m_listener{};
        size_t m_framestate_index{};
        FrameStatistics m_framestate[2]{};
        std::vector<Graphics::Direct3D11::FrameQuery> m_frame_queries;
        size_t m_frame_query_index{};
    };
}
