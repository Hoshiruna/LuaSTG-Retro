#pragma once

#include "Device.hpp"
#include "PostEffectReflection.hpp"
#include "Core/Graphics/Mesh.hpp"
#include <DirectXMath.h>
#include <array>
#include <map>
#include <tuple>

namespace core::Graphics::SDLGPU
{
    class PostEffectShader;
    class Mesh;
    struct ModelPrimitive;
    class Renderer final : public implement::ReferenceCounted<IRenderer>
    {
    public:
        explicit Renderer(Device* device);
        ~Renderer() override;
        bool beginBatch() override;
        bool endBatch() override;
        bool isBatchScope() override { return m_batch; }
        bool flush() override;
        void clearRenderTarget(Color4B const& color) override;
        void clearDepthBuffer(float depth) override;
        void setRenderAttachment(IRenderTarget* target, IDepthStencilBuffer* depth) override;
        void setOrtho(BoxF const& box) override;
        void setPerspective(Vector3F const& eye, Vector3F const& lookat, Vector3F const& up, float fov, float aspect, float near_plane, float far_plane) override;
        BoxF getViewport() override { return m_viewport; }
        void setViewport(BoxF const& box) override;
        void setScissorRect(RectF const& rectangle) override;
        void setViewportAndScissorRect() override;
        void setVertexColorBlendState(VertexColorBlendState state) override;
        void setFogState(FogState state, Color4B const& color, float density_or_near, float far_plane) override;
        void setDepthState(DepthState state) override;
        void setBlendState(BlendState state) override;
        void setTexture(ITexture2D* texture) override;
        bool drawTriangle(DrawVertex const& a, DrawVertex const& b, DrawVertex const& c) override;
        bool drawTriangle(DrawVertex const* vertices) override;
        bool drawQuad(DrawVertex const& a, DrawVertex const& b, DrawVertex const& c, DrawVertex const& d) override;
        bool drawQuad(DrawVertex const* vertices) override;
        bool drawRaw(DrawVertex const* vertices, uint16_t vertex_count, DrawIndex const* indices, uint16_t index_count) override;
        bool drawRequest(uint16_t vertices, uint16_t indices, DrawVertex** output_vertices, DrawIndex** output_indices, uint16_t* offset) override;
        bool createPostEffectShader(StringView path, IPostEffectShader** output) override;
        bool createPostEffectShaderFromSource(StringView source, IPostEffectShader** output, StringView source_name = {}) override;
        bool drawPostEffect(IPostEffectShader*, BlendState, ITexture2D*, IRenderer::SamplerState, Vector4F const*, size_t, ITexture2D* const*, IRenderer::SamplerState const*, size_t) override;
        bool drawPostEffect(IPostEffectShader*, BlendState) override;
        bool createModel(StringView path, IModel** output) override;
        bool drawModel(IModel*) override;
        ISamplerState* getKnownSamplerState(IRenderer::SamplerState state) override;

        void setDefaultAttachment(IRenderTarget* target, IDepthStencilBuffer* depth);
        SDL_GPURenderPass* beginPass(bool clear_color = false, Color4B color = {}, bool clear_depth = false, float depth = 1.0f);
        bool applyPostEffect(PostEffectShader& shader);
        bool drawMesh(Mesh& mesh, Matrix4F const& transform);

    private:
        SmartReference<Device> m_device;
        static SDL_GPUColorTargetBlendState blendState(BlendState state);
        bool drawEffect(PostEffectShader& shader, BlendState blend, std::span<const SDL_GPUTextureSamplerBinding> bindings, std::span<const EffectBuffer> buffers);
        using PipelineKey = std::tuple<BlendState, DepthState, bool, VertexColorBlendState, FogState, bool>;
        SDL_GPUGraphicsPipeline* pipeline();
        SDL_GPUGraphicsPipeline* meshPipeline(Mesh const& mesh);
        SDL_Rect applyViewport(SDL_GPURenderPass* pass);
        void bindSpriteParameters(SDL_GPURenderPass* pass, Texture2D* texture, SDLGPU::SamplerState* sampler);
        using MeshPipelineKey = std::tuple<PipelineKey, bool, bool, PrimitiveTopology>;
        std::map<MeshPipelineKey, Pipeline> m_mesh_pipelines;
        std::array<Shader, 4> m_mesh_shaders;
        SDL_GPUGraphicsPipeline* modelPipeline(ModelPrimitive const& primitive);
        using ModelPipelineKey = std::tuple<int, bool, bool, uint32_t, FogState>;
        std::map<ModelPipelineKey, Pipeline> m_model_pipelines;
        Shader m_model_vertex, m_model_fragment;
        SmartReference<IRenderTarget> m_default_target;
        SmartReference<IDepthStencilBuffer> m_default_depth;
        SmartReference<IRenderTarget> m_target;
        SmartReference<IDepthStencilBuffer> m_depth;
        SmartReference<ITexture2D> m_texture;
        SmartReference<ITexture2D> m_white;
        std::array<SmartReference<ISamplerState>, 8> m_samplers;
        Shader m_vertex_shader;
        Shader m_fragment_shader;
        Shader m_effect_vertex_shader;
        SDL_GPURenderPass* m_effect_pass{};
        std::map<PipelineKey, Pipeline> m_pipelines;
        GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer> m_vertex_buffer;
        GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer> m_index_buffer;
        TransferBuffer m_upload;
        std::vector<DrawVertex> m_vertices;
        std::vector<DrawIndex> m_indices;
        DirectX::XMFLOAT4X4 m_matrix{};
        Vector3F m_eye{};
        BoxF m_viewport{ 0, 0, 0, 640, 480, 1 };
        RectF m_scissor{ 0, 0, 640, 480 };
        Color4B m_fog_color{};
        float m_fog_near{};
        float m_fog_far{ 1.0f };
        VertexColorBlendState m_color{ VertexColorBlendState::Mul };
        FogState m_fog{ FogState::Disable };
        DepthState m_depth_state{ DepthState::Disable };
        BlendState m_blend{ BlendState::Alpha };
        bool m_batch{};
    };
}
