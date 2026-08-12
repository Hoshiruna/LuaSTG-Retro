#include "sdl/Display.hpp"
#include "core/Window.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cassert>
#include <string_view>

namespace
{
    bool requireMainThread(const std::string_view operation)
    {
        if(SDL_IsMainThread()) {
            return true;
        }
        core::Logger::error("[sdl] {} must be called on the main thread", operation);
        return false;
    }

    SDL_Rect getBounds(const SDL_DisplayID id, const bool usable)
    {
        if(!requireMainThread(usable ? "SDL_GetDisplayUsableBounds" : "SDL_GetDisplayBounds")) {
            return {};
        }
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
    DisplaySDL3::DisplaySDL3(const SDL_DisplayID id) noexcept
        : m_id(id)
    {
    }

    uint32_t DisplaySDL3::getSDLDisplayID() const
    {
        if(!requireMainThread("IDisplay::getSDLDisplayID")) {
            return 0;
        }
        return m_id;
    }

    void DisplaySDL3::getFriendlyName(IImmutableString** const output)
    {
        assert(output != nullptr);
        if(!requireMainThread("SDL_GetDisplayName")) {
            IImmutableString::create("", output);
            return;
        }
        const char* const name = SDL_GetDisplayName(m_id);
        if(name == nullptr) {
            Logger::error("[sdl] SDL_GetDisplayName failed: {}", SDL_GetError());
        }
        IImmutableString::create(name != nullptr ? name : "", output);
    }

    Vector2U DisplaySDL3::getSize()
    {
        const auto bounds = getBounds(m_id, false);
        return { static_cast<uint32_t>(bounds.w), static_cast<uint32_t>(bounds.h) };
    }

    Vector2I DisplaySDL3::getPosition()
    {
        const auto bounds = getBounds(m_id, false);
        return { bounds.x, bounds.y };
    }

    RectI DisplaySDL3::getRect()
    {
        return toRect(getBounds(m_id, false));
    }

    Vector2U DisplaySDL3::getWorkAreaSize()
    {
        const auto bounds = getBounds(m_id, true);
        return { static_cast<uint32_t>(bounds.w), static_cast<uint32_t>(bounds.h) };
    }

    Vector2I DisplaySDL3::getWorkAreaPosition()
    {
        const auto bounds = getBounds(m_id, true);
        return { bounds.x, bounds.y };
    }

    RectI DisplaySDL3::getWorkAreaRect()
    {
        return toRect(getBounds(m_id, true));
    }

    bool DisplaySDL3::isPrimary()
    {
        if(!requireMainThread("SDL_GetPrimaryDisplay")) {
            return false;
        }
        const SDL_DisplayID primary = SDL_GetPrimaryDisplay();
        if(primary == 0) {
            Logger::error("[sdl] SDL_GetPrimaryDisplay failed: {}", SDL_GetError());
            return false;
        }
        return m_id == primary;
    }

    float DisplaySDL3::getDisplayScale()
    {
        if(!requireMainThread("SDL_GetDisplayContentScale")) {
            return 1.0f;
        }
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
        if(!requireMainThread("SDL_GetDisplays")) {
            *count = 0;
            return false;
        }
        int display_count{};
        SDL_DisplayID* const displays = SDL_GetDisplays(&display_count);
        if(displays == nullptr) {
            Logger::error("[sdl] SDL_GetDisplays failed: {}", SDL_GetError());
            return false;
        }

        const size_t requested = *count;
        const size_t available = static_cast<size_t>(display_count);
        if(output != nullptr) {
            *count = std::min(requested, available);
            for(size_t i = 0; i < *count; ++i) {
                output[i] = new DisplaySDL3(displays[i]);
            }
        } else {
            *count = available;
        }
        SDL_free(displays);
        return true;
    }

    bool IDisplay::getPrimary(IDisplay** const output)
    {
        assert(output != nullptr);
        *output = nullptr;
        if(!requireMainThread("SDL_GetPrimaryDisplay")) {
            return false;
        }
        const SDL_DisplayID id = SDL_GetPrimaryDisplay();
        if(id == 0) {
            Logger::error("[sdl] SDL_GetPrimaryDisplay failed: {}", SDL_GetError());
            return false;
        }
        *output = new DisplaySDL3(id);
        return true;
    }

    bool IDisplay::getNearestFromWindow(IWindow* const window, IDisplay** const output)
    {
        assert(window != nullptr);
        assert(output != nullptr);
        *output = nullptr;
        if(!requireMainThread("SDL_GetDisplayForWindow")) {
            return false;
        }
        const SDL_DisplayID id = SDL_GetDisplayForWindow(window->getSDLWindow());
        if(id == 0) {
            Logger::error("[sdl] SDL_GetDisplayForWindow failed: {}", SDL_GetError());
            return false;
        }
        *output = new DisplaySDL3(id);
        return true;
    }
}
