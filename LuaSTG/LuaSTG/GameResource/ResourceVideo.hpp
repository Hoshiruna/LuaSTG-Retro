#pragma once
#include "GameResource/ResourceTexture.hpp"

namespace luastg {
	enum class VideoPlaybackState {
		Stopped,
		Playing,
		Paused,
		Ended,
	};

	struct IResourceVideo : IResourceTexture {
		virtual void Update(double delta_seconds) = 0;
		virtual bool Play(bool restart) = 0;
		virtual bool Pause() = 0;
		virtual bool Resume() = 0;
		virtual bool Stop() = 0;
		virtual bool Seek(double seconds) = 0;
		virtual VideoPlaybackState GetVideoState() const noexcept = 0;
		virtual double GetVideoTime() const noexcept = 0;
		virtual double GetVideoDuration() const noexcept = 0;
	};
}

namespace core {
	// UUID v5
	// ns:URL
	// https://www.luastg-sub.com/luastg.IResourceVideo
	template<> constexpr InterfaceId getInterfaceId<luastg::IResourceVideo>() { return UUID::parse("efc9090f-2391-5d88-a080-f0be95d9ddaf"); }
}
