#include "Core/Graphics/Runtime.hpp"
#include "Core/Graphics/Direct3D11/Device.hpp"
#include "Core/Graphics/Direct3D11/FrameQuery.hpp"
#include "Core/Graphics/Direct3D11/Texture2D.hpp"
#include "Core/Graphics/Renderer_D3D11.hpp"
#include "Core/Graphics/SwapChain_D3D11.hpp"
#include "core/SmartReference.hpp"
#include "imgui_impl_dx11.h"
#include <SDL3/SDL.h>
#include <array>
#include <stdexcept>

namespace core::Graphics
{
    class D3D11Runtime final : public IGraphicsRuntime
    {
    public:
        explicit D3D11Runtime(IWindow* window)
            : m_window(window)
        {
            if(!Direct3D11::Device::create({}, m_device.put()) ||
                !SwapChain_D3D11::create(window, m_device.get(), m_swapchain.put()) ||
                !Renderer_D3D11::create(m_device.get(), m_renderer.put())) {
                throw std::runtime_error("Create D3D11 graphics runtime");
            }
            for(auto& query : m_queries) {
                query = std::make_unique<Direct3D11::FrameQuery>(m_device.get());
            }
        }

        ~D3D11Runtime() override { shutdownImGui(); }
        IDevice* device() const noexcept override { return m_device.get(); }
        IRenderer* renderer() const noexcept override { return m_renderer.get(); }
        ISwapChain* swapChain() const noexcept override { return m_swapchain.get(); }

        FrameStatus beginFrame() noexcept override
        {
            if(m_active) {
                spdlog::error("[graphics] A frame is already active");
                return FrameStatus::Failed;
            }
            if(SDL_GetWindowFlags(m_window->getSDLWindow()) & SDL_WINDOW_MINIMIZED) {
                return FrameStatus::Skipped;
            }
            m_query_index = (m_query_index + 1) % m_queries.size();
            m_queries[m_query_index]->begin();
            m_swapchain->applyRenderAttachment();
            m_swapchain->clearRenderAttachment();
            m_active = true;
            return FrameStatus::Ready;
        }

        bool submitFrame(bool present) noexcept override
        {
            if(!m_active) {
                return false;
            }
            m_queries[m_query_index]->end();
            m_active = false;
            const bool result = !present || m_swapchain->present();
            TracyD3D11Collect(m_device->GetTracyContext());
            return result;
        }

        GpuFrameStatistics statistics() const noexcept override
        {
            auto const& query = m_queries[m_query_index];
            return { query->getTime(), query->isAvailable() };
        }

        bool initializeImGui() override
        {
            if(!m_imgui) {
                m_imgui = ImGui_ImplDX11_Init(m_device->GetD3D11Device(), m_device->GetD3D11DeviceContext());
            }
            return m_imgui;
        }
        void shutdownImGui() noexcept override
        {
            if(m_imgui) {
                ImGui_ImplDX11_Shutdown();
                m_imgui = false;
            }
        }
        void newImGuiFrame() override { ImGui_ImplDX11_NewFrame(); }
        void renderImGui(ImDrawData* data) override { ImGui_ImplDX11_RenderDrawData(data); }
        void drawImGuiImage(ITexture2D* texture, Vector2F size, Vector2F uv0, Vector2F uv1) override
        {
            auto* const view = static_cast<Direct3D11::Texture2D*>(texture)->GetView();
            ImGui::Image(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(view)),
                ImVec2(size.x, size.y),
                ImVec2(uv0.x, uv0.y),
                ImVec2(uv1.x, uv1.y));
        }

    private:
        SmartReference<IWindow> m_window;
        SmartReference<Direct3D11::Device> m_device;
        SmartReference<SwapChain_D3D11> m_swapchain;
        SmartReference<Renderer_D3D11> m_renderer;
        std::array<std::unique_ptr<Direct3D11::FrameQuery>, 2> m_queries;
        size_t m_query_index{};
        bool m_active{};
        bool m_imgui{};
    };

    std::unique_ptr<IGraphicsRuntime> IGraphicsRuntime::create(IWindow* window, StringView renderer_driver)
    {
        // Direct3D 11 is a single driver, so there is nothing to select between.
        (void)renderer_driver;
        return std::make_unique<D3D11Runtime>(window);
    }
}
