#include "Core/Graphics/Runtime.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "SwapChain.hpp"
#include "core/Logger.hpp"
#include "imgui_impl_sdlgpu3.h"
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
#include "imgui_internal.h"
#endif

namespace core::Graphics::SDLGPU
{
    class Runtime final : public IGraphicsRuntime
    {
    public:
        explicit Runtime(IWindow* window)
            : m_window(window)
        {
            m_device.attach(new Device(window));
            m_renderer.attach(new Renderer(m_device.get()));
            m_swapchain.attach(new SwapChain(m_device.get()));
            m_swapchain->setRenderer(m_renderer.get());
            m_swapchain->applyRenderAttachment();
            m_device->setRecoveryService([this] {
                // Calls made while recording only schedule recovery.
                if(m_frame)
                    return false;
                return serviceRecovery() == RecoveryResult::Recovered;
            });
        }
        ~Runtime() override
        {
            m_device->setRecoveryService({});
            finishFrame();
            shutdownImGui();
            m_swapchain->setRenderer(nullptr);
        }
        IDevice* device() const noexcept override { return m_device.get(); }
        IRenderer* renderer() const noexcept override { return m_renderer.get(); }
        ISwapChain* swapChain() const noexcept override { return m_swapchain.get(); }
        GpuFrameStatistics statistics() const noexcept override { return {}; }
        RecoveryResult serviceRecovery() noexcept override
        {
            auto& policy = m_device->recovery();
            if(policy.state() == RecoveryState::Healthy)
                return RecoveryResult::NotNeeded;
            if(policy.state() == RecoveryState::Fatal || policy.state() == RecoveryState::Recovering)
                return RecoveryResult::Fatal;
            finishFrame();
            if(!policy.begin()) {
                Logger::error("[sdlgpu] Recovery budget exhausted; exiting");
                return RecoveryResult::Fatal;
            }
            const bool restore_imgui = m_imgui;
            try {
                Logger::info("[sdlgpu] Starting the single GPU recovery attempt");
                m_renderer->discardFrame();
                if(restore_imgui && ImGui::GetDrawData())
                    ImGui::GetDrawData()->Clear();
                const std::string driver = m_device->context().driver();
                m_device->notifyDestroy();
                shutdownImGui();
                m_device->releaseResources();
                m_device->context().close();
                m_device->context().reopen(m_window->getSDLWindow(), driver);
                m_device->rebuildResources();
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
                if(m_device->fail_reconstruction)
                    throw std::runtime_error("Injected reconstruction failure after resource rebuilding");
#endif
                if(restore_imgui && !initializeImGui())
                    throw std::runtime_error("Reinitialize ImGui GPU backend");
                if(policy.state() == RecoveryState::Fatal)
                    throw std::runtime_error("Resource rebuilding reported failure");
                m_device->notifyCreate();
                checkGpu(SDL_WaitForGPUIdle(m_device->gpu()), "Complete recovery uploads");
                if(!policy.complete())
                    throw std::runtime_error("Device recreation listener reported failure");
                Logger::info("[sdlgpu] GPU recovery complete; driver {}", driver);
                return RecoveryResult::Recovered;
            } catch(const std::exception& error) {
                Logger::error("[sdlgpu] GPU recovery failed: {}", error.what());
                policy.fatal();
                shutdownImGui();
                // Retain the device until all remaining wrappers have been destroyed.
                m_device->releaseResources();
                return RecoveryResult::Fatal;
            }
        }
        void drawRecoveryControls() override
        {
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
            const auto* viewport = ImGui::GetMainViewport();
            const float margin = ImGui::GetFontSize();
            ImGui::SetNextWindowPos(
                { viewport->WorkPos.x + viewport->WorkSize.x - margin, viewport->WorkPos.y + margin },
                ImGuiCond_Always,
                { 1, 0 });
            constexpr auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
            const bool visible = ImGui::Begin("SDL GPU recovery tests", nullptr, flags);
            // Keep the test controls visible above the Lua fixture and debugger windows.
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
            if(visible) {
                ImGui::TextUnformatted("One recovery per process. A second failure exits.");
                if(ImGui::Button("Fail next acquisition"))
                    m_device->inject(Device::FailurePoint::Acquisition);
                if(ImGui::Button("Fail next submission"))
                    m_device->inject(Device::FailurePoint::Submission);
                if(ImGui::Button("Fail next upload"))
                    m_device->inject(Device::FailurePoint::Upload);
                ImGui::Checkbox("Fail resource reconstruction", &m_device->fail_reconstruction);
            }
            ImGui::End();
#endif
        }
        FrameStatus beginFrame() noexcept override
        {
            try {
                if(m_frame || m_device->failed())
                    return FrameStatus::Failed;
                if(SDL_GetWindowFlags(m_window->getSDLWindow()) & SDL_WINDOW_MINIMIZED)
                    return FrameStatus::Skipped;
                m_swapchain->prepareFrame();
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
                m_device->checkpoint(Device::FailurePoint::Acquisition);
#endif
                m_frame = std::make_unique<Frame>(m_device->gpu(), m_window->getSDLWindow());
                if(!m_frame->texture() || !m_frame->width() || !m_frame->height()) {
                    finishFrame();
                    return m_device->failed() ? FrameStatus::Failed : FrameStatus::Skipped;
                }
                m_device->setFrame(m_frame.get());
                m_swapchain->applyRenderAttachment();
                m_swapchain->clearRenderAttachment();
                if(m_device->failed()) {
                    finishFrame();
                    return FrameStatus::Failed;
                }
                return FrameStatus::Ready;
            } catch(const std::exception& error) {
                m_device->fail(error);
                finishFrame();
                return FrameStatus::Failed;
            }
        }
        bool submitFrame(bool present) noexcept override
        {
            if(!m_frame)
                return false;
            try {
                if(!m_renderer->flush())
                    throw std::runtime_error("Flush sprites at frame submission");
                // An acquired SDL swapchain texture must be submitted. Even a skipped
                // presentation gets a defined canvas instead of uninitialized pixels.
                if(!m_swapchain->present())
                    throw std::runtime_error("Record canvas presentation");
                (void)present;
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
                m_device->checkpoint(Device::FailurePoint::Submission);
#endif
                m_frame->submit();
                finishFrame();
                return m_device->finishCaptures() && !m_device->failed();
            } catch(const std::exception& error) {
                m_device->fail(error);
                finishFrame();
                return false;
            }
        }
        bool initializeImGui() override
        {
            if(m_imgui)
                return true;
            ImGui_ImplSDLGPU3_InitInfo info{};
            info.Device = m_device->gpu();
            info.ColorTargetFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
            m_imgui = ImGui_ImplSDLGPU3_Init(&info);
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
            if(m_imgui) {
                ImGuiContextHook hook{};
                hook.Type = ImGuiContextHookType_EndFramePre;
                hook.UserData = this;
                hook.Callback = [](ImGuiContext*, ImGuiContextHook* current) {
                    static_cast<Runtime*>(current->UserData)->drawRecoveryControls();
                };
                m_hook = ImGui::AddContextHook(ImGui::GetCurrentContext(), &hook);
            }
#endif
            return m_imgui;
        }
        void shutdownImGui() noexcept override
        {
            if(m_imgui) {
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
                ImGui::RemoveContextHook(ImGui::GetCurrentContext(), m_hook);
                m_hook = 0;
#endif
                ImGui_ImplSDLGPU3_Shutdown();
                m_imgui = false;
            }
        }
        void newImGuiFrame() override { ImGui_ImplSDLGPU3_NewFrame(); }
        void renderImGui(ImDrawData* data) override
        {
            if(!m_frame || !data || !m_imgui)
                return;
            try {
                m_device->beforeTransfer();
                ImGui_ImplSDLGPU3_PrepareDrawData(data, m_frame->commandOutsidePass());
                SDL_GPUColorTargetInfo target{};
                target.texture = m_swapchain->canvas();
                target.load_op = SDL_GPU_LOADOP_LOAD;
                target.store_op = SDL_GPU_STOREOP_STORE;
                auto* pass = m_frame->beginRenderPass({ &target, 1 });
                // Render in the window's UI coordinates at the logical canvas resolution.
                ImDrawData scaled = *data;
                const auto size = m_swapchain->getCanvasSize();
                if(data->DisplaySize.x > 0 && data->DisplaySize.y > 0) {
                    scaled.FramebufferScale = { size.x / data->DisplaySize.x, size.y / data->DisplaySize.y };
                }
                ImGui_ImplSDLGPU3_RenderDrawData(&scaled, m_frame->command(), pass);
                m_frame->endPass();
            } catch(const std::exception& error) {
                m_device->fail(error);
            }
        }
        void drawImGuiImage(ITexture2D* texture, Vector2F size, Vector2F uv0, Vector2F uv1) override
        {
            auto* handle = static_cast<Texture2D*>(texture)->handle();
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(handle)),
                ImVec2(size.x, size.y),
                ImVec2(uv0.x, uv0.y),
                ImVec2(uv1.x, uv1.y));
        }

    private:
        void finishFrame() noexcept
        {
            m_device->setFrame(nullptr);
            if(m_frame) {
                try {
                    m_frame->finish();
                } catch(const std::exception& error) {
                    m_device->fail(error);
                }
            }
            m_frame.reset();
        }
        SmartReference<IWindow> m_window;
        SmartReference<Device> m_device;
        SmartReference<Renderer> m_renderer;
        SmartReference<SwapChain> m_swapchain;
        std::unique_ptr<Frame> m_frame;
        bool m_imgui{};
#ifdef LUASTG_SDLGPU_RECOVERY_TESTS
        ImGuiID m_hook{};
#endif
    };
}

namespace core::Graphics
{
    std::unique_ptr<IGraphicsRuntime> IGraphicsRuntime::create(IWindow* window)
    {
        return std::make_unique<SDLGPU::Runtime>(window);
    }
}
