#include "Scene.hpp"
#include "Shaders.hpp"
#include <algorithm>
#include <cstring>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        constexpr Uint32 pattern_size = 64;
        constexpr Uint32 pattern_bytes = pattern_size * pattern_size * 4;
        constexpr SDL_GPUTextureFormat canvas_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

        // Used for uploads and readback, which never acquire a swapchain texture.
        struct CancelCommand
        {
            void operator()(SDL_GPUCommandBuffer* const command) const noexcept
            {
                SDL_CancelGPUCommandBuffer(command);
            }
        };
        using CopyCommand = std::unique_ptr<SDL_GPUCommandBuffer, CancelCommand>;
    }

    Scene::Scene(SDL_GPUDevice* const device, const ShaderCompiler& compiler)
        : m_device(device)
    {
        m_pattern = createTexture(pattern_size, pattern_size, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        m_white = createTexture(1, 1, SDL_GPU_TEXTUREUSAGE_SAMPLER);
        m_scene = createTexture(width, height, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        m_effect = createTexture(width, height, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        m_nearest = createSampler(SDL_GPU_FILTER_NEAREST);
        m_linear = createSampler(SDL_GPU_FILTER_LINEAR);

        const auto vertex = compiler.compile(device, shaders::quad, SDL_GPU_SHADERSTAGE_VERTEX, "smoke quad");
        const auto fragment = compiler.compile(device, shaders::textured, SDL_GPU_SHADERSTAGE_FRAGMENT, "smoke texture");
        const auto effect = compiler.compile(device, shaders::grayscale, SDL_GPU_SHADERSTAGE_FRAGMENT, "smoke grayscale");
        m_textured = createPipeline(vertex.get(), fragment.get(), true);
        m_grayscale = createPipeline(vertex.get(), effect.get(), false);
        uploadTextures();
    }

    Texture Scene::createTexture(const Uint32 texture_width, const Uint32 texture_height, const SDL_GPUTextureUsageFlags usage)
    {
        SDL_GPUTextureCreateInfo info{};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = canvas_format;
        info.usage = usage;
        info.width = texture_width;
        info.height = texture_height;
        info.layer_count_or_depth = 1;
        info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        return { require(SDL_CreateGPUTexture(m_device, &info), "SDL_CreateGPUTexture"), { m_device } };
    }

    Sampler Scene::createSampler(const SDL_GPUFilter filter)
    {
        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = filter;
        info.mag_filter = filter;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        return { require(SDL_CreateGPUSampler(m_device, &info), "SDL_CreateGPUSampler"), { m_device } };
    }

    Pipeline Scene::createPipeline(SDL_GPUShader* const vertex, SDL_GPUShader* const fragment, const bool blend)
    {
        SDL_GPUColorTargetDescription target{};
        target.format = canvas_format;
        target.blend_state.enable_blend = blend;
        target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = vertex;
        info.fragment_shader = fragment;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        info.target_info.color_target_descriptions = &target;
        info.target_info.num_color_targets = 1;
        return { require(SDL_CreateGPUGraphicsPipeline(m_device, &info), "SDL_CreateGPUGraphicsPipeline"), { m_device } };
    }

    void Scene::uploadTextures()
    {
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        info.size = pattern_bytes + pattern_size * 4;
        const TransferBuffer upload(require(SDL_CreateGPUTransferBuffer(m_device, &info), "Create texture upload buffer"), { m_device });
        auto* const pixels = static_cast<uint8_t*>(require(SDL_MapGPUTransferBuffer(m_device, upload.get(), false), "Map texture upload buffer"));
        constexpr std::array<std::array<uint8_t, 4>, 4> colors{ {
            { 255, 0, 0, 255 },
            { 0, 255, 0, 255 },
            { 0, 0, 255, 255 },
            { 255, 255, 255, 255 },
        } };
        for(Uint32 y = 0; y < pattern_size; ++y) {
            for(Uint32 x = 0; x < pattern_size; ++x) {
                const auto& color = colors[(y >= pattern_size / 2 ? 2 : 0) + (x >= pattern_size / 2 ? 1 : 0)];
                std::memcpy(pixels + (y * pattern_size + x) * 4, color.data(), 4);
            }
        }
        std::memset(pixels + pattern_bytes, 255, pattern_size * 4);
        SDL_UnmapGPUTransferBuffer(m_device, upload.get());

        CopyCommand command(require(SDL_AcquireGPUCommandBuffer(m_device), "Acquire texture upload command"));
        {
            const auto pass = beginCopyPass(command.get());
            SDL_GPUTextureTransferInfo source{};
            source.transfer_buffer = upload.get();
            source.pixels_per_row = pattern_size;
            source.rows_per_layer = pattern_size;
            SDL_GPUTextureRegion destination{};
            destination.texture = m_pattern.get();
            destination.w = pattern_size;
            destination.h = pattern_size;
            destination.d = 1;
            SDL_UploadToGPUTexture(pass.get(), &source, &destination, false);
            source.offset = pattern_bytes;
            source.rows_per_layer = 1;
            destination.texture = m_white.get();
            destination.w = 1;
            destination.h = 1;
            SDL_UploadToGPUTexture(pass.get(), &source, &destination, false);
        }
        check(SDL_SubmitGPUCommandBuffer(command.release()), "Submit texture upload");
    }

    void Scene::drawQuad(SDL_GPUCommandBuffer* const command, SDL_GPURenderPass* const pass, SDL_GPUTexture* const texture, SDL_GPUSampler* const sampler, const std::array<float, 4>& rectangle, const std::array<float, 4>& tint)
    {
        struct QuadData
        {
            std::array<float, 4> rectangle;
            std::array<float, 4> tint;
        };
        const QuadData data{ rectangle, tint };
        SDL_PushGPUVertexUniformData(command, 0, &data, sizeof(data));
        const SDL_GPUTextureSamplerBinding binding{ texture, sampler };
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    }

    void Scene::render(Frame& frame, const bool linear)
    {
        auto* const command = frame.command();
        SDL_GPUSampler* const sampler = linear ? m_linear.get() : m_nearest.get();
        SDL_GPUColorTargetInfo target{};
        target.texture = m_scene.get();
        target.clear_color = { 0, 0, 0, 1 };
        target.load_op = SDL_GPU_LOADOP_CLEAR;
        target.store_op = SDL_GPU_STOREOP_STORE;
        // Cycling keeps writes separate from earlier frames still using the canvas.
        target.cycle = true;
        {
            auto* const pass = frame.beginRenderPass({ &target, 1 });
            SDL_BindGPUGraphicsPipeline(pass, m_textured.get());
            drawQuad(command, pass, m_pattern.get(), sampler, { 0, 0, 320, 240 }, { 1, 1, 1, 1 });
            drawQuad(command, pass, m_white.get(), sampler, { 80, 80, 120, 80 }, { 1, 1, 0, 0.5f });
            drawQuad(command, pass, m_white.get(), sampler, { 120, 100, 120, 80 }, { 0, 1, 1, 0.5f });
        }
        target.texture = m_effect.get();
        {
            auto* const pass = frame.beginRenderPass({ &target, 1 });
            SDL_BindGPUGraphicsPipeline(pass, m_grayscale.get());
            drawQuad(command, pass, m_scene.get(), m_nearest.get(), { 0, 0, 320, 240 }, { 1, 1, 1, 1 });
        }
    }

    void Scene::present(Frame& frame, const bool effect, const bool linear)
    {
        const auto pixel_width = frame.width();
        const auto pixel_height = frame.height();
        const double scale = std::min(static_cast<double>(pixel_width) / width, static_cast<double>(pixel_height) / height);
        const auto scaled_width = std::max(1u, static_cast<Uint32>(width * scale));
        const auto scaled_height = std::max(1u, static_cast<Uint32>(height * scale));
        SDL_GPUBlitInfo blit{};
        blit.source.texture = effect ? m_effect.get() : m_scene.get();
        blit.source.w = width;
        blit.source.h = height;
        blit.destination.texture = frame.texture();
        blit.destination.x = (pixel_width - scaled_width) / 2;
        blit.destination.y = (pixel_height - scaled_height) / 2;
        blit.destination.w = scaled_width;
        blit.destination.h = scaled_height;
        blit.load_op = SDL_GPU_LOADOP_CLEAR;
        blit.clear_color = { 0, 0, 0, 1 };
        blit.filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(frame.commandOutsidePass(), &blit);
    }

    std::vector<uint8_t> Scene::readback()
    {
        SDL_GPUTransferBufferCreateInfo info{};
        info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        info.size = width * height * 4;
        const TransferBuffer download(require(SDL_CreateGPUTransferBuffer(m_device, &info), "Create readback buffer"), { m_device });
        CopyCommand command(require(SDL_AcquireGPUCommandBuffer(m_device), "Acquire readback command"));
        {
            const auto pass = beginCopyPass(command.get());
            SDL_GPUTextureRegion source{};
            source.texture = m_effect.get();
            source.w = width;
            source.h = height;
            source.d = 1;
            SDL_GPUTextureTransferInfo destination{};
            destination.transfer_buffer = download.get();
            destination.pixels_per_row = width;
            destination.rows_per_layer = height;
            SDL_DownloadFromGPUTexture(pass.get(), &source, &destination);
        }
        const Fence fence(require(SDL_SubmitGPUCommandBufferAndAcquireFence(command.release()), "Submit readback command"), { m_device });
        SDL_GPUFence* const fences[] = { fence.get() };
        check(SDL_WaitForGPUFences(m_device, true, fences, 1), "Wait for readback fence");
        std::vector<uint8_t> pixels(info.size);
        const void* const mapped = require(SDL_MapGPUTransferBuffer(m_device, download.get(), false), "Map readback buffer");
        std::memcpy(pixels.data(), mapped, pixels.size());
        SDL_UnmapGPUTransferBuffer(m_device, download.get());
        return pixels;
    }
}
