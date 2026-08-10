#pragma once
#include "GameResource/ResourceBase.hpp"
#include "core/AudioSystem.hpp"

namespace luastg
{
    struct IResourceMusic : public IResourceBase
    {
        [[nodiscard]] virtual core::IAudioAsset* GetAudioAsset() const noexcept = 0;
        [[nodiscard]] virtual core::AudioFrameRange GetLoopRange() const noexcept = 0;
    };
}

namespace core
{
    template<>
    constexpr InterfaceId getInterfaceId<luastg::IResourceMusic>()
    {
        return UUID::parse("5a109cf3-31ef-5c4d-9a81-6951d7aecd00");
    }
}
