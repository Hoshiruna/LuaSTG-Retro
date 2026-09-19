#include "Model.hpp"
#include <cstring>

namespace core::Graphics::SDLGPU
{
    Model::Model(Device* device, StringView path) : m_device(device), data(loadModelData(path))
    {
        device->beforeTransfer();
        auto upload = [&](const void* bytes, size_t size, SDL_GPUBufferUsageFlags usage) {
            SDL_GPUBufferCreateInfo info{};
            info.size = static_cast<uint32_t>(size);
            info.usage = usage;
            GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer> buffer(require(SDL_CreateGPUBuffer(device->gpu(), &info), "Create model buffer"), { device->gpu() });
            SDL_GPUTransferBufferCreateInfo transfer_info{};
            transfer_info.size = info.size;
            transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            TransferBuffer transfer(require(SDL_CreateGPUTransferBuffer(device->gpu(), &transfer_info), "Create model transfer"), { device->gpu() });
            auto* mapped = require(SDL_MapGPUTransferBuffer(device->gpu(), transfer.get(), false), "Map model transfer");
            std::memcpy(mapped, bytes, size);
            SDL_UnmapGPUTransferBuffer(device->gpu(), transfer.get());
            device->copy([&](SDL_GPUCopyPass* pass) {
                const SDL_GPUTransferBufferLocation source{ transfer.get(), 0 };
                const SDL_GPUBufferRegion target{ buffer.get(), 0, info.size };
                SDL_UploadToGPUBuffer(pass, &source, &target, false);
            });
            return buffer;
        };
        for(auto const& primitive : data.primitives) {
            Buffers block;
            if(!primitive.vertices.empty())
                block.vertices = upload(primitive.vertices.data(), primitive.vertices.size() * sizeof(ModelVertex), SDL_GPU_BUFFERUSAGE_VERTEX);
            if(!primitive.indices.empty())
                block.indices = upload(primitive.indices.data(), primitive.indices.size(), SDL_GPU_BUFFERUSAGE_INDEX);
            buffers.push_back(std::move(block));
        }
        for(auto const& source : data.images) {
            SmartReference<Texture2D> image;
            image.attach(new Texture2D(device, source.size, false, false, true));
            image->uploadRgba(source.rgba);
            images.push_back(std::move(image));
        }
        for(auto const& source : data.samplers) {
            SmartReference<SDLGPU::SamplerState> sampler;
            sampler.attach(new SDLGPU::SamplerState(device, source));
            samplers.push_back(std::move(sampler));
        }
    }
    void Model::setDirectionalLight(Vector3F const& direction, Vector3F const& color, float brightness)
    {
        light_direction = { direction.x, direction.y, direction.z, 0 };
        light_color = { color.x, color.y, color.z, brightness };
    }
    void Model::setRotationRollPitchYaw(float roll, float pitch, float yaw)
    {
        DirectX::XMFLOAT4 value;
        DirectX::XMStoreFloat4(&value, DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
        rotation = { value.x, value.y, value.z, value.w };
    }
    DirectX::XMMATRIX Model::transform() const
    {
        using namespace DirectX;
        return XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixRotationQuaternion(XMVectorSet(rotation.x, rotation.y, rotation.z, rotation.w)) * XMMatrixTranslation(position.x, position.y, position.z);
    }
}
