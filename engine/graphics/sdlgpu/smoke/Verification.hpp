#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace core::Graphics::SDLGPU::smoke
{
    bool verifyPixels(std::span<const uint8_t> pixels);
    void saveCapture(std::span<const uint8_t> pixels, std::string_view path);
}
