#pragma once

#include <cstdint>
#include <string_view>

namespace core::Graphics::SDLGPU
{
    bool writePng(std::string_view path, uint32_t width, uint32_t height, const void* rgba, uint32_t pitch);
}
