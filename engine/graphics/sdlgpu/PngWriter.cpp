#include "PngWriter.hpp"
#include "core/Logger.hpp"
#include <climits>
#include <filesystem>
#include <fstream>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include <stb_image_write.h>

namespace core::Graphics::SDLGPU
{
    bool writePng(std::string_view path, uint32_t width, uint32_t height, const void* rgba, uint32_t pitch)
    {
        if(!rgba || width == 0 || height == 0 || width > INT_MAX / 4 || height > INT_MAX || pitch > INT_MAX || pitch < width * 4) {
            return false;
        }
        try {
            const auto native = std::filesystem::u8path(path.begin(), path.end());
            std::ofstream stream(native, std::ios::binary | std::ios::trunc);
            if(!stream) {
                Logger::error("[sdlgpu] Open PNG output failed: {}", path);
                return false;
            }
            auto write = [](void* user, void* data, int size) {
                static_cast<std::ofstream*>(user)->write(static_cast<const char*>(data), size);
            };
            const bool encoded = stbi_write_png_to_func(write, &stream, static_cast<int>(width), static_cast<int>(height), 4, rgba, static_cast<int>(pitch)) != 0;
            stream.close();
            if(!encoded || !stream) {
                Logger::error("[sdlgpu] Write PNG output failed: {}", path);
                return false;
            }
            return true;
        } catch(const std::exception& error) {
            Logger::error("[sdlgpu] Write PNG '{}': {}", path, error.what());
            return false;
        }
    }
}
