#include "core/VideoDecoder.hpp"
#include "core/Logger.hpp"

namespace core
{
    bool IVideoDecoder::create(IData*, IVideoDecoder** const output_decoder)
    {
        *output_decoder = nullptr;
        Logger::error("[core] Video decoding is unavailable on this platform");
        return false;
    }

    bool IVideoDecoder::create(std::string_view, IVideoDecoder** const output_decoder)
    {
        *output_decoder = nullptr;
        Logger::error("[core] Video decoding is unavailable on this platform");
        return false;
    }
}
