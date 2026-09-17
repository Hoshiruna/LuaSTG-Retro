#include "Options.hpp"
#include "Verification.hpp"
#include "GpuContext.hpp"
#include "Scene.hpp"
#include "ShaderCompiler.hpp"
#include "core/Application.hpp"
#include "core/InputSystem.hpp"
#include "core/Logger.hpp"
#include "core/SmartReference.hpp"
#include "core/Window.hpp"
#include "sdl/EventDispatcher.hpp"
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <utility>

namespace core::Graphics::SDLGPU::smoke
{
    class Application final : public IApplication, public ISDLEventListener
    {
    public:
        explicit Application(Options options)
            : m_options(std::move(options))
        {
        }

        ~Application()
        {
            shutdown();
        }

        int exitCode() const noexcept { return m_exit_code; }

        bool onCreate() override
        {
            try {
                if(!IWindow::create({ 960, 720 }, "LuaSTG SDL GPU smoke", WindowFrameStyle::Normal, true, m_window.put())) {
                    throw std::runtime_error("Could not create the SDL smoke window");
                }
                // The context first, because the compiler's back ends depend on the driver.
                m_gpu = std::make_unique<GpuContext>(m_window->getSDLWindow(), m_options.driver, true);
                m_compiler = std::make_unique<ShaderCompiler>(m_gpu->driver());
                m_scene = std::make_unique<Scene>(m_gpu->device(), *m_compiler);

                IMGUI_CHECKVERSION();
                require(ImGui::CreateContext(), "ImGui::CreateContext");
                m_imgui_context = true;
                ImGui::GetIO().IniFilename = nullptr;
                ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                ImGui::StyleColorsDark();
                m_base_style = ImGui::GetStyle();
                updateDpi();
                check(ImGui_ImplSDL3_InitForSDLGPU(m_window->getSDLWindow()), "ImGui_ImplSDL3_InitForSDLGPU");
                m_imgui_platform = true;
                ImGui_ImplSDLGPU3_InitInfo info{};
                info.Device = m_gpu->device();
                info.ColorTargetFormat = m_gpu->swapchainFormat();
                check(ImGui_ImplSDLGPU3_Init(&info), "ImGui_ImplSDLGPU3_Init");
                m_imgui_renderer = true;
                SDLEventDispatcher::addListener(this);
                m_listening = true;
                m_started = true;
                return true;
            } catch(const std::exception& error) {
                fail(error.what());
                return false;
            }
        }

        void onBeforeUpdate() override {}

        void processSDLEvent(const SDL_Event& event) override
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
        }

