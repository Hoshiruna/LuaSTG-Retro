#include "core/Application.hpp"
#include "core/Logger.hpp"
#include "core/SdlRuntime.hpp"
#include "sdl/EventDispatcher.hpp"
#include "sdl/Window.hpp"
#include <SDL3/SDL.h>
#include <atomic>
#include <cassert>
#include <thread>

namespace
{
    core::SdlRuntime sdl_runtime;
    core::IApplication* application_instance{};
    std::thread::id main_thread_id;
    std::atomic_bool exit_requested{};
    bool is_updating{};
    bool update_enabled{ true };
    bool delegate_update_enabled{ true };
}

namespace core
{
    void ApplicationManager::run(IApplication* const application)
    {
        assert(application != nullptr);
        if(application == nullptr) {
            return;
        }
        if(!sdl_runtime.initialize()) {
            Logger::error("[sdl] ApplicationManager could not initialize SDL: {}", SDL_GetError());
            return;
        }

        application_instance = application;
        main_thread_id = std::this_thread::get_id();
        exit_requested.store(false, std::memory_order_relaxed);

        if(!application->onCreate()) {
            application_instance = nullptr;
            return;
        }

        while(!exit_requested.load(std::memory_order_relaxed)) {
            runBeforeUpdate();

            SDL_Event event{};
            while(SDL_PollEvent(&event)) {
                SDLEventDispatcher::dispatch(event);
                WindowSDL3::dispatchSDLEvent(event);
                if(event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    exit_requested.store(true, std::memory_order_relaxed);
                }
            }

            if(exit_requested.load(std::memory_order_relaxed)) {
                break;
            }
            runUpdate();
        }

        application->onDestroy();
        application_instance = nullptr;
    }

    IApplication* ApplicationManager::getApplication()
    {
        return application_instance;
    }

    void ApplicationManager::requestExit()
    {
        exit_requested.store(true, std::memory_order_relaxed);
        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;
        if(!SDL_PushEvent(&event)) {
            Logger::error("[sdl] ApplicationManager could not push the quit event: {}", SDL_GetError());
        }
    }

    bool ApplicationManager::isMainThread()
    {
        return std::this_thread::get_id() == main_thread_id;
    }

    bool ApplicationManager::isUpdating()
    {
        return is_updating;
    }

    void ApplicationManager::runBeforeUpdate()
    {
        if(!update_enabled || is_updating || application_instance == nullptr) {
            return;
        }
        is_updating = true;
        application_instance->onBeforeUpdate();
        is_updating = false;
    }

    void ApplicationManager::runUpdate()
    {
        if(!update_enabled || is_updating || application_instance == nullptr) {
            return;
        }
        is_updating = true;
        const bool keep_running = application_instance->onUpdate();
        is_updating = false;
        if(!keep_running) {
            requestExit();
        }
    }

    bool ApplicationManager::isUpdateEnabled()
    {
        return update_enabled;
    }

    void ApplicationManager::setUpdateEnabled(const bool enabled)
    {
        update_enabled = enabled;
    }

    bool ApplicationManager::isDelegateUpdateEnabled()
    {
        return delegate_update_enabled;
    }

    void ApplicationManager::setDelegateUpdateEnabled(const bool enabled)
    {
        delegate_update_enabled = enabled;
    }
}
