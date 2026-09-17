#include "Options.hpp"
#include <charconv>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace core::Graphics::SDLGPU::smoke
{
    Options parseOptions(const int argc, char* argv[])
    {
        Options result;
        for(int i = 1; i < argc; ++i) {
            const std::string_view argument(argv[i]);
            if(argument == "--help") {
                result.help = true;
            } else if(argument == "--verify") {
                result.verify = true;
            } else if(argument.starts_with("--driver=")) {
                result.driver = argument.substr(9);
                if(result.driver != "direct3d12" && result.driver != "vulkan" && result.driver != "auto") {
                    throw std::runtime_error("--driver must be direct3d12, vulkan, or auto");
                }
            } else if(argument.starts_with("--frames=")) {
                const auto value = argument.substr(9);
                uint64_t count{};
                const auto parsed = std::from_chars(value.data(), value.data() + value.size(), count);
                if(parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || count == 0) {
                    throw std::runtime_error("--frames requires a positive integer");
                }
                result.frames = count;
            } else if(argument.starts_with("--capture=")) {
                result.capture = argument.substr(10);
                if(result.capture.empty()) {
                    throw std::runtime_error("--capture requires an output path");
                }
            } else {
                throw std::runtime_error("Unknown argument: " + std::string(argument));
            }
        }
        if((result.verify || !result.capture.empty()) && !result.frames) {
            result.frames = 1;
        }
        return result;
    }

    void printUsage()
    {
        std::puts(
            "Core.Graphics.SDLGPU.Smoke [options]\n"
            "  --driver=direct3d12|vulkan|auto\n"
            "                             GPU driver; auto probes in preference order (default: direct3d12)\n"
            "  --frames=N                 Exit after N submitted frames (N > 0)\n"
            "  --capture=PATH             Save the final grayscale canvas as a P6 PPM\n"
            "  --verify                   Check grayscale and alpha-blend samples\n"
            "  --help                     Show this help without initializing SDL\n"
            "Capture or verification defaults to one frame unless --frames is set.\n"
            "Without these options, run interactively until the window is closed.");
    }
}
