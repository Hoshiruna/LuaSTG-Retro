#pragma once
#include "core/Data.hpp"
#include "core/ReferenceCounted.hpp"
#include "core/SmartReference.hpp"

#include <string_view>

namespace core {
	struct VideoFrame {
		SmartReference<IData> pixels;
		uint32_t width{};
		uint32_t height{};
		uint32_t pitch{};
		double timestamp{};
	};

	enum class VideoDecodeResult {
		Frame,
		End,
		Error,
	};

	struct CORE_NO_VIRTUAL_TABLE IVideoDecoder : IReferenceCounted {
		[[nodiscard]] virtual uint32_t getWidth() const noexcept = 0;
		[[nodiscard]] virtual uint32_t getHeight() const noexcept = 0;
		[[nodiscard]] virtual double getDuration() const noexcept = 0;
		[[nodiscard]] virtual bool seek(double seconds) = 0;
		[[nodiscard]] virtual VideoDecodeResult readFrame(VideoFrame& frame) = 0;

		[[nodiscard]] static bool create(std::string_view path, IVideoDecoder** output_decoder);
	};

	// UUID v5
	// ns:URL
	// https://www.luastg-sub.com/core.IVideoDecoder
	template<> constexpr InterfaceId getInterfaceId<IVideoDecoder>() { return UUID::parse("91ebd5c0-8d78-5cd6-8f51-c53cf4cf3326"); }
}
