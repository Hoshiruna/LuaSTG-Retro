#include "Mesh.hpp"
#include "Renderer.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace core::Graphics::SDLGPU
{
    MeshData::MeshData(MeshOptions value) : options(value)
    {
        if(options.primitive_topology != PrimitiveTopology::triangle_list && options.primitive_topology != PrimitiveTopology::triangle_strip)
            throw std::invalid_argument("Mesh: invalid primitive topology");
        uv_offset = options.vertex_position_no_z ? 8 : 12;
        color_offset = uv_offset + 8;
        stride = color_offset + (options.vertex_color_compression ? 4 : 16);
        const uint64_t vb = uint64_t(options.vertex_count) * stride;
        const uint64_t ib = uint64_t(options.index_count) * (options.vertex_index_compression ? 2 : 4);
        if(vb > UINT32_MAX || ib > UINT32_MAX)
            throw std::invalid_argument("Mesh: buffer size exceeds SDL limit");
        vertices.resize(static_cast<size_t>(vb));
        indices.resize(static_cast<size_t>(ib));
    }
    void MeshData::write(uint32_t i, uint32_t offset, const void* source, size_t size)
    {
        if(read_only || i >= options.vertex_count) {
            if(validation)
                Logger::error("[sdlgpu] Mesh: read-only or vertex index out of range");
            return;
        }
        std::memcpy(vertices.data() + size_t(i) * stride + offset, source, size);
        vertices_changed = true;
    }
    void MeshData::position(uint32_t i, Vector3F p)
    { write(i, 0, &p, uv_offset); }
    void MeshData::uv(uint32_t i, Vector2F p)
    { write(i, uv_offset, &p, sizeof(p)); }
    void MeshData::color(uint32_t i, Vector4F c)
    {
        if(options.vertex_color_compression) {
            auto pack = [](float v) { return static_cast<uint8_t>(std::isfinite(v) ? std::clamp(v * 255.0f, 0.0f, 255.0f) : 0); };
            const uint8_t bgra[]{ pack(c.z), pack(c.y), pack(c.x), pack(c.w) };
            write(i, color_offset, bgra, sizeof(bgra));
        } else
            write(i, color_offset, &c, sizeof(c));
    }
    void MeshData::index(uint32_t i, uint32_t value)
    {
        if(read_only || i >= options.index_count || value >= options.vertex_count || (options.vertex_index_compression && value > UINT16_MAX)) {
            if(validation)
                Logger::error("[sdlgpu] Mesh: read-only or index out of range");
            return;
        }
        const size_t size = options.vertex_index_compression ? 2 : 4;
        std::memcpy(indices.data() + size_t(i) * size, &value, size);
        indices_changed = true;
    }
    void MeshData::validate() const
    {
        const size_t size = options.vertex_index_compression ? 2 : 4;
        for(uint32_t i = 0; i < options.index_count; ++i) {
            uint32_t value{};
            std::memcpy(&value, indices.data() + size_t(i) * size, size);
            if(value >= options.vertex_count)
                throw std::invalid_argument("Mesh: index references a missing vertex");
        }
    }
    Mesh::Mesh(Device* device, MeshOptions options) : m_device(device), m_data(options)
    {
        auto allocate = [&](size_t size, SDL_GPUBufferUsageFlags usage) {
            SDL_GPUBufferCreateInfo info{};
            info.size = static_cast<uint32_t>(size);
            info.usage = usage;
            return GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer>(require(SDL_CreateGPUBuffer(device->gpu(), &info), "Create mesh buffer"), { device->gpu() });
        };
        if(!m_data.vertices.empty())
            m_vertices = allocate(m_data.vertices.size(), SDL_GPU_BUFFERUSAGE_VERTEX);
        if(!m_data.indices.empty())
            m_indices = allocate(m_data.indices.size(), SDL_GPU_BUFFERUSAGE_INDEX);
        if(!commit())
            throw std::runtime_error("Initialize mesh buffers");
    }
    void Mesh::setVertex(uint32_t i, Vector2F const& p, Vector2F const& uv, Color4B c)
    {
        setPosition(i, p);
        setUv(i, uv);
        setColor(i, c);
    }
    void Mesh::setVertex(uint32_t i, Vector2F const& p, Vector2F const& uv, Vector4F const& c)
    {
        setPosition(i, p);
        setUv(i, uv);
        setColor(i, c);
    }
    void Mesh::setVertex(uint32_t i, Vector3F const& p, Vector2F const& uv, Color4B c)
    {
        setPosition(i, p);
        setUv(i, uv);
        setColor(i, c);
    }
    void Mesh::setVertex(uint32_t i, Vector3F const& p, Vector2F const& uv, Vector4F const& c)
    {
        setPosition(i, p);
        setUv(i, uv);
        setColor(i, c);
    }
    void Mesh::setPosition(uint32_t i, Vector2F const& p)
    { m_data.position(i, { p.x, p.y, 0 }); }
    void Mesh::setPosition(uint32_t i, Vector3F const& p)
    { m_data.position(i, p); }
    void Mesh::setUv(uint32_t i, Vector2F const& p)
    { m_data.uv(i, p); }
    void Mesh::setColor(uint32_t i, Color4B c)
    { m_data.color(i, { c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f }); }
    void Mesh::setColor(uint32_t i, Vector4F const& c)
    { m_data.color(i, c); }
    void Mesh::setIndex(uint32_t i, uint32_t value)
    { m_data.index(i, value); }
    bool Mesh::commit()
    {
        if(m_data.read_only)
            return false;
        try {
            m_data.validate();
            m_device->beforeTransfer();
            auto upload = [&](std::vector<uint8_t> const& bytes, SDL_GPUBuffer* buffer) {
                if(bytes.empty())
                    return;
                SDL_GPUTransferBufferCreateInfo info{};
                info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                info.size = static_cast<uint32_t>(bytes.size());
                TransferBuffer transfer(require(SDL_CreateGPUTransferBuffer(m_device->gpu(), &info), "Create mesh transfer"), { m_device->gpu() });
                auto* data = require(SDL_MapGPUTransferBuffer(m_device->gpu(), transfer.get(), false), "Map mesh transfer");
                std::memcpy(data, bytes.data(), bytes.size());
                SDL_UnmapGPUTransferBuffer(m_device->gpu(), transfer.get());
                m_device->copy([&](SDL_GPUCopyPass* pass) {
                    const SDL_GPUTransferBufferLocation source{ transfer.get(), 0 };
                    const SDL_GPUBufferRegion target{ buffer, 0, info.size };
                    SDL_UploadToGPUBuffer(pass, &source, &target, true);
                });
            };
            if(m_data.vertices_changed)
                upload(m_data.vertices, m_vertices.get());
            if(m_data.indices_changed)
                upload(m_data.indices, m_indices.get());
            m_data.vertices_changed = m_data.indices_changed = false;
            return true;
        } catch(std::exception const& error) {
            Logger::error("[sdlgpu] Commit mesh: {}", error.what());
            return false;
        }
    }
    void MeshRenderer::setLegacyBlendState(IRenderer::VertexColorBlendState color, IRenderer::BlendState blend)
    {
        m_color = color;
        m_blend = blend;
    }
    void MeshRenderer::draw(IRenderer* renderer)
    {
        if(!renderer || !m_mesh || !m_texture) {
            Logger::error("[sdlgpu] Draw mesh: renderer, mesh and texture are required");
            return;
        }
        renderer->setVertexColorBlendState(m_color);
        renderer->setBlendState(m_blend);
        renderer->setTexture(m_texture.get());
        static_cast<Renderer*>(renderer)->drawMesh(*static_cast<Mesh*>(m_mesh.get()), m_transform);
    }
}

namespace core::Graphics
{
    bool IMesh::create(IDevice* device, MeshOptions const& options, IMesh** output)
    {
        if(!output)
            return false;
        *output = nullptr;
        if(!device)
            return false;
        try {
            *output = new SDLGPU::Mesh(static_cast<SDLGPU::Device*>(device), options);
            return true;
        } catch(std::exception const& error) {
            Logger::error("[sdlgpu] Create mesh: {}", error.what());
            return false;
        }
    }
    bool IMeshRenderer::create(IDevice* device, IMeshRenderer** output)
    {
        if(!output)
            return false;
        *output = nullptr;
        if(!device)
            return false;
        try {
            *output = new SDLGPU::MeshRenderer(static_cast<SDLGPU::Device*>(device));
            return true;
        } catch(std::exception const& error) {
            Logger::error("[sdlgpu] Create mesh renderer: {}", error.what());
            return false;
        }
    }
}
