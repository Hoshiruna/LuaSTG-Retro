#include "GpuContext.hpp"
#include "core/Logger.hpp"
#include <string_view>
#include <utility>

namespace core::Graphics::SDLGPU
{
    Frame::Frame(SDL_GPUDevice* const device, SDL_Window* const window)
    {
        m_command = require(SDL_AcquireGPUCommandBuffer(device), "SDL_AcquireGPUCommandBuffer");
        if(!SDL_WaitAndAcquireGPUSwapchainTexture(m_command, window, &m_texture, &m_width, &m_height)) {
            const std::string message = std::string("SDL_WaitAndAcquireGPUSwapchainTexture: ") + SDL_GetError();
            SDL_CancelGPUCommandBuffer(m_command);
            m_command = nullptr;
            throw std::runtime_error(message);
        }
    }

    Frame::~Frame()
    {
        endPass();
        if(m_command != nullptr) {
            const bool result = m_texture != nullptr ? SDL_SubmitGPUCommandBuffer(m_command) : SDL_CancelGPUCommandBuffer(m_command);
            if(!result) {
                Logger::error("[sdlgpu] Failed to finish abandoned frame: {}", SDL_GetError());
            }
        }
    }

    SDL_GPUCommandBuffer* Frame::commandOutsidePass()
    {
        if(m_command == nullptr) {
            throw std::logic_error("Cannot record commands after GPU frame submission");
        }
        endPass();
        return m_command;
    }

    SDL_GPURenderPass* Frame::beginRenderPass(const std::span<const SDL_GPUColorTargetInfo> targets, const SDL_GPUDepthStencilTargetInfo* const depth)
    {
        auto* const buffer = commandOutsidePass();
        m_render_pass = require(SDL_BeginGPURenderPass(buffer, targets.data(), static_cast<Uint32>(targets.size()), depth), "SDL_BeginGPURenderPass");
        return m_render_pass;
    }

    SDL_GPUCopyPass* Frame::beginCopyPass()
    {
        m_copy_pass = require(SDL_BeginGPUCopyPass(commandOutsidePass()), "SDL_BeginGPUCopyPass");
        return m_copy_pass;
    }

    void Frame::endPass() noexcept
    {
        if(m_render_pass != nullptr) {
            SDL_EndGPURenderPass(std::exchange(m_render_pass, nullptr));
        }
        if(m_copy_pass != nullptr) {
            SDL_EndGPUCopyPass(std::exchange(m_copy_pass, nullptr));
        }
    }

    void Frame::submit()
    {
        commandOutsidePass();
        check(SDL_SubmitGPUCommandBuffer(std::exchange(m_command, nullptr)), "SDL_SubmitGPUCommandBuffer");
    }

    GpuContext::GpuContext(SDL_Window* const window, const char* const requested_driver, const bool debug)
    {
        // Shadercross supplies DXIL, SPIR-V, or MSL; ImGui also supplies DXBC.
        constexpr SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
        m_device.reset(require(SDL_CreateGPUDevice(formats, debug, requested_driver), "SDL_CreateGPUDevice"));
        if(requested_driver != nullptr && std::string_view(SDL_GetGPUDeviceDriver(m_device.get())) != requested_driver) {
            throw std::runtime_error("SDL selected a GPU driver different from the requested driver");
        }
        check(SDL_ClaimWindowForGPUDevice(m_device.get(), window), "SDL_ClaimWindowForGPUDevice");
        m_window = window;
        Logger::info("[sdlgpu] GPU driver: {}", driver());
    }

    GpuContext::~GpuContext()
    {
        if(!SDL_WaitForGPUIdle(m_device.get())) {
            Logger::error("[sdlgpu] SDL_WaitForGPUIdle during shutdown: {}", SDL_GetError());
        }
        SDL_ReleaseWindowFromGPUDevice(m_device.get(), m_window);
    }

    SDL_GPUTextureFormat GpuContext::swapchainFormat() const
    {
        return SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_window);
    }

    const char* GpuContext::driver() const
    {
        return SDL_GetGPUDeviceDriver(m_device.get());
    }

    bool GpuContext::setVSync(const bool enabled)
    {
        const auto mode = enabled ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE;
        if(!SDL_WindowSupportsGPUPresentMode(m_device.get(), m_window, mode)) {
            Logger::warn("[sdlgpu] Requested presentation mode is unavailable");
            return false;
        }
        check(SDL_SetGPUSwapchainParameters(m_device.get(), m_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode), "SDL_SetGPUSwapchainParameters");
        return true;
    }
}
