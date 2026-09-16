#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace core::Graphics::SDLGPU::smoke
{
    struct Options
    {
        std::string driver{ "direct3d12" };
        std::optional<uint64_t> frames;
        std::string capture;
        bool verify{};
        bool help{};
    };

    Options parseOptions(int argc, char* argv[]);
    void printUsage();
}
