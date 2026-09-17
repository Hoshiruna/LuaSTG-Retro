#pragma once

#include "Core/ApplicationModel.hpp"
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
        Graphics::IDevice* getDevice() override { return m_graphics->device(); }
        Graphics::ISwapChain* getSwapChain() override { return m_graphics->swapChain(); }
        Graphics::IRenderer* getRenderer() override { return m_graphics->renderer(); }
        Graphics::IGraphicsRuntime* getGraphicsRuntime() override { return m_graphics.get(); }
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

        std::unique_ptr<Graphics::IGraphicsRuntime> m_graphics;
        FrameRateController m_frame_rate_controller;
        IApplicationEventListener* m_listener{};
        size_t m_framestate_index{};
        FrameStatistics m_framestate[2]{};
        bool m_failed{};
    };
}
