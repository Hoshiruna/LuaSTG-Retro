#include "Platform/Application.hpp"
#include "spdlog/spdlog.h"
#include <SDL3/SDL_platform.h>

namespace Platform
{
    void prepareApplicationThread()
    {
    }

    void logSystemInfo()
    {
        spdlog::info("[core] System: {}", SDL_GetPlatform());
    }
}
