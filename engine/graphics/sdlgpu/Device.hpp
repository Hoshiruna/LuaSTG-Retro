#pragma once

#include "Core/Graphics/Device.hpp"
#include "Core/Graphics/Renderer.hpp"
#include "GpuContext.hpp"
#include "ShaderCompiler.hpp"
#include "TextureTransfer.hpp"
#include "core/SmartReference.hpp"
#include "core/Window.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include <functional>
#include <vector>

namespace core::Graphics::SDLGPU
{
    class Device final : public implement::ReferenceCounted<IDevice>
    {
    public:
        // Selecting the driver and claiming the window happen together: an "auto" selection
        // only accepts a driver that can also present to this window.
        Device(IWindow* window, StringView renderer_driver);
        ~Device() override;
        void addEventListener(IDeviceEventListener* listener) override;
        void removeEventListener(IDeviceEventListener* listener) override;
        DeviceMemoryUsageStatistics getMemoryUsageStatistics() override { return {}; }
        bool recreate() override;
        StringView getCurrentGpuName() const noexcept override { return m_context.driver(); }
        bool createVertexBuffer(uint32_t size, IBuffer** output) override;
        bool createIndexBuffer(uint32_t size, IBuffer** output) override;
        bool createConstantBuffer(uint32_t size, IBuffer** output) override;
        bool createTextureFromFile(StringView path, bool mipmap, ITexture2D** output) override;
        bool createTextureFromData(IData* data, bool mipmap, ITexture2D** output) override;
        bool createTexture(Vector2U size, ITexture2D** output) override;
        bool createRenderTarget(Vector2U size, IRenderTarget** output) override;
        bool createDepthStencilBuffer(Vector2U size, IDepthStencilBuffer** output) override;
        bool createSamplerState(Graphics::SamplerState const& info, ISamplerState** output) override;

        SDL_GPUDevice* gpu() const noexcept { return m_context.device(); }
        GpuContext& context() noexcept { return m_context; }
        ShaderCompiler& compiler() noexcept { return m_compiler; }
        void setFrame(Frame* frame) noexcept { m_frame = frame; }
        Frame* frame() const noexcept { return m_frame; }
        void setRenderer(IRenderer* renderer) noexcept { m_renderer = renderer; }
        void beforeTransfer();
        void copy(const std::function<void(SDL_GPUCopyPass*)>& record);
        bool saveTexture(SDL_GPUTexture* texture, Vector2U size, StringView path);
        bool finishCaptures();
        void fail(const std::exception& error) noexcept;
        bool failed() const noexcept { return m_failed; }

    private:
        struct Capture
        {
            std::string path;
            Vector2U size;
            TextureTransfer transfer;
        };
        SmartReference<IWindow> m_window;
        // Declaration order is load-bearing: the compiler asks the context which driver was
        // selected in order to decide whether it needs the DXIL back end.
        GpuContext m_context;
        ShaderCompiler m_compiler;
        std::vector<IDeviceEventListener*> m_listeners;
        std::vector<Capture> m_captures;
        Frame* m_frame{};
        IRenderer* m_renderer{};
        bool m_failed{};
    };

    class Texture2D final : public implement::ReferenceCounted<ITexture2D>
    {
    public:
        Texture2D(Device* device, Vector2U size, bool dynamic, bool render_target, bool mipmap);
        bool isDynamic() const noexcept override { return m_dynamic; }
        bool isPremultipliedAlpha() const noexcept override { return m_premultiplied; }
        void setPremultipliedAlpha(bool value) override;
        Vector2U getSize() const noexcept override { return m_size; }
        bool setSize(Vector2U size) override;
        bool uploadPixelData(RectU rectangle, const void* data, uint32_t pitch) override;
        void setPixelData(IData* data) override;
        bool saveToFile(StringView path) override;
        void setSamplerState(ISamplerState* sampler) override;
        ISamplerState* getSamplerState() const noexcept override { return m_sampler.get(); }
        SDL_GPUTexture* handle() const noexcept { return m_texture.get(); }
        // Engine pixel uploads use BGRA8; retained pixels and GPU textures use RGBA8.
        void uploadRgba(std::span<const uint8_t> pixels);

    private:
        void allocate(Vector2U size);
        SmartReference<Device> m_device;
        SmartReference<ISamplerState> m_sampler;
        Texture m_texture;
        Vector2U m_size;
        std::vector<uint8_t> m_pixels;
        bool m_dynamic{};
        bool m_render_target{};
        bool m_mipmap{};
        bool m_premultiplied{};
    };

    class RenderTarget final : public implement::ReferenceCounted<IRenderTarget>
    {
    public:
        RenderTarget(Device* device, Vector2U size);
        bool setSize(Vector2U size) override { return m_texture->setSize(size); }
        ITexture2D* getTexture() const noexcept override { return m_texture.get(); }

    private:
        SmartReference<Texture2D> m_texture;
    };

    class DepthBuffer final : public implement::ReferenceCounted<IDepthStencilBuffer>
    {
    public:
        DepthBuffer(Device* device, Vector2U size);
        bool setSize(Vector2U size) override;
        Vector2U getSize() const noexcept override { return m_size; }
        SDL_GPUTexture* handle() const noexcept { return m_texture.get(); }
        static constexpr SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;

    private:
        SmartReference<Device> m_device;
        Texture m_texture;
        Vector2U m_size;
    };

    class SamplerState final : public implement::ReferenceCounted<ISamplerState>
    {
    public:
        SamplerState(Device* device, Graphics::SamplerState const& info);
        SDL_GPUSampler* handle() const noexcept { return m_sampler.get(); }
        Graphics::SamplerState const& description() const noexcept { return m_info; }

    private:
        SmartReference<Device> m_device;
        Sampler m_sampler;
        Graphics::SamplerState m_info;
    };
}
