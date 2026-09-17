#include "Renderer.hpp"
#include "SpriteShaders.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        constexpr uint32_t batch_capacity = 65535;
        constexpr uint32_t vertex_bytes = batch_capacity * sizeof(IRenderer::DrawVertex);
        constexpr uint32_t index_bytes = batch_capacity * sizeof(IRenderer::DrawIndex);

        SDL_FColor normalized(Color4B color)
        {
            return { color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f };
        }

        SDL_GPUColorTargetBlendState blendState(IRenderer::BlendState state)
        {
            using Blend = IRenderer::BlendState;
            SDL_GPUColorTargetBlendState result{};
            result.enable_blend = state != Blend::Disable;
            result.color_blend_op = result.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            result.src_color_blendfactor = result.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            result.dst_color_blendfactor = result.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            switch(state) {
                case Blend::Disable: break;
                case Blend::Alpha: break;
                case Blend::One:
                    result.dst_color_blendfactor = result.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                    break;
                case Blend::Min:
                case Blend::Max:
                    result.color_blend_op = result.alpha_blend_op = state == Blend::Min ? SDL_GPU_BLENDOP_MIN : SDL_GPU_BLENDOP_MAX;
                    result.dst_color_blendfactor = result.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                    break;
                case Blend::Mul:
                    result.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
                    result.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                    break;
                case Blend::Screen:
                    result.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
                    break;
                case Blend::Add:
                case Blend::Sub:
                case Blend::RevSub:
                    result.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                    if(state == Blend::Sub)
                        result.color_blend_op = SDL_GPU_BLENDOP_SUBTRACT;
                    if(state == Blend::RevSub)
                        result.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
                    break;
                case Blend::Inv:
                    result.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
                    result.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
                    result.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
                    result.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                    break;
                default: throw std::invalid_argument("Invalid sprite blend state");
            }
            return result;
        }

        bool unsupported(const char* feature)
        {
            Logger::error("[sdlgpu] {} is not supported in the core 2D milestone", feature);
            return false;
        }
    }

    Renderer::Renderer(Device* device)
        : m_device(device)
    {
        m_vertex_shader = device->compiler().compile(device->gpu(), shaders::sprite_vertex, SDL_GPU_SHADERSTAGE_VERTEX, "runtime sprite vertex");
        m_fragment_shader = device->compiler().compile(device->gpu(), shaders::sprite_fragment, SDL_GPU_SHADERSTAGE_FRAGMENT, "runtime sprite fragment");
        for(size_t i = 0; i < m_samplers.size(); ++i) {
            Graphics::SamplerState state;
            state.filer = i < 4 ? Filter::Point : Filter::Linear;
            state.address_u = state.address_v = i % 4 == 0 ? TextureAddressMode::Wrap : (i % 4 == 1 ? TextureAddressMode::Clamp : TextureAddressMode::Border);
            state.border_color = i % 4 == 3 ? BorderColor::White : BorderColor::Black;
            m_samplers[i].attach(new SDLGPU::SamplerState(device, state));
        }
        SmartReference<Texture2D> white;
        white.attach(new Texture2D(device, { 1, 1 }, false, false, false));
        const std::array<uint8_t, 4> pixel{ 255, 255, 255, 255 };
        white->uploadRgba(pixel);
        m_white = white.get();
        SDL_GPUBufferCreateInfo buffer{};
        buffer.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        buffer.size = vertex_bytes;
        m_vertex_buffer = { require(SDL_CreateGPUBuffer(device->gpu(), &buffer), "Create sprite vertex buffer"), { device->gpu() } };
        buffer.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        buffer.size = index_bytes;
        m_index_buffer = { require(SDL_CreateGPUBuffer(device->gpu(), &buffer), "Create sprite index buffer"), { device->gpu() } };
        SDL_GPUTransferBufferCreateInfo upload{};
        upload.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        upload.size = vertex_bytes + index_bytes;
        m_upload = { require(SDL_CreateGPUTransferBuffer(device->gpu(), &upload), "Create sprite upload buffer"), { device->gpu() } };
        m_vertices.reserve(batch_capacity);
        m_indices.reserve(batch_capacity);
        setOrtho({ 0, 0, 0, 640, 480, 1 });
        device->setRenderer(this);
    }
    Renderer::~Renderer()
    {
        m_device->setRenderer(nullptr);
    }

    SDL_GPUGraphicsPipeline* Renderer::pipeline()
    {
        const bool premultiplied = m_texture && m_texture->isPremultipliedAlpha();
        const PipelineKey key{ m_blend, m_depth_state, m_depth.get() != nullptr, m_color, m_fog, premultiplied };
        if(auto found = m_pipelines.find(key); found != m_pipelines.end())
            return found->second.get();
        const SDL_GPUVertexBufferDescription buffer{ 0, sizeof(DrawVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
        const SDL_GPUVertexAttribute attributes[]{
            { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(DrawVertex, x) },
            { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(DrawVertex, color) },
            { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(DrawVertex, u) },
        };
        SDL_GPUColorTargetDescription target{};
        target.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        target.blend_state = blendState(m_blend);
        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = m_vertex_shader.get();
        info.fragment_shader = m_fragment_shader.get();
        info.vertex_input_state = { &buffer, 1, attributes, 3 };
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.depth_stencil_state.enable_depth_test = m_depth && m_depth_state == DepthState::Enable;
        info.depth_stencil_state.enable_depth_write = info.depth_stencil_state.enable_depth_test;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        info.target_info.has_depth_stencil_target = m_depth.get() != nullptr;
        info.target_info.depth_stencil_format = DepthBuffer::format;
        Pipeline result(require(SDL_CreateGPUGraphicsPipeline(m_device->gpu(), &info), "Create sprite pipeline"), { m_device->gpu() });
        return m_pipelines.emplace(key, std::move(result)).first->second.get();
    }

    SDL_GPURenderPass* Renderer::beginPass(bool clear_color, Color4B color, bool clear_depth, float depth)
    {
        auto* frame = require(m_device->frame(), "Sprite render outside a frame");
        auto* target = require(m_target.get(), "Sprite render without a color attachment");
        SDL_GPUColorTargetInfo attachment{};
        attachment.texture = static_cast<Texture2D*>(target->getTexture())->handle();
        attachment.clear_color = normalized(color);
        attachment.load_op = clear_color ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        attachment.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth_attachment{};
        if(m_depth) {
            if(m_depth->getSize() != target->getTexture()->getSize())
                throw std::runtime_error("Color and depth attachment dimensions differ");
            depth_attachment.texture = static_cast<DepthBuffer*>(m_depth.get())->handle();
            depth_attachment.clear_depth = depth;
            depth_attachment.load_op = clear_depth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
            depth_attachment.store_op = SDL_GPU_STOREOP_STORE;
            depth_attachment.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            depth_attachment.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        }
        return frame->beginRenderPass({ &attachment, 1 }, m_depth ? &depth_attachment : nullptr);
    }

    bool Renderer::flush()
    {
        if(m_device->failed())
            return false;
        if(m_indices.empty()) {
            m_vertices.clear();
            return true;
        }
        try {
            auto* frame = require(m_device->frame(), "Flush sprites outside a frame");
            auto* texture = static_cast<Texture2D*>(m_texture ? m_texture.get() : m_white.get());
            if(m_target && m_target->getTexture() == texture)
                throw std::runtime_error("Cannot sample the active render target");
            auto* sampler = static_cast<SDLGPU::SamplerState*>(texture->getSamplerState());
            if(!sampler)
                sampler = static_cast<SDLGPU::SamplerState*>(m_samplers[size_t(IRenderer::SamplerState::LinearClamp)].get());
            auto* graphics_pipeline = pipeline();
            frame->endPass();
            auto* upload = static_cast<uint8_t*>(require(SDL_MapGPUTransferBuffer(m_device->gpu(), m_upload.get(), true), "Map sprite upload"));
            const auto used_vertices = static_cast<uint32_t>(m_vertices.size() * sizeof(DrawVertex));
            const auto used_indices = static_cast<uint32_t>(m_indices.size() * sizeof(DrawIndex));
            std::memcpy(upload, m_vertices.data(), used_vertices);
            std::memcpy(upload + vertex_bytes, m_indices.data(), used_indices);
            SDL_UnmapGPUTransferBuffer(m_device->gpu(), m_upload.get());
            m_device->copy([&](SDL_GPUCopyPass* pass) {
                const SDL_GPUTransferBufferLocation vertex_source{ m_upload.get(), 0 };
                const SDL_GPUTransferBufferLocation index_source{ m_upload.get(), vertex_bytes };
                const SDL_GPUBufferRegion vertex_target{ m_vertex_buffer.get(), 0, used_vertices };
                const SDL_GPUBufferRegion index_target{ m_index_buffer.get(), 0, used_indices };
                SDL_UploadToGPUBuffer(pass, &vertex_source, &vertex_target, true);
                SDL_UploadToGPUBuffer(pass, &index_source, &index_target, true);
            });
            auto* pass = beginPass();
            SDL_BindGPUGraphicsPipeline(pass, graphics_pipeline);
            const SDL_GPUBufferBinding vertex_binding{ m_vertex_buffer.get(), 0 };
            const SDL_GPUBufferBinding index_binding{ m_index_buffer.get(), 0 };
            SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);
            SDL_BindGPUIndexBuffer(pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
            const SDL_GPUViewport viewport{ m_viewport.a.x, m_viewport.a.y, m_viewport.b.x - m_viewport.a.x, m_viewport.b.y - m_viewport.a.y, m_viewport.a.z, m_viewport.b.z };
            SDL_SetGPUViewport(pass, &viewport);
            const auto size = m_target->getTexture()->getSize();
            const int left = std::clamp(static_cast<int>(m_scissor.a.x), 0, static_cast<int>(size.x));
            const int top = std::clamp(static_cast<int>(m_scissor.a.y), 0, static_cast<int>(size.y));
            const int right = std::clamp(static_cast<int>(m_scissor.b.x), left, static_cast<int>(size.x));
            const int bottom = std::clamp(static_cast<int>(m_scissor.b.y), top, static_cast<int>(size.y));
            const SDL_Rect scissor{ left, top, right - left, bottom - top };
            SDL_SetGPUScissor(pass, &scissor);
            SDL_PushGPUVertexUniformData(frame->command(), 0, &m_matrix, sizeof(m_matrix));
            const auto& description = sampler->description();
            const auto filter = description.filer;
            const bool min_linear = filter != Filter::Point && filter != Filter::PointMagLinear && filter != Filter::PointMipLinear && filter != Filter::LinearMinPoint;
            const bool mag_linear = filter != Filter::Point && filter != Filter::PointMinLinear && filter != Filter::PointMipLinear && filter != Filter::LinearMagPoint;
            const bool mip_linear = filter == Filter::Linear || filter == Filter::Anisotropic || filter == Filter::LinearMinPoint || filter == Filter::LinearMagPoint || filter == Filter::PointMipLinear;
            const bool white_border = description.border_color == BorderColor::White || description.border_color == BorderColor::TransparentWhite;
            const bool opaque_border = description.border_color == BorderColor::White || description.border_color == BorderColor::OpaqueBlack;
            struct Parameters
            {
                float eye[4];
                SDL_FColor fog;
                float fog_range[4];
                uint32_t modes[4];
                uint32_t addressing[4];
                float border[4];
                float lod[4];
            };
            const Parameters parameters{
                { m_eye.x, m_eye.y, m_eye.z, 0 }, normalized(m_fog_color), { m_fog_near, m_fog_far, 0, 0 }, { uint32_t(m_color), uint32_t(m_fog), texture->isPremultipliedAlpha() ? 1u : 0u, filter == Filter::Anisotropic ? std::clamp(description.max_anisotropy, 1u, 16u) : 1u }, { uint32_t(description.address_u), uint32_t(description.address_v), uint32_t(min_linear), uint32_t(mag_linear) }, { float(white_border), float(white_border), float(white_border), float(opaque_border) }, { description.mip_lod_bias, description.min_lod, description.max_lod, float(mip_linear) }
            };
            SDL_PushGPUFragmentUniformData(frame->command(), 0, &parameters, sizeof(parameters));
            const SDL_GPUTextureSamplerBinding binding{ texture->handle(), sampler->handle() };
            SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
            if(scissor.w && scissor.h)
                SDL_DrawGPUIndexedPrimitives(pass, static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);
            m_vertices.clear();
            m_indices.clear();
            return true;
        } catch(const std::exception& error) {
            m_device->fail(error);
            m_vertices.clear();
            m_indices.clear();
            return false;
        }
    }

    bool Renderer::beginBatch()
    {
        if(m_batch || !m_device->frame() || m_device->failed())
            return false;
        m_batch = true;
        return true;
    }
    bool Renderer::endBatch()
    {
        if(!m_batch)
            return false;
        m_batch = false;
        return flush();
    }
    void Renderer::clearRenderTarget(Color4B const& color)
    {
        if(!flush())
            return;
        try {
            beginPass(true, color);
        } catch(const std::exception& error) {
            m_device->fail(error);
        }
    }
    void Renderer::clearDepthBuffer(float depth)
    {
        if(!flush() || !m_depth)
            return;
        try {
            beginPass(false, {}, true, depth);
        } catch(const std::exception& error) {
            m_device->fail(error);
        }
    }
    void Renderer::setDefaultAttachment(IRenderTarget* target, IDepthStencilBuffer* depth)
    {
        m_default_target = target;
        m_default_depth = depth;
        setRenderAttachment(target, depth);
    }
    void Renderer::setRenderAttachment(IRenderTarget* target, IDepthStencilBuffer* depth)
    {
        if(!flush())
            return;
        if(auto* frame = m_device->frame())
            frame->endPass();
        m_target = target ? target : m_default_target.get();
        m_depth = target ? depth : m_default_depth.get();
    }
    void Renderer::setOrtho(BoxF const& box)
    {
        if(!flush())
            return;
        DirectX::XMStoreFloat4x4(&m_matrix, DirectX::XMMatrixOrthographicOffCenterLH(box.a.x, box.b.x, box.b.y, box.a.y, box.a.z, box.b.z));
    }
    void Renderer::setPerspective(Vector3F const& eye, Vector3F const& lookat, Vector3F const& up, float fov, float aspect, float near_plane, float far_plane)
    {
        if(!flush())
            return;
        m_eye = eye;
        const auto view = DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(eye.x, eye.y, eye.z, 0),
            DirectX::XMVectorSet(lookat.x, lookat.y, lookat.z, 0),
            DirectX::XMVectorSet(up.x, up.y, up.z, 0));
        DirectX::XMStoreFloat4x4(&m_matrix, view * DirectX::XMMatrixPerspectiveFovLH(fov, aspect, near_plane, far_plane));
    }
    void Renderer::setViewport(BoxF const& box)
    {
        if(flush())
            m_viewport = box;
    }
    void Renderer::setScissorRect(RectF const& rectangle)
    {
        if(flush())
            m_scissor = rectangle;
    }
    void Renderer::setViewportAndScissorRect()
    {
        flush();
    } // Every sprite pass reapplies both states.
    void Renderer::setVertexColorBlendState(VertexColorBlendState state)
    {
        if(state != m_color && flush())
            m_color = state;
    }
    void Renderer::setFogState(FogState state, Color4B const& color, float density_or_near, float far_plane)
    {
        if(!flush())
            return;
        m_fog = state;
        m_fog_color = color;
        m_fog_near = density_or_near;
        m_fog_far = far_plane;
    }
    void Renderer::setDepthState(DepthState state)
    {
        if(state != m_depth_state && flush())
            m_depth_state = state;
    }
    void Renderer::setBlendState(BlendState state)
    {
        if(state != m_blend && flush())
            m_blend = state;
    }
    void Renderer::setTexture(ITexture2D* texture)
    {
        if(texture != m_texture.get() && flush())
            m_texture = texture;
    }
    bool Renderer::drawRequest(uint16_t vertices, uint16_t indices, DrawVertex** output_vertices, DrawIndex** output_indices, uint16_t* offset)
    {
        if(!m_batch || !output_vertices || !output_indices || !offset || !vertices || !indices)
            return false;
        if((m_vertices.size() + vertices > batch_capacity || m_indices.size() + indices > batch_capacity) && !flush())
            return false;
        const auto old_vertices = m_vertices.size();
        const auto old_indices = m_indices.size();
        m_vertices.resize(old_vertices + vertices);
        m_indices.resize(old_indices + indices);
        *output_vertices = m_vertices.data() + old_vertices;
        *output_indices = m_indices.data() + old_indices;
        *offset = static_cast<uint16_t>(old_vertices);
        return true;
    }
    bool Renderer::drawRaw(DrawVertex const* vertices, uint16_t vertex_count, DrawIndex const* indices, uint16_t index_count)
    {
        if(!vertices || !indices)
            return false;
        for(uint16_t i = 0; i < index_count; ++i)
            if(indices[i] >= vertex_count)
                return false;
        DrawVertex* output_vertices{};
        DrawIndex* output_indices{};
        uint16_t offset{};
        if(!drawRequest(vertex_count, index_count, &output_vertices, &output_indices, &offset))
            return false;
        std::copy_n(vertices, vertex_count, output_vertices);
        for(uint16_t i = 0; i < index_count; ++i) output_indices[i] = indices[i] + offset;
        return true;
    }
    bool Renderer::drawTriangle(DrawVertex const& a, DrawVertex const& b, DrawVertex const& c)
    {
        const DrawVertex vertices[]{ a, b, c };
        return drawTriangle(vertices);
    }
    bool Renderer::drawTriangle(DrawVertex const* vertices)
    {
        constexpr DrawIndex indices[]{ 0, 1, 2 };
        return drawRaw(vertices, 3, indices, 3);
    }
    bool Renderer::drawQuad(DrawVertex const& a, DrawVertex const& b, DrawVertex const& c, DrawVertex const& d)
    {
        const DrawVertex vertices[]{ a, b, c, d };
        return drawQuad(vertices);
    }
    bool Renderer::drawQuad(DrawVertex const* vertices)
    {
        constexpr DrawIndex indices[]{ 0, 1, 2, 0, 2, 3 };
        return drawRaw(vertices, 4, indices, 6);
    }
    ISamplerState* Renderer::getKnownSamplerState(IRenderer::SamplerState state)
    {
        const auto index = static_cast<size_t>(state);
        return index < m_samplers.size() ? m_samplers[index].get() : nullptr;
    }
    bool Renderer::createPostEffectShader(StringView, IPostEffectShader** output)
    {
        if(output)
            *output = nullptr;
        return unsupported("Post-effect shaders");
    }
    bool Renderer::createPostEffectShaderFromSource(StringView, IPostEffectShader** output)
    {
        if(output)
            *output = nullptr;
        return unsupported("Post-effect shaders");
    }
    bool Renderer::drawPostEffect(IPostEffectShader*, BlendState, ITexture2D*, IRenderer::SamplerState, Vector4F const*, size_t, ITexture2D* const*, IRenderer::SamplerState const*, size_t)
    {
        return unsupported("Post-effects");
    }
    bool Renderer::drawPostEffect(IPostEffectShader*, BlendState)
    {
        return unsupported("Post-effects");
    }
    bool Renderer::createModel(StringView, IModel** output)
    {
        if(output)
            *output = nullptr;
        return unsupported("Models");
    }
    bool Renderer::drawModel(IModel*)
    {
        return unsupported("Models");
    }
}

namespace core::Graphics
{
    bool IRenderer::create(IDevice* device, IRenderer** output)
    {
        if(!device || !output)
            return false;
        *output = nullptr;
        try {
            *output = new SDLGPU::Renderer(static_cast<SDLGPU::Device*>(device));
            return true;
        } catch(const std::exception& error) {
            Logger::error("[sdlgpu] Create renderer: {}", error.what());
            return false;
        }
    }
}