        bool onUpdate() override
        {
            try {
                if((SDL_GetWindowFlags(m_window->getSDLWindow()) & SDL_WINDOW_MINIMIZED) != 0) {
                    SDL_Delay(10);
                    return true;
                }
                updateDpi();
                ImGui_ImplSDLGPU3_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::NewFrame();
                controls();
                ImGui::Render();
                const auto& io = ImGui::GetIO();
                InputSystem::getInstance().setGameplayCapture(io.WantCaptureKeyboard, io.WantCaptureMouse);

                {
                    auto frame = m_gpu->acquire();
                    if(frame.texture() == nullptr || frame.width() == 0 || frame.height() == 0) {
                        SDL_Delay(10);
                        return true;
                    }
                    auto* const draw_data = ImGui::GetDrawData();
                    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, frame.commandOutsidePass());
                    m_scene->render(frame, m_linear);
                    m_scene->present(frame, m_effect, m_linear);
                    SDL_GPUColorTargetInfo target{};
                    target.texture = frame.texture();
                    target.load_op = SDL_GPU_LOADOP_LOAD;
                    target.store_op = SDL_GPU_STOREOP_STORE;
                    {
                        auto* const pass = frame.beginRenderPass({ &target, 1 });
                        ImGui_ImplSDLGPU3_RenderDrawData(draw_data, frame.command(), pass);
                    }
                    frame.submit();
                }
                ++m_rendered_frames;
                if(m_options.frames && m_rendered_frames >= *m_options.frames) {
                    m_completed = true;
                    return false;
                }
                return true;
            } catch(const std::exception& error) {
                fail(error.what());
                return false;
            }
        }

        void onDestroy() override
        {
            if(m_started && !m_failed) {
                try {
                    if(m_rendered_frames == 0 || (m_options.frames && !m_completed)) {
                        throw std::runtime_error("Window closed before the requested frames completed");
                    }
                    if(m_options.verify || !m_options.capture.empty()) {
                        const auto pixels = m_scene->readback();
                        if(!m_options.capture.empty()) {
                            saveCapture(pixels, m_options.capture);
                        }
                        if(m_options.verify && !verifyPixels(pixels)) {
                            throw std::runtime_error("Grayscale canvas verification failed");
                        }
                    }
                    check(SDL_WaitForGPUIdle(m_gpu->device()), "SDL_WaitForGPUIdle");
                    Logger::info("[sdlgpu] Completed {} frames on {}", m_rendered_frames, m_gpu->driver());
                    m_exit_code = EXIT_SUCCESS;
                } catch(const std::exception& error) {
                    fail(error.what());
                }
            }
            shutdown();
        }

    private:
        void fail(const char* const message)
        {
            m_failed = true;
            m_exit_code = EXIT_FAILURE;
            Logger::error("[sdlgpu] {}", message);
            std::fprintf(stderr, "SDL GPU smoke: %s\n", message);
        }

        void updateDpi()
        {
            const float scale = m_window->getDPIScaling();
            if(scale == m_dpi_scale) {
                return;
            }
            m_dpi_scale = scale;
            ImGui::GetStyle() = m_base_style;
            ImGui::GetStyle().ScaleAllSizes(scale);
            ImGui::GetStyle().FontScaleDpi = scale;
        }

        void controls()
        {
            ImGui::SetNextWindowPos(ImVec2(12, 12), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(300 * m_dpi_scale, 0), ImGuiCond_FirstUseEver);
            if(ImGui::Begin("SDL GPU smoke", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Driver: %s", m_gpu->driver());
                const auto size = m_window->getPixelSize();
                ImGui::Text("Drawable: %u x %u", size.x, size.y);
                ImGui::Text("Canvas: %u x %u", Scene::width, Scene::height);
                ImGui::Text("DPI scale: %.2f", m_dpi_scale);
                ImGui::Text("Submitted frames: %llu", static_cast<unsigned long long>(m_rendered_frames));
                ImGui::Checkbox("Grayscale post-effect", &m_effect);
                ImGui::Checkbox("Linear sampling", &m_linear);
                bool vsync = m_vsync;
                if(ImGui::Checkbox("VSync", &vsync)) {
                    if(m_gpu->setVSync(vsync)) {
                        m_vsync = vsync;
                    }
                }
                bool fullscreen = (SDL_GetWindowFlags(m_window->getSDLWindow()) & SDL_WINDOW_FULLSCREEN) != 0;
                if(ImGui::Checkbox("Fullscreen", &fullscreen)) {
                    check(fullscreen ? m_window->setFullScreenMode() : m_window->setWindowMode({ 960, 720 }), "Change fullscreen state");
                }
                ImGui::TextUnformatted("Corners: red / green / blue / white");
                ImGui::TextUnformatted("Center: overlapping translucent quads");
            }
            ImGui::End();
        }

        void shutdown() noexcept
        {
            if(m_listening) {
                SDLEventDispatcher::removeListener(this);
                m_listening = false;
            }
            if(m_gpu && !SDL_WaitForGPUIdle(m_gpu->device())) {
                std::fprintf(stderr, "SDL GPU smoke shutdown: %s\n", SDL_GetError());
                m_exit_code = EXIT_FAILURE;
            }
            if(m_imgui_renderer) {
                ImGui_ImplSDLGPU3_Shutdown();
                m_imgui_renderer = false;
            }
            if(m_imgui_platform) {
                ImGui_ImplSDL3_Shutdown();
                m_imgui_platform = false;
            }
            if(m_imgui_context) {
                ImGui::DestroyContext();
                m_imgui_context = false;
            }
            m_scene.reset();
            m_gpu.reset();
            m_compiler.reset();
            m_window.reset();
        }

        Options m_options;
        SmartReference<IWindow> m_window;
        std::unique_ptr<ShaderCompiler> m_compiler;
        std::unique_ptr<GpuContext> m_gpu;
        std::unique_ptr<Scene> m_scene;
        ImGuiStyle m_base_style;
        uint64_t m_rendered_frames{};
        float m_dpi_scale{};
        int m_exit_code{ EXIT_FAILURE };
        bool m_imgui_context{};
        bool m_imgui_platform{};
        bool m_imgui_renderer{};
        bool m_listening{};
        bool m_started{};
        bool m_failed{};
        bool m_completed{};
        bool m_effect{ true };
        bool m_linear{};
        bool m_vsync{ true };
    };
}

int
main(const int argc, char* argv[])
{
    using namespace core::Graphics::SDLGPU::smoke;
    try {
        auto options = parseOptions(argc, argv);
        if(options.help) {
            printUsage();
            return EXIT_SUCCESS;
        }
        // The engine builds spdlog without a default logger.
        spdlog::set_default_logger(spdlog::stdout_color_mt("sdlgpu-smoke"));
        Application application(std::move(options));
        core::ApplicationManager::run(&application);
        return application.exitCode();
    } catch(const std::exception& error) {
        std::fprintf(stderr, "SDL GPU smoke: %s\n", error.what());
        return EXIT_FAILURE;
    }
}
