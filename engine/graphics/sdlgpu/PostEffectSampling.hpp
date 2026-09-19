#pragma once

#include "Core/Graphics/Device.hpp"
#include "PostEffectReflection.hpp"
#include <array>
#include <span>

namespace core::Graphics::SDLGPU
{
    inline constexpr uint32_t effect_sampler_register = 65535;
    inline constexpr char effect_sampler_buffer[] = "_luastg_effect_samplers";
    struct EffectSamplingParameters
    {
        uint32_t addressing[4]{};
        uint32_t filtering[4]{};
        float border[4]{};
        float lod[4]{};
    };
    static_assert(sizeof(EffectSamplingParameters) == 64);
    EffectSamplingParameters effectSamplingParameters(Graphics::SamplerState const& description);
    std::string addPostEffectSampling(std::string_view source, EffectLayout const& layout);
    void setEffectSampling(std::span<EffectBuffer> buffers, std::span<const EffectSamplingParameters> parameters);
}
