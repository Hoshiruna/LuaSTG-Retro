#include "Device.hpp"
#include "PngWriter.hpp"
#include "core/FileSystem.hpp"
#include "core/Image.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        template<typename Interface, typename Factory>
        bool createResource(Interface** output, Factory&& factory)
        {
            if(output == nullptr) {
                return false;
            }
            *output = nullptr;
            try {
                *output = factory();
                return true;
            } catch(const std::exception& error) {
                Logger::error("[sdlgpu] Create resource: {}", error.what());
                return false;
            }
        }

        uint32_t pixelBytes(Vector2U size)
        {
            if(size.x == 0 || size.y == 0 || uint64_t(size.x) * size.y > (std::numeric_limits<uint32_t>::max)() / 4) {
                throw std::invalid_argument("Invalid texture dimensions");
            }
            return size.x * size.y * 4;
        }

        class Buffer final : public implement::ReferenceCounted<IBuffer>
        {
        public:
            Buffer(Device* device, uint32_t size, SDL_GPUBufferUsageFlags usage)
                : m_device(device), m_pixels(size)
            {
                SDL_GPUBufferCreateInfo info{};
                info.usage = usage;
                info.size = size;
                m_buffer = { require(SDL_CreateGPUBuffer(device->gpu(), &info), "Create GPU buffer"), { device->gpu() } };
            }
            bool map(uint32_t size, bool discard, void** output) override
            {
                if(output == nullptr || m_mapped || size == 0 || size > m_pixels.size()) {
                    return false;
                }
                if(discard) {
                    std::fill(m_pixels.begin(), m_pixels.end(), 0);
                }
                m_mapped = true;
                *output = m_pixels.data();
                return true;
            }
            bool unmap() override
            {
                if(!m_mapped) {
                    return false;
                }
                m_mapped = false;
                try {
                    m_device->beforeTransfer();
                    SDL_GPUTransferBufferCreateInfo info{};
                    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                    info.size = static_cast<uint32_t>(m_pixels.size());
                    TransferBuffer upload(require(SDL_CreateGPUTransferBuffer(m_device->gpu(), &info), "Create buffer upload"), { m_device->gpu() });
                    auto* mapped = require(SDL_MapGPUTransferBuffer(m_device->gpu(), upload.get(), false), "Map buffer upload");
                    std::memcpy(mapped, m_pixels.data(), m_pixels.size());
                    SDL_UnmapGPUTransferBuffer(m_device->gpu(), upload.get());
                    m_device->copy([&](SDL_GPUCopyPass* pass) {
                        const SDL_GPUTransferBufferLocation source{ upload.get(), 0 };
                        const SDL_GPUBufferRegion target{ m_buffer.get(), 0, info.size };
                        SDL_UploadToGPUBuffer(pass, &source, &target, true);
                    });
                    return true;
                } catch(const std::exception& error) {
                    m_device->fail(error);
                    return false;
                }
            }

        private:
            SmartReference<Device> m_device;
            GpuResource<SDL_GPUBuffer, SDL_ReleaseGPUBuffer> m_buffer;
            std::vector<uint8_t> m_pixels;
            bool m_mapped{};
        };
    }

    Device::Device(IWindow* const window, const StringView renderer_driver)
        : m_window(window)
        , m_context(window->getSDLWindow(), renderer_driver)
        , m_compiler(m_context.driver())
    {
    }
    Device::~Device() = default;
    void Device::addEventListener(IDeviceEventListener* listener)
    {
        removeEventListener(listener);
        m_listeners.push_back(listener);
    }
    void Device::removeEventListener(IDeviceEventListener* listener)
    {
        std::erase(m_listeners, listener);
    }
    bool Device::recreate()
    {
        Logger::error("[sdlgpu] Device recovery is not available in the core 2D milestone");
        return false;
    }
    void Device::fail(const std::exception& error) noexcept
    {
        Logger::error("[sdlgpu] {}", error.what());
        m_failed = true;
    }
    void Device::beforeTransfer()
    {
        if(m_renderer && !m_renderer->flush()) {
            throw std::runtime_error("Flush sprite batch before resource transfer");
        }
        if(m_frame) {
            m_frame->endPass();
        }
    }
    void Device::copy(const std::function<void(SDL_GPUCopyPass*)>& record)
    {
        if(m_frame) {
            record(m_frame->beginCopyPass());
            m_frame->endPass();
            return;
        }
        auto* command = require(SDL_AcquireGPUCommandBuffer(gpu()), "Acquire resource upload command");
        try {
            {
                auto pass = beginCopyPass(command);
                record(pass.get());
            }
        } catch(...) {
            SDL_CancelGPUCommandBuffer(command);
            throw;
        }
        check(SDL_SubmitGPUCommandBuffer(command), "Submit resource upload");
    }
    bool Device::saveTexture(SDL_GPUTexture* texture, Vector2U size, StringView path)
    {
        try {
            beforeTransfer();
            Capture capture{ std::string(path), size, TextureTransfer(gpu(), size.x, size.y, SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD) };
            copy([&](SDL_GPUCopyPass* pass) { capture.transfer.download(pass, texture); });
            m_captures.push_back(std::move(capture));
            return m_frame != nullptr || finishCaptures();
        } catch(const std::exception& error) {
            fail(error);
            return false;
        }
    }
    bool Device::finishCaptures()
    {
        if(m_captures.empty()) {
            return true;
        }
        check(SDL_WaitForGPUIdle(gpu()), "Wait for texture capture");
        bool success = true;
        for(const auto& capture : m_captures) {
            auto pixels = capture.transfer.readPixels();
            success = writePng(capture.path, capture.size.x, capture.size.y, pixels.data(), capture.size.x * 4) && success;
        }
        m_captures.clear();
        return success;
    }
    bool Device::createVertexBuffer(uint32_t size, IBuffer** output)
    {
        return createResource(output, [&] { return new Buffer(this, size, SDL_GPU_BUFFERUSAGE_VERTEX); });
    }
    bool Device::createIndexBuffer(uint32_t size, IBuffer** output)
    {
        return createResource(output, [&] { return new Buffer(this, size, SDL_GPU_BUFFERUSAGE_INDEX); });
    }
    bool Device::createConstantBuffer(uint32_t size, IBuffer** output)
    {
        return createResource(output, [&] { return new Buffer(this, size, SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ); });
    }
    bool Device::createTextureFromFile(StringView path, bool mipmap, ITexture2D** output)
    {
        if(output) {
            *output = nullptr;
        }
        SmartReference<IData> data;
        return FileSystemManager::readFile(path, data.put()) && createTextureFromData(data.get(), mipmap, output);
    }
    bool Device::createTextureFromData(IData* data, bool mipmap, ITexture2D** output)
    {
        return createResource(output, [&]() -> ITexture2D* {
            SmartReference<core::IImage> image;
            if(!ImageFactory::createFromData(data, image.put())) {
                throw std::runtime_error("Decode texture image");
            }
            const auto size = image->getSize();
            std::vector<uint8_t> pixels(pixelBytes(size));
            for(uint32_t y = 0; y < size.y; ++y) {
                for(uint32_t x = 0; x < size.x; ++x) {
                    const auto pixel = image->getPixel(x, y);
                    const float channels[]{ pixel.x, pixel.y, pixel.z, pixel.w };
                    for(size_t channel = 0; channel < 4; ++channel) {
                        pixels[(size_t(y) * size.x + x) * 4 + channel] = static_cast<uint8_t>(std::clamp(channels[channel], 0.0f, 1.0f) * 255.0f + 0.5f);
                    }
                }
            }
            SmartReference<Texture2D> texture;
            texture.attach(new Texture2D(this, size, false, false, mipmap));
            texture->setPremultipliedAlpha(image->getAlphaMode() == ImageAlphaMode::premultiplied);
            texture->uploadRgba(pixels);
            return texture.detach();
        });
    }
    bool Device::createTexture(Vector2U size, ITexture2D** output)
    {
        return createResource(output, [&] { return new Texture2D(this, size, true, false, false); });
    }
    bool Device::createRenderTarget(Vector2U size, IRenderTarget** output)
    {
        return createResource(output, [&] { return new RenderTarget(this, size); });
    }
    bool Device::createDepthStencilBuffer(Vector2U size, IDepthStencilBuffer** output)
    {
        return createResource(output, [&] { return new DepthBuffer(this, size); });
    }
    bool Device::createSamplerState(Graphics::SamplerState const& info, ISamplerState** output)
    {
        return createResource(output, [&] { return new SamplerState(this, info); });
    }

    Texture2D::Texture2D(Device* device, Vector2U size, bool dynamic, bool render_target, bool mipmap)
        : m_device(device), m_dynamic(dynamic), m_render_target(render_target), m_mipmap(mipmap), m_premultiplied(render_target)
    {
        allocate(size);
    }
    void Texture2D::allocate(Vector2U size)
    {
        const auto bytes = pixelBytes(size);
        m_device->beforeTransfer();
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | (m_render_target || m_mipmap ? SDL_GPU_TEXTUREUSAGE_COLOR_TARGET : 0);
        info.width = size.x;
        info.height = size.y;
        info.layer_count_or_depth = 1;
        info.num_levels = m_mipmap ? 1 + static_cast<uint32_t>(std::floor(std::log2((std::max)(size.x, size.y)))) : 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        Texture replacement(require(SDL_CreateGPUTexture(m_device->gpu(), &info), "Create RGBA texture"), { m_device->gpu() });
        std::vector<uint8_t> pixels(bytes);
        m_texture = std::move(replacement);
        m_pixels = std::move(pixels);
        m_size = size;
        uploadRgba(m_pixels);
    }
    void Texture2D::uploadRgba(std::span<const uint8_t> pixels)
    {
        if(pixels.size() != pixelBytes(m_size)) {
            throw std::invalid_argument("Texture pixel count does not match its dimensions");
        }
        m_device->beforeTransfer();
        if(pixels.data() != m_pixels.data()) {
            m_pixels.assign(pixels.begin(), pixels.end());
        }
        TextureTransfer transfer(m_device->gpu(), m_size.x, m_size.y, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
        m_device->copy([&](SDL_GPUCopyPass* pass) { transfer.upload(pass, handle(), 0, 0, m_pixels, m_size.x * 4); });
        if(m_mipmap) {
            if(auto* frame = m_device->frame()) {
                SDL_GenerateMipmapsForGPUTexture(frame->commandOutsidePass(), handle());
            } else {
                auto* command = require(SDL_AcquireGPUCommandBuffer(m_device->gpu()), "Acquire mipmap command");
                SDL_GenerateMipmapsForGPUTexture(command, handle());
                check(SDL_SubmitGPUCommandBuffer(command), "Submit mipmap generation");
            }
        }
    }
    bool Texture2D::setSize(Vector2U size)
    {
        if(!m_dynamic) {
            return false;
        }
        try {
            allocate(size);
            return true;
        } catch(const std::exception& error) {
            m_device->fail(error);
            return false;
        }
    }
    bool Texture2D::uploadPixelData(RectU rectangle, const void* data, uint32_t pitch)
    {
        if(!m_dynamic || !data || rectangle.b.x > m_size.x || rectangle.b.y > m_size.y ||
            rectangle.a.x >= rectangle.b.x || rectangle.a.y >= rectangle.b.y || pitch < (rectangle.b.x - rectangle.a.x) * 4) {
            return false;
        }
        try {
            m_device->beforeTransfer();
            const uint32_t width = rectangle.b.x - rectangle.a.x;
            const uint32_t height = rectangle.b.y - rectangle.a.y;
            std::vector<uint8_t> region(size_t(width) * height * 4);
            for(uint32_t y = 0; y < height; ++y) {
                for(uint32_t x = 0; x < width; ++x) {
                    const auto* source = static_cast<const uint8_t*>(data) + size_t(y) * pitch + x * 4;
                    auto* target = region.data() + (size_t(y) * width + x) * 4;
                    target[0] = source[2];
                    target[1] = source[1];
                    target[2] = source[0];
                    target[3] = source[3];
                }
                std::memcpy(m_pixels.data() + (size_t(y + rectangle.a.y) * m_size.x + rectangle.a.x) * 4,
                    region.data() + size_t(y) * width * 4,
                    size_t(width) * 4);
            }
            TextureTransfer transfer(m_device->gpu(), width, height, SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD);
            m_device->copy([&](SDL_GPUCopyPass* pass) { transfer.upload(pass, handle(), rectangle.a.x, rectangle.a.y, region, width * 4); });
            return true;
        } catch(const std::exception& error) {
            m_device->fail(error);
            return false;
        }
    }
    void Texture2D::setPixelData(IData* data)
    {
        if(data && data->size() >= pixelBytes(m_size)) {
            uploadPixelData({ { 0, 0 }, m_size }, data->data(), m_size.x * 4);
        }
    }
    bool Texture2D::saveToFile(StringView path)
    {
        return m_device->saveTexture(handle(), m_size, path);
    }
    void Texture2D::setPremultipliedAlpha(bool value)
    {
        if(value == m_premultiplied)
            return;
        try {
            m_device->beforeTransfer();
            m_premultiplied = value;
        } catch(const std::exception& error) {
            m_device->fail(error);
        }
    }
    void Texture2D::setSamplerState(ISamplerState* sampler)
    {
        if(sampler == m_sampler.get())
            return;
        try {
            m_device->beforeTransfer();
            m_sampler = sampler;
        } catch(const std::exception& error) {
            m_device->fail(error);
        }
    }

    RenderTarget::RenderTarget(Device* device, Vector2U size)
    {
        m_texture.attach(new Texture2D(device, size, true, true, false));
    }
    DepthBuffer::DepthBuffer(Device* device, Vector2U size)
        : m_device(device)
    {
        if(!setSize(size)) {
            throw std::runtime_error("Create depth buffer");
        }
    }
    bool DepthBuffer::setSize(Vector2U size)
    {
        try {
            pixelBytes(size);
            m_device->beforeTransfer();
            SDL_GPUTextureCreateInfo info{};
            info.type = SDL_GPU_TEXTURETYPE_2D;
            info.format = format;
            info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            info.width = size.x;
            info.height = size.y;
            info.layer_count_or_depth = 1;
            info.num_levels = 1;
            info.sample_count = SDL_GPU_SAMPLECOUNT_1;
            m_texture = { require(SDL_CreateGPUTexture(m_device->gpu(), &info), "Create depth texture"), { m_device->gpu() } };
            m_size = size;
            return true;
        } catch(const std::exception& error) {
            m_device->fail(error);
            return false;
        }
    }

    SamplerState::SamplerState(Device* device, Graphics::SamplerState const& info)
        : m_device(device), m_info(info)
    {
        // Border and mirror-once addressing are implemented by the sprite shader.
        auto address = [](TextureAddressMode mode) {
            if(mode == TextureAddressMode::Wrap) {
                return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
            }
            if(mode == TextureAddressMode::Mirror) {
                return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
            }
            return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        };
        SDL_GPUSamplerCreateInfo state{};
        const auto filter = info.filer;
        state.min_filter = filter == Filter::Point || filter == Filter::PointMagLinear || filter == Filter::PointMipLinear || filter == Filter::LinearMinPoint ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        state.mag_filter = filter == Filter::Point || filter == Filter::PointMinLinear || filter == Filter::PointMipLinear || filter == Filter::LinearMagPoint ? SDL_GPU_FILTER_NEAREST : SDL_GPU_FILTER_LINEAR;
        state.mipmap_mode = filter == Filter::Linear || filter == Filter::Anisotropic || filter == Filter::LinearMinPoint || filter == Filter::LinearMagPoint || filter == Filter::PointMipLinear ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        state.address_mode_u = address(info.address_u);
        state.address_mode_v = address(info.address_v);
        state.address_mode_w = address(info.address_w);
        state.mip_lod_bias = info.mip_lod_bias;
        state.min_lod = (std::max)(0.0f, info.min_lod);
        state.max_lod = info.max_lod;
        state.enable_anisotropy = filter == Filter::Anisotropic;
        state.max_anisotropy = static_cast<float>(std::clamp(info.max_anisotropy, 1u, 16u));
        m_sampler = { require(SDL_CreateGPUSampler(device->gpu(), &state), "Create sampler"), { device->gpu() } };
    }
}

namespace core::Graphics
{
    bool IDevice::create(IDevice** output)
    {
        if(!output) {
            return false;
        }
        *output = nullptr;
        try {
            *output = new SDLGPU::Device();
            return true;
        } catch(const std::exception& error) {
            Logger::error("[sdlgpu] Create device: {}", error.what());
            return false;
        }
    }
}
