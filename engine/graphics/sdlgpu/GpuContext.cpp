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

    namespace
    {
        // SDL reports every driver compiled into it, whether or not this machine can run it.
        std::string listDrivers()
        {
            std::string names;
            for(int i = 0; i < SDL_GetNumGPUDrivers(); ++i) {
                const char* const name = SDL_GetGPUDriver(i);
                if(name == nullptr) {
                    continue;
                }
                if(!names.empty()) {
                    names += ", ";
                }
                names += name;
            }
            return names.empty() ? std::string("none") : names;
        }

        // What "auto" tries, in order. Direct3D 12 leads on Windows because it is the closest
        // relative of the Direct3D 11 backend this one replaces, so it is the better-tested
        // path on the vendor drivers our users already run.
        std::span<const char* const> preferredDrivers()
        {
#if defined(_WIN32)
            static constexpr const char* order[]{ "direct3d12", "vulkan" };
#elif defined(__APPLE__)
            static constexpr const char* order[]{ "metal" };
#else
            static constexpr const char* order[]{ "vulkan" };
#endif
            return order;
        }
    }

    GpuContext::GpuContext(SDL_Window* const window, const std::string_view requested, const bool debug)
    {
        if(requested == "auto") {
            probe(window, debug);
        } else if(requested.empty()) {
            open(window, nullptr, debug);
        } else {
            const std::string name(requested);
            try {
                open(window, name.c_str(), debug);
            } catch(const std::exception& error) {
                // Asking for a specific driver and getting a different one silently is worse
                // than not starting, so name the driver and say what this build can offer.
                throw std::runtime_error("Cannot use the requested GPU driver \"" + name + "\": " + error.what()
                    + ". Drivers built into SDL: " + listDrivers());
            }
        }
        Logger::info("[sdlgpu] GPU driver: {}", driver());
    }

    void GpuContext::open(SDL_Window* const window, const char* const requested_driver, const bool debug)
    {
        // Shadercross supplies DXIL, SPIR-V, or MSL; ImGui also supplies DXBC.
        constexpr SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
        m_device.reset(require(SDL_CreateGPUDevice(formats, debug, requested_driver), "SDL_CreateGPUDevice"));
        if(requested_driver != nullptr && std::string_view(SDL_GetGPUDeviceDriver(m_device.get())) != requested_driver) {
            throw std::runtime_error("SDL selected a GPU driver different from the requested driver");
        }
        if(window != nullptr) {
            claimWindow(window);
        }
    }

    void GpuContext::probe(SDL_Window* const window, const bool debug)
    {
        std::string attempts;
        const auto attempt = [&](const char* const candidate) {
            try {
                open(window, candidate, debug);
                return true;
            } catch(const std::exception& error) {
                const char* const name = candidate != nullptr ? candidate : "SDL's own choice";
                Logger::warn("[sdlgpu] GPU driver {} is unusable: {}", name, error.what());
                attempts += std::string("\n  ") + name + ": " + error.what();
                // Claiming the window is the last step of open(), so a failed attempt leaves
                // at most a device behind and m_window still null. Drop it and try the next.
                m_device.reset();
                return false;
            }
        };
        for(const char* const candidate : preferredDrivers()) {
            if(attempt(candidate)) {
                return;
            }
        }
        // The preference list is not exhaustive, and SDL may know a driver we never named.
        if(attempt(nullptr)) {
            return;
        }
        throw std::runtime_error("No usable GPU driver. Drivers built into SDL: " + listDrivers() + ". Attempts:" + attempts);
    }

    GpuContext::~GpuContext()
    {
        if(!SDL_WaitForGPUIdle(m_device.get())) {
            Logger::error("[sdlgpu] SDL_WaitForGPUIdle during shutdown: {}", SDL_GetError());
        }
        if(m_window != nullptr) {
            SDL_ReleaseWindowFromGPUDevice(m_device.get(), m_window);
        }
    }

    void GpuContext::claimWindow(SDL_Window* window)
    {
        if(m_window != nullptr || window == nullptr) {
            throw std::logic_error("GPU context requires one non-null window");
        }
        check(SDL_ClaimWindowForGPUDevice(m_device.get(), window), "SDL_ClaimWindowForGPUDevice");
        m_window = window;
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
