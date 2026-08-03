#include "GameResource/Implement/ResourceMusicImpl.hpp"

namespace luastg {
	ResourceMusicImpl::ResourceMusicImpl(
		char const* const name,
		core::IAudioAsset* const asset,
		core::AudioFrameRange const loop_range)
		: ResourceBaseImpl(ResourceType::Music, name)
		, m_asset(asset)
		, m_loop_range(loop_range) {
	}
}
