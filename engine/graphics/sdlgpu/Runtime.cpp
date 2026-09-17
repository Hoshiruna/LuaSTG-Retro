#include "Core/Graphics/Runtime.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "SwapChain.hpp"
#include "core/Logger.hpp"
#include "imgui_impl_sdlgpu3.h"

namespace core::Graphics::SDLGPU
{
    class Runtime final : public IGraphicsRuntime
    {
    public:
        Runtime(IWindow* window, StringView renderer_driver)
            : m_window(window)
        {
            m_device.attach(new Device(window, renderer_driver));
            m_renderer.attach(new Renderer(m_device.get()));
            m_swapchain.attach(new SwapChain(m_device.get()));
            m_swapchain->setRenderer(m_renderer.get());
            m_swapchain->applyRenderAttachment();
        }
        ~Runtime() override
        {
            finishFrame();
            shutdownImGui();
            m_swapchain->setRenderer(nullptr);
        }
        IDevice* device() const noexcept override { return m_device.get(); }
        IRenderer* renderer() const noexcept override { return m_renderer.get(); }
        ISwapChain* swapChain() const noexcept override { return m_swapchain.get(); }
        GpuFrameStatistics statistics() const noexcept override { return {}; }
        FrameStatus beginFrame() noexcept override
        {
            try {
                if(m_frame || m_device->failed())
                    return FrameStatus::Failed;
                if(SDL_GetWindowFlags(m_window->getSDLWindow()) & SDL_WINDOW_MINIMIZED)
                    return FrameStatus::Skipped;
                m_swapchain->prepareFrame();
                m_frame = std::make_unique<Frame>(m_device->gpu(), m_window->getSDLWindow());
                if(!m_frame->texture() || !m_frame->width() || !m_frame->height()) {
                    finishFrame();
                    return FrameStatus::Skipped;
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
            return m_imgui;
        }
        void shutdownImGui() noexcept override
        {
            if(m_imgui) {
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
            m_frame.reset();
        }
        SmartReference<IWindow> m_window;
        SmartReference<Device> m_device;
        SmartReference<Renderer> m_renderer;
        SmartReference<SwapChain> m_swapchain;
        std::unique_ptr<Frame> m_frame;
        bool m_imgui{};
    };
}

namespace core::Graphics
{
    std::unique_ptr<IGraphicsRuntime> IGraphicsRuntime::create(IWindow* window, StringView renderer_driver)
    {
        return std::make_unique<SDLGPU::Runtime>(window, renderer_driver);
    }
    bool IMesh::create(IDevice*, MeshOptions const&, IMesh** output)
    {
        if(output)
            *output = nullptr;
        Logger::error("[sdlgpu] Meshes are not supported in the core 2D milestone");
        return false;
    }
    bool IMeshRenderer::create(IDevice*, IMeshRenderer** output)
    {
        if(output)
            *output = nullptr;
        Logger::error("[sdlgpu] Mesh rendering is not supported in the core 2D milestone");
        return false;
    }
}
