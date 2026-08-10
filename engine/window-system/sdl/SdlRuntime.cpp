#include "core/SdlRuntime.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>

namespace core
{
    SdlRuntime::~SdlRuntime()
    {
        if(m_initialized) {
            SDL_Quit();
        }
    }

    bool SdlRuntime::initialize()
    {
        if(m_initialized) {
            return true;
        }

        constexpr SDL_InitFlags subsystems = SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK;
        if(!SDL_Init(subsystems)) {
            Logger::error("[sdl] SDL_Init failed: {}", SDL_GetError());
            return false;
        }

        m_initialized = true;
        return true;
    }
}
