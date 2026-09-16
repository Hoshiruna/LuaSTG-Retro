#include "Verification.hpp"
#include "Scene.hpp"
#include "core/Logger.hpp"
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace core::Graphics::SDLGPU::smoke
{
    namespace
    {
        void checkSize(const std::span<const uint8_t> pixels)
        {
            if(pixels.size() != Scene::width * Scene::height * 4) {
                throw std::runtime_error("Readback did not contain a 320 x 240 RGBA canvas");
            }
        }
    }

    bool verifyPixels(const std::span<const uint8_t> pixels)
    {
        checkSize(pixels);
        struct Sample
        {
            const char* name;
            uint32_t x;
            uint32_t y;
            int gray;
        };
        // Independent expectations: Rec.709 weights applied to the fixture colors.
        // Interior samples avoid texture filtering and rasterization boundaries.
        constexpr std::array<Sample, 7> samples{ {
            { "red", 40, 40, 54 },
            { "green", 280, 40, 182 },
            { "blue", 40, 200, 18 },
            { "white", 280, 200, 255 },
            { "yellow over red", 100, 90, 145 },
            { "cyan over white", 220, 150, 228 },
            { "cyan over yellow over red", 140, 110, 173 },
        } };
        bool passed = true;
        for(const auto& sample : samples) {
            const size_t offset = (sample.y * Scene::width + sample.x) * 4;
            const bool matches =
                std::abs(static_cast<int>(pixels[offset]) - sample.gray) <= 2 &&
                std::abs(static_cast<int>(pixels[offset + 1]) - sample.gray) <= 2 &&
                std::abs(static_cast<int>(pixels[offset + 2]) - sample.gray) <= 2 &&
                std::abs(static_cast<int>(pixels[offset + 3]) - 255) <= 2;
            if(!matches) {
                Logger::error("[sdlgpu] Verification failed for {} at ({}, {}): expected gray {} and alpha 255, got ({}, {}, {}, {})",
                    sample.name,
                    sample.x,
                    sample.y,
                    sample.gray,
                    pixels[offset],
                    pixels[offset + 1],
                    pixels[offset + 2],
                    pixels[offset + 3]);
                passed = false;
            }
        }
        if(passed) {
            Logger::info("[sdlgpu] Verification passed: seven grayscale/alpha samples, tolerance 2/255");
        }
        return passed;
    }

    void saveCapture(const std::span<const uint8_t> pixels, const std::string_view path)
    {
        checkSize(pixels);
        const std::filesystem::path output_path(std::u8string(path.begin(), path.end()));
        std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
        if(!output) {
            throw std::runtime_error("Cannot open capture path: " + std::string(path));
        }
        output << "P6\n"
               << Scene::width << ' ' << Scene::height << "\n255\n";
        std::array<char, Scene::width * 3> row{};
        for(uint32_t y = 0; y < Scene::height; ++y) {
            for(uint32_t x = 0; x < Scene::width; ++x) {
                for(size_t channel = 0; channel < 3; ++channel) {
                    row[x * 3 + channel] = static_cast<char>(pixels[(y * Scene::width + x) * 4 + channel]);
                }
            }
            output.write(row.data(), static_cast<std::streamsize>(row.size()));
        }
        output.close();
        if(!output) {
            throw std::runtime_error("Failed to write capture: " + std::string(path));
        }
        Logger::info("[sdlgpu] Saved grayscale canvas: {}", path);
    }
}
