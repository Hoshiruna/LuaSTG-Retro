#pragma once
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include "GameResource/ResourceMusic.hpp"
#include "core/SmartReference.hpp"

namespace luastg {
	class ResourceMusicImpl final : public ResourceBaseImpl<IResourceMusic> {
	public:
		ResourceMusicImpl(char const* name, core::IAudioAsset* asset, core::AudioFrameRange loop_range);
		[[nodiscard]] core::IAudioAsset* GetAudioAsset() const noexcept override { return m_asset.get(); }
		[[nodiscard]] core::AudioFrameRange GetLoopRange() const noexcept override { return m_loop_range; }

	private:
		core::SmartReference<core::IAudioAsset> m_asset;
		core::AudioFrameRange m_loop_range;
	};
}
