#pragma once

#include "Resources.hpp"
#include <span>

namespace core::Graphics::SDLGPU
{
    // A swapchain acquisition must be submitted; SDL forbids canceling it.
    class Frame final
    {
    public:
        explicit Frame(SDL_GPUDevice* device, SDL_Window* window);
        ~Frame();
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;

        // Borrowed handles are valid until the frame switches passes or submits.
        SDL_GPUCommandBuffer* command() const noexcept { return m_command; }
        SDL_GPUCommandBuffer* commandOutsidePass();
        SDL_GPURenderPass* beginRenderPass(std::span<const SDL_GPUColorTargetInfo> targets, const SDL_GPUDepthStencilTargetInfo* depth = nullptr);
        SDL_GPUCopyPass* beginCopyPass();
        void endPass() noexcept;
        SDL_GPUTexture* texture() const noexcept { return m_texture; }
        Uint32 width() const noexcept { return m_width; }
        Uint32 height() const noexcept { return m_height; }
        void submit();

    private:
        SDL_GPUCommandBuffer* m_command{};
        SDL_GPURenderPass* m_render_pass{};
        SDL_GPUCopyPass* m_copy_pass{};
        SDL_GPUTexture* m_texture{};
        Uint32 m_width{};
        Uint32 m_height{};
    };

    class GpuContext final
    {
    public:
        GpuContext(SDL_Window* window, const char* driver = nullptr, bool debug = false);
        ~GpuContext();
        GpuContext(const GpuContext&) = delete;
        GpuContext& operator=(const GpuContext&) = delete;

        SDL_GPUDevice* device() const noexcept { return m_device.get(); }
        SDL_GPUTextureFormat swapchainFormat() const;
        const char* driver() const;
        bool setVSync(bool enabled);
        Frame acquire() { return Frame(m_device.get(), m_window); }

    private:
        std::unique_ptr<SDL_GPUDevice, decltype(&SDL_DestroyGPUDevice)> m_device{ nullptr, SDL_DestroyGPUDevice };
        SDL_Window* m_window{};
    };
}
