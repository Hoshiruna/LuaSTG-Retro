#pragma once

union SDL_Event;

namespace core
{
    struct ISDLEventListener
    {
        virtual ~ISDLEventListener() = default;
        virtual void processSDLEvent(const SDL_Event& event) = 0;
    };

    class SDLEventDispatcher
    {
    public:
        static void addListener(ISDLEventListener* listener);
        static void removeListener(ISDLEventListener* listener);
        static void dispatch(const SDL_Event& event);
    };
}
