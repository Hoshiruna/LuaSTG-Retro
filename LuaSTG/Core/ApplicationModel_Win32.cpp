#include "Core/ApplicationModel_Win32.hpp"

#include "Platform/ProcessorInfo.hpp"
#include "Platform/WindowsVersion.hpp"
#include "core/Configuration.hpp"
#include "core/InputSystem.hpp"
#include "sdl/EventDispatcher.hpp"
#include "sdl/Window.hpp"

#include <SDL3/SDL.h>

#include <cassert>
#include <cstdio>
#include <iterator>
#include <stdexcept>
#include <string>

namespace core
{
    static std::string bytesCountToString(DWORDLONG const size)
    {
        char buffer[64]{};
        int count{};
        if(size < 1024llu) {
            count = std::snprintf(buffer, std::size(buffer), "%u B", static_cast<unsigned int>(size));
        } else if(size < 1024llu * 1024llu) {
            count = std::snprintf(buffer, std::size(buffer), "%.2f KiB", static_cast<double>(size) / 1024.0);
        } else if(size < 1024llu * 1024llu * 1024llu) {
            count = std::snprintf(buffer, std::size(buffer), "%.2f MiB", static_cast<double>(size) / 1048576.0);
        } else {
            count = std::snprintf(buffer, std::size(buffer), "%.2f GiB", static_cast<double>(size) / 1073741824.0);
        }
        return std::string(buffer, static_cast<size_t>(count));
    }

    static void logSystemMemoryStatus()
    {
        MEMORYSTATUSEX info{ sizeof(MEMORYSTATUSEX) };
        if(!GlobalMemoryStatusEx(&info)) {
            spdlog::error("[core] Failed to retrieve system memory usage");
            return;
        }

        spdlog::info(
            "[core] System memory usage:\n"
            "    Physical memory usage: {}%\n"
            "    Total physical memory: {}\n"
            "    Available physical memory: {}\n"
            "    Current process commit memory limit: {}\n"
            "    Current process available commit memory: {}\n"
            "    Current process user-mode address space limit: {}\n"
            "    Current process available user-mode address space: {}",
            info.dwMemoryLoad,
            bytesCountToString(info.ullTotalPhys),
            bytesCountToString(info.ullAvailPhys),
            bytesCountToString(info.ullTotalPageFile),
            bytesCountToString(info.ullAvailPageFile),
            bytesCountToString(info.ullTotalVirtual),
            bytesCountToString(info.ullAvailVirtual));
    }

    class ScopeTimer
    {
    public:
        explicit ScopeTimer(double& output)
            : m_output(output), m_start(SDL_GetTicksNS())
        {
        }

        ~ScopeTimer()
        {
            auto const current = SDL_GetTicksNS();
            m_output = static_cast<double>(current - m_start) / 1'000'000'000.0;
        }

    private:
        double& m_output;
        uint64_t m_start;
    };

    bool SDLCALL ApplicationModel_Win32::sdlEventWatch(void* const userdata, SDL_Event* const event)
    {
        auto* const self = static_cast<ApplicationModel_Win32*>(userdata);
        if(self == nullptr || event == nullptr || event->type != SDL_EVENT_WINDOW_EXPOSED || !self->m_running) {
            return true;
        }

        auto* const window = self->m_window->getSDLWindow();
        if(window != nullptr && event->window.windowID == SDL_GetWindowID(window)) {
            self->renderExposedFrame();
        }
        return true;
    }

    void ApplicationModel_Win32::renderExposedFrame()
    {
        if(m_updating || m_rendering || m_exit_flag.load(std::memory_order_relaxed)) {
            return;
        }

        m_rendering = true;
        m_swapchain->applyRenderAttachment();
        m_swapchain->clearRenderAttachment();
        if(m_listener->onRender()) {
            m_swapchain->present();
            TracyD3D11Collect(m_device->GetTracyContext());
        }
        m_rendering = false;
    }

    bool ApplicationModel_Win32::runSingleThread()
    {
        SetThreadAffinityMask(GetCurrentThread(), 1);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        TracyD3D11Collect(m_device->GetTracyContext());
        FrameMark;
        {
            tracy_zone_scoped_with_name("OnInitWait");
            m_swapchain->waitFrameLatency();
            m_frame_rate_controller.update();
        }

        if(!SDL_AddEventWatch(&ApplicationModel_Win32::sdlEventWatch, this)) {
            spdlog::error("[sdl] SDL_AddEventWatch failed: {}", SDL_GetError());
            return false;
        }

        m_exit_flag.store(false, std::memory_order_relaxed);
        m_running = true;
        while(!m_exit_flag.load(std::memory_order_relaxed)) {
            SDL_Event event{};
            while(SDL_PollEvent(&event)) {
                SDLEventDispatcher::dispatch(event);
                InputSystem::getInstance().processEvent(event);
                WindowSDL3::dispatchSDLEvent(event);
                if(event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    m_exit_flag.store(true, std::memory_order_relaxed);
                }
            }
            InputSystem::getInstance().update();

            if(m_exit_flag.load(std::memory_order_relaxed)) {
                break;
            }
            runFrame();
        }

        m_running = false;
        SDL_RemoveEventWatch(&ApplicationModel_Win32::sdlEventWatch, this);
        return true;
    }

