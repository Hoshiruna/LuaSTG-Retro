#pragma once
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include "GameResource/ResourceSoundEffect.hpp"
#include "core/SmartReference.hpp"

namespace luastg
{
    class ResourceSoundEffectImpl final : public ResourceBaseImpl<IResourceSoundEffect>
    {
    public:
        ResourceSoundEffectImpl(char const* name, core::IAudioAsset* asset);
        [[nodiscard]] core::IAudioAsset* GetAudioAsset() const noexcept override { return m_asset.get(); }

    private:
        core::SmartReference<core::IAudioAsset> m_asset;
    };
}
