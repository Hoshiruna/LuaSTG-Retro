#include "Model.hpp"
#include "ModelShaders.hpp"
#include "Renderer.hpp"
#include "core/Logger.hpp"

namespace core::Graphics::SDLGPU
{
    bool Renderer::createModel(StringView path, IModel** output)
    {
        if(!output)
            return false;
        *output = nullptr;
        try {
            *output = new Model(m_device.get(), path);
            return true;
        } catch(std::exception const& error) {
            Logger::error("[sdlgpu] Load model '{}': {}", path, error.what());
            return false;
        }
    }
    SDL_GPUGraphicsPipeline* Renderer::modelPipeline(ModelPrimitive const& primitive)
    {
        const ModelPipelineKey key{ primitive.topology, primitive.double_sided, m_depth.get() != nullptr, primitive.alpha_mode, m_fog };
        if(auto found = m_model_pipelines.find(key); found != m_model_pipelines.end())
            return found->second.get();
        if(!m_model_vertex)
            m_model_vertex = m_device->compiler().compile(m_device->gpu(), shaders::model_vertex, SDL_GPU_SHADERSTAGE_VERTEX, "runtime model vertex");
        if(!m_model_fragment)
            m_model_fragment = m_device->compiler().compile(m_device->gpu(), shaders::model_fragment, SDL_GPU_SHADERSTAGE_FRAGMENT, "runtime model fragment");
        const SDL_GPUVertexBufferDescription buffer{ 0, sizeof(ModelVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
        const SDL_GPUVertexAttribute attributes[]{
            { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, position) },
            { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(ModelVertex, normal) },
            { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(ModelVertex, color) },
            { 3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(ModelVertex, uv) },
        };
        SDL_GPUColorTargetDescription color{};
        color.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = m_model_vertex.get();
        info.fragment_shader = m_model_fragment.get();
        info.vertex_input_state = { &buffer, 1, attributes, 4 };
        switch(primitive.topology) {
            case 0: info.primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST; break;
            case 1: info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST; break;
            case 3: info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINESTRIP; break;
            case 5: info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP; break;
            default: info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST; break;
        }
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = primitive.double_sided ? SDL_GPU_CULLMODE_NONE : SDL_GPU_CULLMODE_BACK;
        info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = m_depth.get() != nullptr;
        info.depth_stencil_state.enable_depth_write = info.depth_stencil_state.enable_depth_test;
        info.target_info.color_target_descriptions = &color;
        info.target_info.num_color_targets = 1;
        info.target_info.has_depth_stencil_target = m_depth.get() != nullptr;
        info.target_info.depth_stencil_format = DepthBuffer::format;
        Pipeline pipeline(require(SDL_CreateGPUGraphicsPipeline(m_device->gpu(), &info), "Create model pipeline"), { m_device->gpu() });
        return m_model_pipelines.emplace(key, std::move(pipeline)).first->second.get();
    }
    bool Renderer::drawModel(IModel* resource)
    {
        if(!resource || !m_batch || !flush())
            return false;
        try {
            auto& model = *static_cast<Model*>(resource);
            if(model.device() != m_device.get())
                throw std::invalid_argument("Model belongs to another device");
            for(uint32_t mode = 0; mode < 3; ++mode)
                for(size_t i = 0; i < model.data.primitives.size(); ++i) {
                    auto const& primitive = model.data.primitives[i];
                    if(primitive.alpha_mode != mode || primitive.vertices.empty())
                        continue;
                    auto* pipeline = modelPipeline(primitive);
                    auto* pass = beginPass();
                    SDL_BindGPUGraphicsPipeline(pass, pipeline);
                    const SDL_GPUBufferBinding vertices{ model.buffers[i].vertices.get(), 0 };
                    SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
                    if(primitive.index_count) {
                        const SDL_GPUBufferBinding indices{ model.buffers[i].indices.get(), 0 };
                        SDL_BindGPUIndexBuffer(pass, &indices, primitive.index32 ? SDL_GPU_INDEXELEMENTSIZE_32BIT : SDL_GPU_INDEXELEMENTSIZE_16BIT);
                    }
                    auto* texture = primitive.image >= 0 ? model.images[size_t(primitive.image)].get() : static_cast<Texture2D*>(m_white.get());
                    auto* sampler = primitive.sampler >= 0 ? model.samplers[size_t(primitive.sampler)].get() : static_cast<SDLGPU::SamplerState*>(getKnownSamplerState(IRenderer::SamplerState::LinearWrap));
                    const SDL_GPUTextureSamplerBinding binding{ texture->handle(), sampler->handle() };
                    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
                    auto* command = m_device->frame()->command();
                    SDL_PushGPUVertexUniformData(command, 0, &m_matrix, sizeof(m_matrix));
                    struct Transform
                    {
                        DirectX::XMFLOAT4X4 world, normal;
                    } transform;
                    auto matrix = DirectX::XMLoadFloat4x4(&primitive.local) * model.transform();
                    DirectX::XMStoreFloat4x4(&transform.world, matrix);
                    matrix.r[3] = DirectX::g_XMIdentityR3;
                    DirectX::XMStoreFloat4x4(&transform.normal, DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, matrix)));
                    SDL_PushGPUVertexUniformData(command, 1, &transform, sizeof(transform));
                    struct Parameters
                    {
                        Vector4F eye, fog, range, base, ambient, direction, light;
                        uint32_t modes[4];
                        Vector4F alpha;
                    };
                    const Parameters parameters{
                        { m_eye.x, m_eye.y, m_eye.z, 0 },
                        { m_fog_color.r / 255.0f, m_fog_color.g / 255.0f, m_fog_color.b / 255.0f, m_fog_color.a / 255.0f },
                        { m_fog_near, m_fog_far, 0, 0 },
                        primitive.base_color,
                        model.ambient,
                        model.light_direction,
                        model.light_color,
                        { uint32_t(m_fog), mode, 0, 0 },
                        { primitive.alpha_cutoff, 0, 0, 0 }
                    };
                    SDL_PushGPUFragmentUniformData(command, 0, &parameters, sizeof(parameters));
                    const auto clip = applyViewport(pass);
                    if(clip.w && clip.h) {
                        if(primitive.index_count)
                            SDL_DrawGPUIndexedPrimitives(pass, primitive.index_count, 1, 0, 0, 0);
                        else
                            SDL_DrawGPUPrimitives(pass, uint32_t(primitive.vertices.size()), 1, 0, 0);
                    }
                }
            m_device->frame()->endPass();
            return true;
        } catch(std::exception const& error) {
            m_device->fail(error);
            return false;
        }
    }
}
