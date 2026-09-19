#pragma once

#include "Device.hpp"
#include "Core/Graphics/Mesh.hpp"

namespace core::Graphics::SDLGPU
{
    // CPU storage remains available after commit, including for read-only meshes.
    class MeshData
    {
    public:
        explicit MeshData(MeshOptions options);
        void position(uint32_t index, Vector3F value);
        void uv(uint32_t index, Vector2F value);
        void color(uint32_t index, Vector4F value);
        void index(uint32_t index, uint32_t value);
        void validate() const;
        MeshOptions options;
        uint32_t uv_offset{}, color_offset{}, stride{};
        std::vector<uint8_t> vertices, indices;
        bool read_only{};
        bool validation{ true };
        bool vertices_changed{ true }, indices_changed{ true };

    private:
        void write(uint32_t index, uint32_t offset, const void* source, size_t size);
    };

    class Mesh final : public implement::ReferenceCounted<IMesh>
    {
    public:
        Mesh(Device* device, MeshOptions options);
        uint32_t getVertexCount() const noexcept override { return m_data.options.vertex_count; }
        uint32_t getIndexCount() const noexcept override { return m_data.options.index_count; }
        PrimitiveTopology getPrimitiveTopology() const noexcept override { return m_data.options.primitive_topology; }
        bool isReadOnly() const noexcept override { return m_data.read_only; }
        void setValidationEnable(bool value) override { m_data.validation = value; }
        void setVertex(uint32_t i, Vector2F const& p, Vector2F const& uv, Color4B c) override;
        void setVertex(uint32_t i, Vector2F const& p, Vector2F const& uv, Vector4F const& c) override;
        void setVertex(uint32_t i, Vector3F const& p, Vector2F const& uv, Color4B c) override;
        void setVertex(uint32_t i, Vector3F const& p, Vector2F const& uv, Vector4F const& c) override;
        void setPosition(uint32_t i, Vector2F const& p) override;
        void setPosition(uint32_t i, Vector3F const& p) override;
        void setUv(uint32_t i, Vector2F const& uv) override;
        void setColor(uint32_t i, Color4B c) override;
        void setColor(uint32_t i, Vector4F const& c) override;
        void setIndex(uint32_t i, uint32_t value) override;
        bool commit() override;
        void setReadOnly() override { m_data.read_only = true; }
        MeshData const& data() const noexcept { return m_data; }
        SDL_GPUBuffer* vertices() const noexcept { return m_vertices.get(); }
        SDL_GPUBuffer* indices() const noexcept { return m_indices.get(); }
        Device* device() const noexcept { return m_device.get(); }

    private:
        SmartReference<Device> m_device;
        MeshData m_data;
        GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer> m_vertices, m_indices;
    };

    class MeshRenderer final : public implement::ReferenceCounted<IMeshRenderer>
    {
    public:
        explicit MeshRenderer(Device* device) : m_device(device) {}
        void setTransform(Matrix4F const& value) override { m_transform = value; }
        void setTexture(ITexture2D* value) override { m_texture = value; }
        void setMesh(IMesh* value) override { m_mesh = value; }
        void setLegacyBlendState(IRenderer::VertexColorBlendState color, IRenderer::BlendState blend) override;
        void draw(IRenderer* renderer) override;

    private:
        SmartReference<Device> m_device;
        SmartReference<ITexture2D> m_texture;
        SmartReference<IMesh> m_mesh;
        Matrix4F m_transform{ Matrix4F::identity() };
        IRenderer::VertexColorBlendState m_color{ IRenderer::VertexColorBlendState::Mul };
        IRenderer::BlendState m_blend{ IRenderer::BlendState::Alpha };
    };
}
