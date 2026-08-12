#pragma once
#include "core/Display.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include <SDL3/SDL_video.h>

namespace core
{
    class DisplaySDL3 final : public implement::ReferenceCounted<IDisplay>
    {
    public:
        explicit DisplaySDL3(SDL_DisplayID id) noexcept;

        uint32_t getSDLDisplayID() const override;
        void getFriendlyName(IImmutableString** output) override;
        Vector2U getSize() override;
        Vector2I getPosition() override;
        RectI getRect() override;
        Vector2U getWorkAreaSize() override;
        Vector2I getWorkAreaPosition() override;
        RectI getWorkAreaRect() override;
        bool isPrimary() override;
        float getDisplayScale() override;

    private:
        SDL_DisplayID m_id{};
    };
}
