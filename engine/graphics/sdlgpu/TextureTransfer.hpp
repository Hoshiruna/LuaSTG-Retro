#pragma once

#include "Resources.hpp"
#include <cstdint>
#include <span>
#include <vector>

namespace core::Graphics::SDLGPU
{
    // CPU rows use tightly packed RGBA8. GPU transfer rows meet D3D12 alignment.
    class TextureTransfer final
    {
    public:
        TextureTransfer(SDL_GPUDevice* device, uint32_t width, uint32_t height, SDL_GPUTransferBufferUsage usage);
        void upload(SDL_GPUCopyPass* pass, SDL_GPUTexture* texture, uint32_t x, uint32_t y, std::span<const uint8_t> pixels, uint32_t source_pitch);
        void download(SDL_GPUCopyPass* pass, SDL_GPUTexture* texture);
        // Call only after the command buffer containing download has completed.
        std::vector<uint8_t> readPixels() const;

    private:
        SDL_GPUDevice* m_device{};
        TransferBuffer m_buffer;
        uint32_t m_width{};
        uint32_t m_height{};
        uint32_t m_pitch{};
        SDL_GPUTransferBufferUsage m_usage{};
    };
}
