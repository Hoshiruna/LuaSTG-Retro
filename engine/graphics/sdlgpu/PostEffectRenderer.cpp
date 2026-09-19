#include "Renderer.hpp"
#include "PostEffectShader.hpp"
#include "ShaderSource.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cstring>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        constexpr char effect_vertex[] = R"hlsl(
struct Output {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
Output main(uint id : SV_VertexID) {
    float2 uv = float2((id << 1) & 2, id & 2);
    Output output;
    output.position = float4(uv.x * 2 - 1, 1 - uv.y * 2, 0.5, 1);
    output.uv = uv;
    output.color = 1;
    return output;
}
)hlsl";
    }

    bool Renderer::createPostEffectShader(StringView path, IPostEffectShader** output)
    {
        if(!output)
            return false;
        *output = nullptr;
        try {
            *output = new PostEffectShader(m_device.get(), readShaderSource(path), path);
            return true;
        } catch(std::exception const& error) {
            Logger::error("[sdlgpu] Load post-effect {}: {}", path, error.what());
            return false;
        }
    }

    bool Renderer::createPostEffectShaderFromSource(StringView source, IPostEffectShader** output, StringView source_name)
    {
        if(!output)
            return false;
        *output = nullptr;
        try {
            *output = new PostEffectShader(m_device.get(), source, source_name);
            return true;
        } catch(std::exception const& error) {
            Logger::error("[sdlgpu] Compile post-effect: {}", error.what());
            return false;
        }
    }

    bool Renderer::applyPostEffect(PostEffectShader& shader)
    {
        if(!m_effect_pass || shader.device() != m_device.get() || !m_device->frame())
            return false;
        auto bindings = shader.bindings(getKnownSamplerState(IRenderer::SamplerState::LinearClamp), m_target->getTexture());
        auto buffers = shader.layout().buffers;
        setEffectSampling(buffers, bindings.sampling);
        for(auto const& buffer : buffers) {
            if(buffer.active)
                SDL_PushGPUFragmentUniformData(m_device->frame()->command(), buffer.slot, buffer.bytes.data(), static_cast<uint32_t>(buffer.bytes.size()));
        }
        if(!bindings.textures.empty())
            SDL_BindGPUFragmentSamplers(m_effect_pass, 0, bindings.textures.data(), static_cast<uint32_t>(bindings.textures.size()));
        return true;
    }

    bool Renderer::drawEffect(PostEffectShader& shader, BlendState blend, std::span<const SDL_GPUTextureSamplerBinding> bindings, std::span<const EffectBuffer> buffers)
    {
        if(!m_batch || !m_device->frame() || !m_target || m_device->failed())
            throw std::runtime_error("Post-effect requires an active batch and render target");
        if(shader.device() != m_device.get())
            throw std::runtime_error("Post-effect belongs to another graphics device");
        if(!flush())
            return false;
        auto* frame = m_device->frame();
        frame->endPass();
        if(!m_effect_vertex_shader)
            m_effect_vertex_shader = m_device->compiler().compile(m_device->gpu(), effect_vertex, SDL_GPU_SHADERSTAGE_VERTEX, "post-effect fullscreen vertex");
        auto* pipeline = shader.pipeline(m_effect_vertex_shader.get(), blend, blendState(blend), m_depth.get() != nullptr);
        auto* pass = beginPass();
        m_effect_pass = pass;
        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        auto const size = m_target->getTexture()->getSize();
        const SDL_GPUViewport viewport{ 0, 0, float(size.x), float(size.y), 0, 1 };
        const SDL_Rect scissor{ 0, 0, static_cast<int>(size.x), static_cast<int>(size.y) };
        SDL_SetGPUViewport(pass, &viewport);
        SDL_SetGPUScissor(pass, &scissor);
        for(auto const& buffer : buffers) {
            if(buffer.active)
                SDL_PushGPUFragmentUniformData(frame->command(), buffer.slot, buffer.bytes.data(), static_cast<uint32_t>(buffer.bytes.size()));
        }
        if(!bindings.empty())
            SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), static_cast<uint32_t>(bindings.size()));
        if(bindings.empty() && buffers.empty() && !shader.apply(this))
            throw std::runtime_error("Bind named post-effect parameters");
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        m_effect_pass = nullptr;
        frame->endPass();
        // Sprite state is retained on the CPU and applied by the next flush.
        return true;
    }

    bool Renderer::drawPostEffect(IPostEffectShader* effect, BlendState blend)
    {
        try {
            auto* shader = dynamic_cast<PostEffectShader*>(effect);
            if(!shader || !m_target)
                throw std::runtime_error("Invalid post-effect or destination");
            return drawEffect(*shader, blend, {}, {});
        } catch(std::exception const& error) {
            m_effect_pass = nullptr;
            if(m_device->frame())
                m_device->frame()->endPass();
            Logger::error("[sdlgpu] Draw post-effect: {}", error.what());
            return false;
        }
    }

    bool Renderer::drawPostEffect(IPostEffectShader* effect, BlendState blend, ITexture2D* screen, IRenderer::SamplerState screen_sampler, Vector4F const* constants, size_t constant_count, ITexture2D* const* textures, IRenderer::SamplerState const* samplers, size_t texture_count)
    {
        try {
            auto* shader = dynamic_cast<PostEffectShader*>(effect);
            if(!shader || !m_target || constant_count > 8 || texture_count > 4 || (constant_count && !constants) || (texture_count && (!textures || !samplers)))
                throw std::runtime_error("Invalid legacy post-effect arguments");
            std::vector<SDL_GPUTextureSamplerBinding> bindings;
            std::vector<EffectSamplingParameters> sampling;
            for(auto const& entry : shader->layout().textures) {
                auto* raw_texture = entry.legacy_slot == 4 ? screen : (entry.legacy_slot < texture_count ? textures[entry.legacy_slot] : nullptr);
                auto* texture = dynamic_cast<Texture2D*>(raw_texture);
                auto const mode = entry.legacy_slot == 4 ? screen_sampler : (entry.legacy_slot < texture_count ? samplers[entry.legacy_slot] : IRenderer::SamplerState::LinearClamp);
                auto* sampler = static_cast<SDLGPU::SamplerState*>(getKnownSamplerState(mode));
                if(!texture || texture->device() != m_device.get() || !sampler)
                    throw std::runtime_error(shader->name() + ": missing texture or invalid sampler for " + entry.name);
                if(texture == m_target->getTexture())
                    throw std::runtime_error(shader->name() + ": cannot sample the active destination");
                bindings.push_back({ texture->handle(), sampler->handle() });
                sampling.push_back(effectSamplingParameters(sampler->description()));
            }
            auto buffers = shader->layout().buffers;
            const auto size = m_target->getTexture()->getSize();
            const float engine_data[]{ float(size.x), float(size.y), 0, 0, m_viewport.a.x, m_viewport.a.y, m_viewport.b.x, m_viewport.b.y };
            for(auto& buffer : buffers) {
                if(buffer.legacy_slot == effect_sampler_register)
                    continue;
                std::fill(buffer.bytes.begin(), buffer.bytes.end(), 0);
                if(buffer.legacy_slot == 0) {
                    if(buffer.bytes.size() > 8 * sizeof(Vector4F))
                        throw std::runtime_error("Legacy b0 exceeds eight float4 values");
                    if(constant_count)
                        std::memcpy(buffer.bytes.data(), constants, (std::min)(buffer.bytes.size(), constant_count * sizeof(Vector4F)));
                } else if(buffer.legacy_slot == 1) {
                    if(buffer.bytes.size() > sizeof(engine_data))
                        throw std::runtime_error("Legacy b1 exceeds the engine data layout");
                    std::memcpy(buffer.bytes.data(), engine_data, buffer.bytes.size());
                } else {
                    throw std::runtime_error("Positional PostEffect supports only b0 and b1");
                }
            }
            setEffectSampling(buffers, sampling);
            return drawEffect(*shader, blend, bindings, buffers);
        } catch(std::exception const& error) {
            m_effect_pass = nullptr;
            if(m_device->frame())
                m_device->frame()->endPass();
            Logger::error("[sdlgpu] Draw legacy post-effect: {}", error.what());
            return false;
        }
    }
}
