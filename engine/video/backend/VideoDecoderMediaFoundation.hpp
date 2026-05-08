#pragma once
#include "core/VideoDecoder.hpp"
#include "core/SmartReference.hpp"
#include "core/implement/ReferenceCounted.hpp"

#include <wrl/client.h>
#include <mfidl.h>
#include <mfreadwrite.h>

namespace core {
	class VideoDecoderMediaFoundation final : public implement::ReferenceCounted<IVideoDecoder> {
	public:
		uint32_t getWidth() const noexcept override { return m_width; }
		uint32_t getHeight() const noexcept override { return m_height; }
		double getDuration() const noexcept override { return m_duration; }
		bool seek(double seconds) override;
		VideoDecodeResult readFrame(VideoFrame& frame) override;

		bool open(std::string_view path);

	private:
		bool openFromPhysicalFile(std::string_view path, IMFAttributes* attributes);
		bool openFromMemory(std::string_view path, IMFAttributes* attributes);
		bool updateMediaType();

	private:
		SmartReference<IData> m_source_data;
		Microsoft::WRL::ComPtr<IMFSourceReader> m_reader;
		uint32_t m_width{};
		uint32_t m_height{};
		int32_t m_stride{};
		double m_duration{};
	};
}
