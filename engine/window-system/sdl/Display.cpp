#include "sdl/Display.hpp"
#include "core/Window.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cassert>

namespace
{
    SDL_Rect getBounds(const SDL_DisplayID id, const bool usable)
    {
        SDL_Rect bounds{};
        const bool ok = usable ? SDL_GetDisplayUsableBounds(id, &bounds) : SDL_GetDisplayBounds(id, &bounds);
        if(!ok) {
            core::Logger::error("[sdl] Failed to read display bounds: {}", SDL_GetError());
        }
        return bounds;
    }

    core::RectI toRect(const SDL_Rect& value)
    {
        return { value.x, value.y, value.x + value.w, value.y + value.h };
    }
}

namespace core
{
    Display::Display(const SDL_DisplayID id) noexcept
        : m_id(id)
    {
    }

    uint32_t Display::getSDLDisplayID() const noexcept
    {
        return m_id;
    }

    void Display::getFriendlyName(IImmutableString** const output)
    {
        assert(output != nullptr);
        const char* const name = SDL_GetDisplayName(m_id);
        IImmutableString::create(name != nullptr ? name : "", output);
    }

    Vector2U Display::getSize()
    {
        const auto bounds = getBounds(m_id, false);
        return { static_cast<uint32_t>(bounds.w), static_cast<uint32_t>(bounds.h) };
    }

    Vector2I Display::getPosition()
    {
        const auto bounds = getBounds(m_id, false);
        return { bounds.x, bounds.y };
    }

    RectI Display::getRect()
    {
        return toRect(getBounds(m_id, false));
    }

    Vector2U Display::getWorkAreaSize()
    {
        const auto bounds = getBounds(m_id, true);
        return { static_cast<uint32_t>(bounds.w), static_cast<uint32_t>(bounds.h) };
    }

    Vector2I Display::getWorkAreaPosition()
    {
        const auto bounds = getBounds(m_id, true);
        return { bounds.x, bounds.y };
    }

    RectI Display::getWorkAreaRect()
    {
        return toRect(getBounds(m_id, true));
    }

    bool Display::isPrimary()
    {
        return m_id == SDL_GetPrimaryDisplay();
    }

    float Display::getDisplayScale()
    {
        const float scale = SDL_GetDisplayContentScale(m_id);
        if(scale <= 0.0f) {
            Logger::error("[sdl] Failed to read display scale: {}", SDL_GetError());
            return 1.0f;
        }
        return scale;
    }
}

namespace core
{
    bool IDisplay::getAll(size_t* const count, IDisplay** const output)
    {
        assert(count != nullptr);
        int display_count{};
        SDL_DisplayID* const displays = SDL_GetDisplays(&display_count);
        if(displays == nullptr) {
            Logger::error("[sdl] SDL_GetDisplays failed: {}", SDL_GetError());
            return false;
        }

        const size_t requested = *count;
        *count = static_cast<size_t>(display_count);
        if(output != nullptr) {
            const size_t available = requested == 0 ? *count : std::min(requested, *count);
            for(size_t i = 0; i < available; ++i) {
                output[i] = new Display(displays[i]);
            }
        }
        SDL_free(displays);
        return true;
    }

    bool IDisplay::getPrimary(IDisplay** const output)
    {
        assert(output != nullptr);
        const SDL_DisplayID id = SDL_GetPrimaryDisplay();
        if(id == 0) {
            Logger::error("[sdl] SDL_GetPrimaryDisplay failed: {}", SDL_GetError());
            return false;
        }
        *output = new Display(id);
        return true;
    }

    bool IDisplay::getNearestFromWindow(IWindow* const window, IDisplay** const output)
    {
        assert(window != nullptr);
        assert(output != nullptr);
        const SDL_DisplayID id = SDL_GetDisplayForWindow(window->getSDLWindow());
        if(id == 0) {
            Logger::error("[sdl] SDL_GetDisplayForWindow failed: {}", SDL_GetError());
            return false;
        }
        *output = new Display(id);
        return true;
    }
}
