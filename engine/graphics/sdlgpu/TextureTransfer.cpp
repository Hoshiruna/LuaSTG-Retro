#include "TextureTransfer.hpp"
#include <cstring>
#include <limits>

namespace core::Graphics::SDLGPU
{
    TextureTransfer::TextureTransfer(SDL_GPUDevice* device, uint32_t width, uint32_t height, SDL_GPUTransferBufferUsage usage)
        : m_device(device), m_width(width), m_height(height), m_usage(usage)
    {
        const uint64_t pitch = (uint64_t(width) * 4 + 255) & ~uint64_t(255);
        constexpr auto limit = (std::numeric_limits<uint32_t>::max)();
        if(width == 0 || height == 0 || pitch > limit || height > limit / pitch) {
            throw std::invalid_argument("Texture transfer dimensions exceed the transfer buffer limit");
        }
        const uint64_t size = pitch * height;
        m_pitch = static_cast<uint32_t>(pitch);
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = usage;
        info.size = static_cast<uint32_t>(size);
        m_buffer = TransferBuffer(require(SDL_CreateGPUTransferBuffer(device, &info), "Create texture transfer buffer"), { device });
    }

    void TextureTransfer::upload(SDL_GPUCopyPass* pass, SDL_GPUTexture* texture, uint32_t x, uint32_t y, std::span<const uint8_t> pixels, uint32_t source_pitch)
    {
        const size_t row_size = size_t(m_width) * 4;
        if(m_usage != SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD || source_pitch < row_size ||
            pixels.size() < uint64_t(source_pitch) * (m_height - 1) + row_size) {
            throw std::invalid_argument("Invalid texture upload pixels or row pitch");
        }
        auto* destination = static_cast<uint8_t*>(require(SDL_MapGPUTransferBuffer(m_device, m_buffer.get(), true), "Map texture upload"));
        for(uint32_t row = 0; row < m_height; ++row) {
            std::memcpy(destination + size_t(row) * m_pitch, pixels.data() + size_t(row) * source_pitch, row_size);
        }
        SDL_UnmapGPUTransferBuffer(m_device, m_buffer.get());

        SDL_GPUTextureTransferInfo source{};
        source.transfer_buffer = m_buffer.get();
        source.pixels_per_row = m_pitch / 4;
        source.rows_per_layer = m_height;
        SDL_GPUTextureRegion target{};
        target.texture = texture;
        target.x = x;
        target.y = y;
        target.w = m_width;
        target.h = m_height;
        target.d = 1;
        SDL_UploadToGPUTexture(pass, &source, &target, false);
    }

    void TextureTransfer::download(SDL_GPUCopyPass* pass, SDL_GPUTexture* texture)
    {
        if(m_usage != SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD) {
            throw std::logic_error("Texture readback requires a download transfer buffer");
        }
        SDL_GPUTextureRegion source{};
        source.texture = texture;
        source.w = m_width;
        source.h = m_height;
        source.d = 1;
        SDL_GPUTextureTransferInfo destination{};
        destination.transfer_buffer = m_buffer.get();
        destination.pixels_per_row = m_pitch / 4;
        destination.rows_per_layer = m_height;
        SDL_DownloadFromGPUTexture(pass, &source, &destination);
    }

    std::vector<uint8_t> TextureTransfer::readPixels() const
    {
        if(m_usage != SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD) {
            throw std::logic_error("Cannot read an upload transfer buffer");
        }
        std::vector<uint8_t> pixels(size_t(m_width) * m_height * 4);
        auto* source = static_cast<const uint8_t*>(require(SDL_MapGPUTransferBuffer(m_device, m_buffer.get(), false), "Map texture readback"));
        for(uint32_t row = 0; row < m_height; ++row) {
            std::memcpy(pixels.data() + size_t(row) * m_width * 4, source + size_t(row) * m_pitch, size_t(m_width) * 4);
        }
        SDL_UnmapGPUTransferBuffer(m_device, m_buffer.get());
        return pixels;
    }
}
