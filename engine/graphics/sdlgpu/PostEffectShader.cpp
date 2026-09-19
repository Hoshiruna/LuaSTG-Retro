#include "PostEffectShader.hpp"
#include "Renderer.hpp"
#include "ShaderSource.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cstring>

namespace core::Graphics::SDLGPU
{
    PostEffectShader::PostEffectShader(Device* device, std::string_view source, std::string_view name)
        : m_device(device), m_name(name.empty() ? "<memory>" : name), m_source(expandShaderIncludes(source, m_name))
    {
        Logger::info("[sdlgpu] Compiling post-effect: {}", m_name);
        auto spirv = device->compiler().compileLegacy(m_source, m_name);
        m_layout = normalizePostEffect(spirv);
        if(!m_layout.textures.empty()) {
            const auto sampled = addPostEffectSampling(m_source, m_layout);
            spirv = device->compiler().compileLegacy(sampled, m_name);
            auto sampled_layout = normalizePostEffect(spirv);
            if(sampled_layout.textures != m_layout.textures)
                throw std::runtime_error(m_name + ": texture bindings changed while adding sampler support");
            for(auto const& original : m_layout.buffers) {
                auto found = std::find_if(sampled_layout.buffers.begin(), sampled_layout.buffers.end(), [&](auto const& buffer) { return buffer.name == original.name; });
                if(found == sampled_layout.buffers.end() || found->variables != original.variables || found->bytes.size() != original.bytes.size())
                    throw std::runtime_error(m_name + ": shader constant layout changed while adding sampler support");
            }
            m_layout = std::move(sampled_layout);
        }
        m_shader = device->compiler().create(device->gpu(), spirv, SDL_GPU_SHADERSTAGE_FRAGMENT, m_name.c_str());
        m_textures.resize(m_layout.textures.size());
    }

    bool PostEffectShader::setValue(StringView name, const float* value, uint32_t components)
    {
        for(auto& buffer : m_layout.buffers) {
            for(auto const& variable : buffer.variables) {
                if(variable.name != name)
                    continue;
                if(variable.components != components || variable.size != components * sizeof(float)) {
                    Logger::error("[sdlgpu] {}: parameter '{}' expects {} float components (0 means a non-scalar/vector type), received {}",
                        m_name,
                        name,
                        variable.components,
                        components);
                    return false;
                }
                std::memcpy(buffer.bytes.data() + variable.offset, value, variable.size);
                return true;
            }
        }
        Logger::error("[sdlgpu] {}: no constant named '{}'", m_name, name);
        return false;
    }
    bool PostEffectShader::setFloat(StringView name, float value)
    { return setValue(name, &value, 1); }
    bool PostEffectShader::setFloat2(StringView name, Vector2F value)
    {
        const float data[]{ value.x, value.y };
        return setValue(name, data, 2);
    }
    bool PostEffectShader::setFloat3(StringView name, Vector3F value)
    {
        const float data[]{ value.x, value.y, value.z };
        return setValue(name, data, 3);
    }
    bool PostEffectShader::setFloat4(StringView name, Vector4F value)
    {
        const float data[]{ value.x, value.y, value.z, value.w };
        return setValue(name, data, 4);
    }
    bool PostEffectShader::setTexture2D(StringView name, ITexture2D* texture)
    {
        auto* own_texture = dynamic_cast<Texture2D*>(texture);
        if(!own_texture || own_texture->device() != m_device.get()) {
            Logger::error("[sdlgpu] {}: '{}' requires a texture from this device", m_name, name);
            return false;
        }
        for(auto const& entry : m_layout.textures) {
            if(entry.name == name) {
                m_textures[entry.slot] = texture;
                return true;
            }
        }
        Logger::error("[sdlgpu] {}: no Texture2D named '{}'", m_name, name);
        return false;
    }

    EffectBindings PostEffectShader::bindings(ISamplerState* fallback, ITexture2D* destination) const
    {
        EffectBindings result;
        for(auto const& entry : m_layout.textures) {
            auto* texture = static_cast<Texture2D*>(m_textures[entry.slot].get());
            if(!texture)
                throw std::runtime_error(m_name + ": no texture assigned to " + entry.name);
            if(texture == destination)
                throw std::runtime_error(m_name + ": cannot sample the destination texture (" + entry.name + ")");
            auto* sampler = dynamic_cast<SamplerState*>(texture->getSamplerState() ? texture->getSamplerState() : fallback);
            if(!sampler)
                throw std::runtime_error(m_name + ": invalid sampler for " + entry.name);
            result.textures.push_back({ texture->handle(), sampler->handle() });
            result.sampling.push_back(effectSamplingParameters(sampler->description()));
        }
        return result;
    }

    bool PostEffectShader::apply(IRenderer* renderer)
    {
        auto* own_renderer = dynamic_cast<Renderer*>(renderer);
        return own_renderer && own_renderer->applyPostEffect(*this);
    }

    SDL_GPUGraphicsPipeline* PostEffectShader::pipeline(SDL_GPUShader* vertex, IRenderer::BlendState blend, SDL_GPUColorTargetBlendState state, bool depth)
    {
        const PipelineKey key{ blend, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, depth, DepthBuffer::format };
        if(auto found = m_pipelines.find(key); found != m_pipelines.end())
            return found->second.get();
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        target.blend_state = state;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex;
        info.fragment_shader = m_shader.get();
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        info.target_info.has_depth_stencil_target = depth;
        info.target_info.depth_stencil_format = DepthBuffer::format;
        Pipeline value(require(SDL_CreateGPUGraphicsPipeline(m_device->gpu(), &info), "Create post-effect pipeline " + m_name), { m_device->gpu() });
        return m_pipelines.emplace(key, std::move(value)).first->second.get();
    }
}
