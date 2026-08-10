#include "GameResource/Implement/ResourceSoundEffectImpl.hpp"

namespace luastg
{
    ResourceSoundEffectImpl::ResourceSoundEffectImpl(char const* const name, core::IAudioAsset* const asset)
        : ResourceBaseImpl(ResourceType::SoundEffect, name), m_asset(asset)
    {
    }
}
