#pragma once

#include "Resources.hpp"
#include "ShaderCompiler.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace core::Graphics::SDLGPU
{
    class Scene final
    {
    public:
        static constexpr Uint32 width = 320;
        static constexpr Uint32 height = 240;

        Scene(SDL_GPUDevice* device, const ShaderCompiler& compiler);
        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;

        void render(SDL_GPUCommandBuffer* command, bool linear);
        void present(SDL_GPUCommandBuffer* command, SDL_GPUTexture* swapchain, Uint32 pixel_width, Uint32 pixel_height, bool effect, bool linear);
        std::vector<uint8_t> readback();

    private:
        Texture createTexture(Uint32 texture_width, Uint32 texture_height, SDL_GPUTextureUsageFlags usage);
        Sampler createSampler(SDL_GPUFilter filter);
        Pipeline createPipeline(SDL_GPUShader* vertex, SDL_GPUShader* fragment, bool blend);
        void uploadTextures();
        void drawQuad(SDL_GPUCommandBuffer* command, SDL_GPURenderPass* pass, SDL_GPUTexture* texture, SDL_GPUSampler* sampler, const std::array<float, 4>& rectangle, const std::array<float, 4>& tint);

        SDL_GPUDevice* m_device{};
        Texture m_pattern;
        Texture m_white;
        Texture m_scene;
        Texture m_effect;
        Sampler m_nearest;
        Sampler m_linear;
        Pipeline m_textured;
        Pipeline m_grayscale;
    };
}
