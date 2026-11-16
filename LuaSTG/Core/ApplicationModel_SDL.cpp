#include "Core/ApplicationModel_SDL.hpp"

#include "Platform/Application.hpp"
#include "core/Configuration.hpp"
#include "core/InputSystem.hpp"
#include "sdl/EventDispatcher.hpp"
#include "sdl/Window.hpp"

#include <SDL3/SDL.h>

#include <cassert>
#include <iterator>
#include <stdexcept>

namespace core
{
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

    bool SDLCALL ApplicationModel_SDL::sdlEventWatch(void* const userdata, SDL_Event* const event)
    {
        auto* const self = static_cast<ApplicationModel_SDL*>(userdata);
        if(self == nullptr || event == nullptr || event->type != SDL_EVENT_WINDOW_EXPOSED || !self->m_running) {
            return true;
        }

        auto* const window = self->m_window->getSDLWindow();
        if(window != nullptr && event->window.windowID == SDL_GetWindowID(window)) {
            self->renderExposedFrame();
        }
        return true;
    }

    void ApplicationModel_SDL::renderExposedFrame()
    {
        if(!m_has_updated || m_updating || m_rendering || m_exit_flag.load(std::memory_order_relaxed)) {
            return;
        }

        m_rendering = true;
        const auto recovery = m_graphics->serviceRecovery();
        if(recovery != Graphics::RecoveryResult::NotNeeded) {
            if(recovery == Graphics::RecoveryResult::Fatal) {
                m_failed = true;
                requestExit();
            }
            m_rendering = false;
            return;
        }
        const auto status = m_graphics->beginFrame();
        if(status == Graphics::FrameStatus::Ready) {
            if(!m_graphics->submitFrame(m_listener->onRender()) &&
                m_graphics->serviceRecovery() != Graphics::RecoveryResult::Recovered) {
                m_failed = true;
                requestExit();
            }
        } else if(status == Graphics::FrameStatus::Failed &&
            m_graphics->serviceRecovery() != Graphics::RecoveryResult::Recovered) {
            m_failed = true;
            requestExit();
        }
        m_rendering = false;
    }

    bool ApplicationModel_SDL::runSingleThread()
    {
        Platform::prepareApplicationThread();

        FrameMark;
        {
            tracy_zone_scoped_with_name("OnInitWait");
            getSwapChain()->waitFrameLatency();
            m_frame_rate_controller.update();
        }

        if(!SDL_AddEventWatch(&ApplicationModel_SDL::sdlEventWatch, this)) {
            spdlog::error("[sdl] SDL_AddEventWatch failed: {}", SDL_GetError());
            return false;
        }

        m_exit_flag.store(false, std::memory_order_relaxed);
        m_has_updated = false;
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
        SDL_RemoveEventWatch(&ApplicationModel_SDL::sdlEventWatch, this);
        return !m_failed;
    }

    void ApplicationModel_SDL::runFrame()
    {
        m_rendering = true;
        const auto recovery = m_graphics->serviceRecovery();
        m_rendering = false;
        if(recovery == Graphics::RecoveryResult::Fatal) {
            m_failed = true;
            requestExit();
            return;
        }
        size_t const frame_index = (m_framestate_index + 1) % std::size(m_framestate);
        auto& statistics = m_framestate[frame_index];
        statistics = {};
        ScopeTimer total_timer(statistics.total_time);

        bool update_result{};
        {
            tracy_zone_scoped_with_name("OnUpdate");
            ScopeTimer update_timer(statistics.update_time);
            m_updating = true;
            update_result = m_listener->onUpdate();
            m_has_updated = update_result;
            m_updating = false;
        }

        if(update_result && !m_rendering) {
            m_rendering = true;
            bool render_result{};
            tracy_zone_scoped_with_name("OnRender");
            auto const status = m_graphics->beginFrame();
            if(status == Graphics::FrameStatus::Failed &&
                m_graphics->serviceRecovery() != Graphics::RecoveryResult::Recovered) {
                m_failed = true;
                requestExit();
            }
            if(status == Graphics::FrameStatus::Ready) {
                ScopeTimer render_timer(statistics.render_time);
                render_result = m_listener->onRender();
            }

            if(status == Graphics::FrameStatus::Ready) {
                tracy_zone_scoped_with_name("OnPresent");
                ScopeTimer present_timer(statistics.present_time);
                if(!m_graphics->submitFrame(render_result) &&
                    m_graphics->serviceRecovery() != Graphics::RecoveryResult::Recovered) {
                    m_failed = true;
                    requestExit();
                }
            } else if(status == Graphics::FrameStatus::Skipped) {
                SDL_Delay(10);
            }
            m_rendering = false;
        }

        {
            tracy_zone_scoped_with_name("OnWait");
            ScopeTimer wait_timer(statistics.wait_time);
            getSwapChain()->waitFrameLatency();
            m_frame_rate_controller.update();
        }

        m_framestate_index = frame_index;
        FrameMark;
    }

    FrameStatistics ApplicationModel_SDL::getFrameStatistics()
    {
        return m_framestate[m_framestate_index];
    }

    FrameRenderStatistics ApplicationModel_SDL::getFrameRenderStatistics()
    {
        return m_graphics->statistics();
    }

    void ApplicationModel_SDL::requestExit()
    {
        m_exit_flag.store(true, std::memory_order_relaxed);
        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;
        if(!SDL_PushEvent(&event)) {
            spdlog::error("[sdl] SDL_PushEvent failed: {}", SDL_GetError());
        }
    }

    bool ApplicationModel_SDL::run()
    {
        return runSingleThread();
    }

    ApplicationModel_SDL::ApplicationModel_SDL(IApplicationEventListener* const listener)
        : m_listener(listener)
    {
        assert(m_listener);
        if(!m_sdl_runtime.initialize()) {
            throw std::runtime_error("SdlRuntime::initialize");
        }

        Platform::logSystemInfo();

        if(!IWindow::create(m_window.put())) {
            throw std::runtime_error("IWindow::create");
        }
        m_graphics = Graphics::IGraphicsRuntime::create(m_window.get());
        if(!InputSystem::getInstance().initialize()) {
            throw std::runtime_error("InputSystem::initialize");
        }
    }

    ApplicationModel_SDL::~ApplicationModel_SDL()
    {
        InputSystem::getInstance().shutdown();
        m_graphics.reset();
        m_window.reset();
    }

    bool IApplicationModel::create(IApplicationEventListener* const listener, IApplicationModel** const model)
    {
        try {
            *model = new ApplicationModel_SDL(listener);
            return true;
        } catch(std::exception const& exception) {
            spdlog::error("[core] Application initialization failed: {}", exception.what());
            *model = nullptr;
            return false;
        }
    }
}
