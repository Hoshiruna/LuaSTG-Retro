#include "sdl/EventDispatcher.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cassert>
#include <vector>

namespace
{
    std::vector<core::ISDLEventListener*> listeners;
    bool dispatching{};
}

namespace core
{
    void SDLEventDispatcher::addListener(ISDLEventListener* const listener)
    {
        if(!SDL_IsMainThread()) {
            Logger::error("[sdl] SDLEventDispatcher::addListener must be called on the main thread");
            return;
        }
        if(listener == nullptr) {
            return;
        }
        std::erase(listeners, listener);
        listeners.push_back(listener);
    }

    void SDLEventDispatcher::removeListener(ISDLEventListener* const listener)
    {
        if(!SDL_IsMainThread()) {
            Logger::error("[sdl] SDLEventDispatcher::removeListener must be called on the main thread");
            return;
        }
        std::erase(listeners, listener);
    }

    void SDLEventDispatcher::dispatch(const SDL_Event& event)
    {
        if(!SDL_IsMainThread()) {
            Logger::error("[sdl] SDLEventDispatcher::dispatch must be called on the main thread");
            return;
        }
        assert(!dispatching);
        dispatching = true;
        try {
            const auto current_listeners = listeners;
            for(ISDLEventListener* const listener : current_listeners) {
                if(std::ranges::find(listeners, listener) != listeners.end()) {
                    listener->processSDLEvent(event);
                }
            }
        } catch(...) {
            dispatching = false;
            throw;
        }
        dispatching = false;
    }
}