    void ApplicationModel_Win32::runFrame()
    {
        size_t const frame_index = (m_framestate_index + 1) % std::size(m_framestate);
        auto& statistics = m_framestate[frame_index];
        ScopeTimer total_timer(statistics.total_time);
        size_t const frame_query_index = (m_frame_query_index + 1) % m_frame_queries.size();
        auto& frame_query = m_frame_queries[frame_query_index];

        bool update_result{};
        {
            tracy_zone_scoped_with_name("OnUpdate");
            ScopeTimer update_timer(statistics.update_time);
            m_updating = true;
            update_result = m_listener->onUpdate();
            m_updating = false;
        }

        if(update_result && !m_rendering) {
            m_rendering = true;
            bool render_result{};
            tracy_zone_scoped_with_name("OnRender");
            tracy_d3d11_context_zone(m_device->GetTracyContext(), "OnRender");
            {
                ScopeTimer render_timer(statistics.render_time);
                frame_query.begin();
                m_swapchain->applyRenderAttachment();
                m_swapchain->clearRenderAttachment();
                render_result = m_listener->onRender();
                frame_query.end();
            }

            if(render_result) {
                tracy_zone_scoped_with_name("OnPresent");
                ScopeTimer present_timer(statistics.present_time);
                m_swapchain->present();
                TracyD3D11Collect(m_device->GetTracyContext());
            }
            m_rendering = false;
        }

        {
            tracy_zone_scoped_with_name("OnWait");
            ScopeTimer wait_timer(statistics.wait_time);
            m_swapchain->waitFrameLatency();
            m_frame_rate_controller.update();
        }

        m_framestate_index = frame_index;
        m_frame_query_index = frame_query_index;
        FrameMark;
    }

    FrameStatistics ApplicationModel_Win32::getFrameStatistics()
    {
        return m_framestate[m_framestate_index];
    }

    FrameRenderStatistics ApplicationModel_Win32::getFrameRenderStatistics()
    {
        FrameRenderStatistics statistics{};
        statistics.render_time = m_frame_queries[m_frame_query_index].getTime();
        return statistics;
    }

    void ApplicationModel_Win32::requestExit()
    {
        m_exit_flag.store(true, std::memory_order_relaxed);
        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;
        if(!SDL_PushEvent(&event)) {
            spdlog::error("[sdl] SDL_PushEvent failed: {}", SDL_GetError());
        }
    }

    bool ApplicationModel_Win32::run()
    {
        return runSingleThread();
    }

    ApplicationModel_Win32::ApplicationModel_Win32(IApplicationEventListener* const listener)
        : m_listener(listener)
    {
        assert(m_listener);
        if(!m_sdl_runtime.initialize()) {
            throw std::runtime_error("SdlRuntime::initialize");
        }

        spdlog::info("[core] System: {}", Platform::WindowsVersion::GetName());
        spdlog::info("[core] Kernel: {}", Platform::WindowsVersion::GetKernelVersionString());
        spdlog::info("[core] CPU: {}", Platform::ProcessorInfo::name());
        logSystemMemoryStatus();

        if(!IWindow::create(m_window.put())) {
            throw std::runtime_error("IWindow::create");
        }
        auto const& gpu = ConfigurationLoader::getInstance().getGraphicsSystem().getPreferredDeviceName();
        if(!Graphics::Direct3D11::Device::create(gpu, m_device.put())) {
            throw std::runtime_error("Graphics::Direct3D11::Device::create");
        }
        if(!Graphics::SwapChain_D3D11::create(*m_window, *m_device, m_swapchain.put())) {
            throw std::runtime_error("Graphics::SwapChain_D3D11::create");
        }
        if(!Graphics::Renderer_D3D11::create(*m_device, m_renderer.put())) {
            throw std::runtime_error("Graphics::Renderer_D3D11::create");
        }
        m_frame_queries.reserve(2);
        for(size_t index = 0; index < 2; index += 1) {
            m_frame_queries.emplace_back(m_device.get());
        }
        if(!InputSystem::getInstance().initialize()) {
            throw std::runtime_error("InputSystem::initialize");
        }
    }

    ApplicationModel_Win32::~ApplicationModel_Win32()
    {
        InputSystem::getInstance().shutdown();
        m_frame_queries.clear();
        m_renderer.reset();
        m_swapchain.reset();
        m_device.reset();
        m_window.reset();
    }

    bool IApplicationModel::create(IApplicationEventListener* const listener, IApplicationModel** const model)
    {
        try {
            *model = new ApplicationModel_Win32(listener);
            return true;
        } catch(...) {
            *model = nullptr;
            return false;
        }
    }
}
