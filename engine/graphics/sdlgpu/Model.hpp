#pragma once
#include "Device.hpp"
#include "ModelData.hpp"

namespace core::Graphics::SDLGPU
{
    class Model final : public implement::ReferenceCounted<IModel>
    {
    private:
        SmartReference<Device> m_device;

    public:
        Model(Device* device, StringView path);
        void setAmbient(Vector3F const& color, float brightness) override { ambient = { color.x, color.y, color.z, brightness }; }
        void setDirectionalLight(Vector3F const& direction, Vector3F const& color, float brightness) override;
        void setScaling(Vector3F const& value) override { scale = value; }
        void setPosition(Vector3F const& value) override { position = value; }
        void setRotationRollPitchYaw(float roll, float pitch, float yaw) override;
        void setRotationQuaternion(Vector4F const& value) override { rotation = value; }
        DirectX::XMMATRIX transform() const;
        Device* device() const noexcept { return m_device.get(); }
        struct Buffers
        {
            GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer> vertices, indices;
        };
        ModelData data;
        std::vector<Buffers> buffers;
        std::vector<SmartReference<Texture2D>> images;
        std::vector<SmartReference<SDLGPU::SamplerState>> samplers;
        Vector4F ambient{ 1, 1, 1, 1 }, light_direction{ 0, -1, 0, 0 }, light_color{ 1, 1, 1, 0 };

    private:
        Vector3F scale{ 1, 1, 1 }, position{};
        Vector4F rotation{ 0, 0, 0, 1 };
    };
}
