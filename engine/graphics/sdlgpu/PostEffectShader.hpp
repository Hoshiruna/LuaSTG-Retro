#pragma once

#include "Device.hpp"
#include "PostEffectReflection.hpp"
#include "PostEffectSampling.hpp"
#include <map>
#include <tuple>

namespace core::Graphics::SDLGPU
{
    struct EffectBindings
    {
        std::vector<SDL_GPUTextureSamplerBinding> textures;
        std::vector<EffectSamplingParameters> sampling;
    };
    class PostEffectShader final : public implement::ReferenceCounted<IPostEffectShader>
    {
    public:
        PostEffectShader(Device* device, std::string_view source, std::string_view name);
        bool setFloat(StringView name, float value) override;
        bool setFloat2(StringView name, Vector2F value) override;
        bool setFloat3(StringView name, Vector3F value) override;
        bool setFloat4(StringView name, Vector4F value) override;
        bool setTexture2D(StringView name, ITexture2D* texture) override;
        bool apply(IRenderer* renderer) override;

        Device* device() const noexcept { return m_device.get(); }
        std::string const& name() const noexcept { return m_name; }
        EffectLayout const& layout() const noexcept { return m_layout; }
        EffectBindings bindings(ISamplerState* fallback, ITexture2D* destination) const;
        SDL_GPUGraphicsPipeline* pipeline(SDL_GPUShader* vertex, IRenderer::BlendState blend, SDL_GPUColorTargetBlendState state, bool depth);

    private:
        bool setValue(StringView name, const float* value, uint32_t components);
        SmartReference<Device> m_device;
        std::string m_name;
        std::string m_source;
        EffectLayout m_layout;
        std::vector<SmartReference<ITexture2D>> m_textures;
        Shader m_shader;
        using PipelineKey = std::tuple<IRenderer::BlendState, SDL_GPUTextureFormat, bool, SDL_GPUTextureFormat>;
        std::map<PipelineKey, Pipeline> m_pipelines;
    };
}
