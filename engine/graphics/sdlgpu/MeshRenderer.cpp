#include "Mesh.hpp"
#include "MeshShaders.hpp"
#include "Renderer.hpp"
#include "core/Logger.hpp"

namespace core::Graphics::SDLGPU
{
    SDL_GPUGraphicsPipeline* Renderer::meshPipeline(Mesh const& mesh)
    {
        const auto& data = mesh.data();
        const auto& options = data.options;
        const PipelineKey state{ m_blend, m_depth_state, m_depth.get() != nullptr, m_color, m_fog, m_texture && m_texture->isPremultipliedAlpha() };
        const MeshPipelineKey key{ state, options.vertex_position_no_z, options.vertex_color_compression, options.primitive_topology };
        if(auto found = m_mesh_pipelines.find(key); found != m_mesh_pipelines.end())
            return found->second.get();
        auto& shader = m_mesh_shaders[size_t(options.vertex_position_no_z) * 2 + size_t(options.vertex_color_compression)];
        if(!shader) {
            const auto source = shaders::meshVertex(options.vertex_position_no_z, options.vertex_color_compression);
            shader = m_device->compiler().compile(m_device->gpu(), source.c_str(), SDL_GPU_SHADERSTAGE_VERTEX, "runtime mesh vertex");
        }
        const SDL_GPUVertexBufferDescription buffer{ 0, data.stride, SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
        const SDL_GPUVertexAttribute attributes[]{
            { 0, 0, options.vertex_position_no_z ? SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2 : SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 },
            { 1, 0, options.vertex_color_compression ? SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM : SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, data.color_offset },
            { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, data.uv_offset },
        };
        SDL_GPUColorTargetDescription color{};
        color.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        color.blend_state = blendState(m_blend);
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = shader.get();
        info.fragment_shader = m_fragment_shader.get();
        info.vertex_input_state = { &buffer, 1, attributes, 3 };
        info.primitive_type = options.primitive_topology == PrimitiveTopology::triangle_list ? SDL_GPU_PRIMITIVETYPE_TRIANGLELIST : SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = m_depth && m_depth_state == DepthState::Enable;
        info.depth_stencil_state.enable_depth_write = info.depth_stencil_state.enable_depth_test;
        info.target_info.color_target_descriptions = &color;
        info.target_info.num_color_targets = 1;
        info.target_info.has_depth_stencil_target = m_depth.get() != nullptr;
        info.target_info.depth_stencil_format = DepthBuffer::format;
        Pipeline pipeline(require(SDL_CreateGPUGraphicsPipeline(m_device->gpu(), &info), "Create mesh pipeline"), { m_device->gpu() });
        return m_mesh_pipelines.emplace(key, std::move(pipeline)).first->second.get();
    }

    bool Renderer::drawMesh(Mesh& mesh, Matrix4F const& transform)
    {
        if(!m_batch || !flush())
            return false;
        try {
            if(mesh.device() != m_device.get())
                throw std::invalid_argument("Mesh belongs to another device");
            if(!mesh.getVertexCount())
                return true;
            auto* texture = static_cast<Texture2D*>(require(m_texture.get(), "Mesh texture missing"));
            if(m_target && m_target->getTexture() == texture)
                throw std::invalid_argument("Cannot sample the active render target");
            auto* sampler = static_cast<SDLGPU::SamplerState*>(texture->getSamplerState());
            if(!sampler)
                sampler = static_cast<SDLGPU::SamplerState*>(getKnownSamplerState(IRenderer::SamplerState::LinearWrap));
            auto* pipeline = meshPipeline(mesh);
            auto* pass = beginPass();
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            const SDL_GPUBufferBinding vertices{ mesh.vertices(), 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
            if(mesh.getIndexCount()) {
                const SDL_GPUBufferBinding indices{ mesh.indices(), 0 };
                SDL_BindGPUIndexBuffer(pass, &indices, mesh.data().options.vertex_index_compression ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
            }
            auto* command = m_device->frame()->command();
            SDL_PushGPUVertexUniformData(command, 0, &m_matrix, sizeof(m_matrix));
            SDL_PushGPUVertexUniformData(command, 1, &transform, sizeof(transform));
            bindSpriteParameters(pass, texture, sampler);
            const auto clip = applyViewport(pass);
            if(clip.w && clip.h) {
                if(mesh.getIndexCount())
                    SDL_DrawGPUIndexedPrimitives(pass, mesh.getIndexCount(), 1, 0, 0, 0);
                else
                    SDL_DrawGPUPrimitives(pass, mesh.getVertexCount(), 1, 0, 0);
            }
            m_device->frame()->endPass();
            return true;
        } catch(std::exception const& error) {
            m_device->fail(error);
            return false;
        }
    }
}
