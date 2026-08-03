#pragma once
#include "GameResource/ResourceBase.hpp"
#include "core/AudioSystem.hpp"

namespace luastg {
	struct IResourceSoundEffect : public IResourceBase {
		[[nodiscard]] virtual core::IAudioAsset* GetAudioAsset() const noexcept = 0;
	};
}

namespace core {
	template<> constexpr InterfaceId getInterfaceId<luastg::IResourceSoundEffect>() { return UUID::parse("300c1295-98f3-5f6d-b285-d0be8d5ae3a2"); }
}
