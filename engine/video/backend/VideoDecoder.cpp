#include "core/VideoDecoder.hpp"
#include "core/SmartReference.hpp"
#include "backend/VideoDecoderMediaFoundation.hpp"

namespace core {
	bool IVideoDecoder::create(std::string_view const path, IVideoDecoder** const output_decoder) {
		*output_decoder = nullptr;
		SmartReference<VideoDecoderMediaFoundation> decoder;
		decoder.attach(new VideoDecoderMediaFoundation);
		if (!decoder->open(path)) {
			return false;
		}
		*output_decoder = decoder.detach();
		return true;
	}
}
