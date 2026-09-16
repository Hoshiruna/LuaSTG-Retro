#pragma once

#include <SDL3/SDL.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace core::Graphics::SDLGPU
{
    inline void check(const bool result, const std::string_view operation)
    {
        if(!result) {
            throw std::runtime_error(std::string(operation) + ": " + SDL_GetError());
        }
    }

    template<typename T>
    T* require(T* const pointer, const std::string_view operation)
    {
        check(pointer != nullptr, operation);
        return pointer;
    }

    template<typename T, auto Release>
    struct GpuDeleter
    {
        SDL_GPUDevice* device{};

        void operator()(T* const resource) const noexcept
        {
            Release(device, resource);
        }
    };

    template<typename T, auto Release>
    using GpuResource = std::unique_ptr<T, GpuDeleter<T, Release>>;

    using Texture = GpuResource<SDL_GPUTexture, SDL_ReleaseGPUTexture>;
    using Sampler = GpuResource<SDL_GPUSampler, SDL_ReleaseGPUSampler>;
    using Shader = GpuResource<SDL_GPUShader, SDL_ReleaseGPUShader>;
    using Pipeline = GpuResource<SDL_GPUGraphicsPipeline, SDL_ReleaseGPUGraphicsPipeline>;
    using TransferBuffer = GpuResource<SDL_GPUTransferBuffer, SDL_ReleaseGPUTransferBuffer>;
    using Fence = GpuResource<SDL_GPUFence, SDL_ReleaseGPUFence>;
    using RenderPass = std::unique_ptr<SDL_GPURenderPass, decltype(&SDL_EndGPURenderPass)>;
    using CopyPass = std::unique_ptr<SDL_GPUCopyPass, decltype(&SDL_EndGPUCopyPass)>;

    inline RenderPass beginRenderPass(SDL_GPUCommandBuffer* const command, const SDL_GPUColorTargetInfo& target)
    {
        return { require(SDL_BeginGPURenderPass(command, &target, 1, nullptr), "SDL_BeginGPURenderPass"), SDL_EndGPURenderPass };
    }

    inline CopyPass beginCopyPass(SDL_GPUCommandBuffer* const command)
    {
        return { require(SDL_BeginGPUCopyPass(command), "SDL_BeginGPUCopyPass"), SDL_EndGPUCopyPass };
    }
}
