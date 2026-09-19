#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace core::Graphics::SDLGPU
{
    struct EffectVariable
    {
        std::string name;
        uint32_t offset{};
        uint32_t size{};
        uint32_t components{};
        bool operator==(EffectVariable const&) const = default;
    };
    struct EffectBuffer
    {
        std::string name;
        uint32_t legacy_slot{};
        uint32_t slot{};
        std::vector<uint8_t> bytes;
        std::vector<EffectVariable> variables;
        bool active{ true };
    };
    struct EffectTexture
    {
        std::string name;
        uint32_t legacy_slot{};
        uint32_t slot{};
        std::string sampler_name;
        bool operator==(EffectTexture const&) const = default;
    };
    struct EffectLayout
    {
        std::vector<EffectBuffer> buffers;
        std::vector<EffectTexture> textures;
    };

    // Only binding and interface decorations change; member names and offsets stay intact.
    EffectLayout normalizePostEffect(std::vector<uint32_t>& words);
}
