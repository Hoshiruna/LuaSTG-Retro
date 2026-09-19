#pragma once

#include <string>
#include <string_view>

namespace core::Graphics::SDLGPU
{
    std::string readShaderSource(std::string_view path);
    std::string expandShaderIncludes(std::string_view source, std::string_view name);
}
