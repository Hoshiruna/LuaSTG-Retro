#include "Core/Graphics/Runtime.hpp"
#include "core/FileSystem.hpp"
#include "core/SdlRuntime.hpp"
#include "core/SmartReference.hpp"
#include "sdl/EventDispatcher.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <objbase.h>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

namespace
{
    void check(bool value, const char* operation)
    {
        if(!value)
            throw std::runtime_error(operation);
    }

    // The D3D11 image writer uses COM. Destroy it after all graphics resources.
    struct ComScope
    {
        HRESULT result{ CoInitializeEx(nullptr, COINIT_MULTITHREADED) };
        ~ComScope()
        {
            if(SUCCEEDED(result))
                CoUninitialize();
        }
    };

    int capture(const char* output)
    {
        using namespace core;
        using namespace core::Graphics;
        ComScope com;
        SdlRuntime sdl;
        check(sdl.initialize(), "Initialize SDL");
        SmartReference<IWindow> window;
        check(IWindow::create({ 512, 256 }, "Model lighting and transforms", WindowFrameStyle::Normal, true, window.put()), "Create capture window");
        auto runtime = IGraphicsRuntime::create(window.get());
        check(runtime->swapChain()->setCanvasSize({ 512, 256 }), "Set capture canvas");
        int drawable_width{}, drawable_height{};
        check(SDL_GetWindowSizeInPixels(window->getSDLWindow(), &drawable_width, &drawable_height), "Get capture drawable size");
        check(drawable_width > 0 && drawable_height > 0, "Capture drawable is empty");
        check(runtime->swapChain()->setWindowMode({ static_cast<uint32_t>(drawable_width), static_cast<uint32_t>(drawable_height) }), "Initialize capture swapchain");
        runtime->swapChain()->setVSync(false);
        FileSystemManager::addSearchPath(LUASTG_GEOMETRY_FIXTURES);
        SmartReference<IModel> model;
        auto* renderer = runtime->renderer();
        // D3D11 only accepts 16/32-bit model indices; both assets describe the same quad.
#ifdef LUASTG_GRAPHICS_SDLGPU
        check(renderer->createModel("solid-index8.gltf", model.put()), "Load 8-bit model index fixture");
#else
        check(renderer->createModel("solid.gltf", model.put()), "Load project-owned model fixture");
#endif
        const auto deadline = SDL_GetTicks() + 10000;
        while(SDL_GetTicks() < deadline) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                SDLEventDispatcher::dispatch(event);
                if(event.type == SDL_EVENT_QUIT)
                    throw std::runtime_error("Capture cancelled");
            }
            const auto status = runtime->beginFrame();
            if(status == FrameStatus::Failed)
                throw std::runtime_error("Begin capture frame");
            if(status == FrameStatus::Skipped) {
                SDL_Delay(10);
                continue;
            }
            check(renderer->beginBatch(), "Begin capture batch");
            renderer->clearRenderTarget(Color4B(16, 24, 32, 255));
            renderer->clearDepthBuffer(1);
            for(int tile = 0; tile < 8; ++tile) {
                const float x = float(tile % 4 * 128), y = float(tile / 4 * 128);
                renderer->setViewport({ x, y, 0, x + 128, y + 128, 1 });
                renderer->setScissorRect({ x + 4, y + 4, x + 124, y + 124 });
                renderer->setPerspective({ 0, 0, -2 }, { 0, 0, 0 }, { 0, 1, 0 }, 1.570796327f, 1, 0.1f, 10);
                renderer->setDepthState(IRenderer::DepthState::Enable);
                renderer->setFogState(IRenderer::FogState::Disable, {}, 0, 1);
                model->setPosition({ 0, 0, 0 });
                model->setScaling({ 1, 1, 1 });
                model->setRotationQuaternion({ 0, 0, 0, 1 });
                model->setAmbient({ 1, 1, 1 }, 1);
                model->setDirectionalLight({ 0, 0, -1 }, { 1, 1, 1 }, 0);
                if(tile == 1)
                    model->setAmbient({ 1, 0.5f, 0.25f }, 0.5f);
                if(tile == 2) {
                    model->setAmbient({ 1, 1, 1 }, 0.25f);
                    model->setDirectionalLight({ 0, 0, -1 }, { 1, 0.5f, 0.25f }, 0.75f);
                }
                if(tile >= 3 && tile <= 5) {
                    renderer->setFogState(static_cast<IRenderer::FogState>(tile - 2), Color4B(32, 64, 96, 255), tile == 3 ? 1.0f : 0.35f, 4);
                }
                if(tile == 6)
                    model->setRotationRollPitchYaw(0.5f, 0, 0);
                if(tile == 7) {
                    model->setRotationQuaternion({ 0, 0, 0.24740396f, 0.96891242f });
                    model->setScaling({ 0.75f, 1, 1 });
                    model->setPosition({ 0.1f, 0, 0 });
                }
                check(renderer->drawModel(model.get()), "Draw model lighting/transform case");
            }
            check(renderer->endBatch(), "End capture batch");
            check(runtime->swapChain()->saveSnapshotToFile(output), "Save model lighting capture");
            check(runtime->submitFrame(true), "Submit model lighting capture");
            check(std::filesystem::is_regular_file(output), "Capture file was not written");
            spdlog::info("Saved {} (8 model lighting, fog and transform cases)", output);
            return 0;
        }
        throw std::runtime_error("No drawable window within 10 seconds");
    }
}

int
main(int argc, char** argv)
{
    try {
        auto logger = spdlog::stdout_color_mt("geometry-capture");
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(logger);
        if(argc > 2)
            throw std::runtime_error("Usage: Core.Graphics.GeometryCapture.exe [output.png]");
        return capture(argc == 2 ? argv[1] : "geometry-lighting.png");
    } catch(std::exception const& error) {
        if(spdlog::default_logger_raw())
            spdlog::error("Geometry capture failed: {}", error.what());
        else
            std::fprintf(stderr, "Geometry capture failed: %s\n", error.what());
        return 1;
    }
}
