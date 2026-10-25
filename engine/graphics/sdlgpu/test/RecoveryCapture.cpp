#include "Core/Graphics/Runtime.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "Device.hpp"
#include "Model.hpp"
#include "PostEffectShader.hpp"
#include "core/FileSystem.hpp"
#include "core/SdlRuntime.hpp"
#include "core/SmartReference.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string_view>

namespace
{
    using namespace core;
    using namespace core::Graphics;
    using namespace core::Graphics::SDLGPU;

    void require(bool value, const char* operation)
    {
        if(!value)
            throw std::runtime_error(operation);
    }

    void capture(IGraphicsRuntime& runtime, ITexture2D* texture, IModel* model, IRenderTarget* target, IPostEffectShader* effect, const char* path)
    {
        const auto deadline = SDL_GetTicks() + 10000;
        while(true) {
            SDL_Event event{};
            while(SDL_PollEvent(&event))
                if(event.type == SDL_EVENT_QUIT)
                    throw std::runtime_error("Capture cancelled");
            const auto status = runtime.beginFrame();
            if(status == FrameStatus::Ready)
                break;
            require(status == FrameStatus::Skipped && SDL_GetTicks() < deadline, "Begin capture frame");
            SDL_Delay(10);
        }
        auto* renderer = runtime.renderer();
        renderer->setOrtho({ 0, 0, 0, 128, 128, 1 });
        renderer->setViewport({ 0, 0, 0, 128, 128, 1 });
        renderer->setScissorRect({ 0, 0, 128, 128 });
        renderer->setDepthState(IRenderer::DepthState::Disable);
        renderer->setBlendState(IRenderer::BlendState::One);
        renderer->setTexture(texture);
        require(renderer->beginBatch(), "Begin capture batch");
        renderer->clearRenderTarget(Color4B(16, 24, 32, 255));
        const IRenderer::DrawVertex vertices[]{
            { 16, 16, 0, 0 },
            { 112, 16, 1, 0 },
            { 112, 112, 1, 1 },
            { 16, 112, 0, 1 },
        };
        require(renderer->drawQuad(vertices), "Draw capture quad");
        renderer->setViewport({ 32, 32, 0, 96, 96, 1 });
        renderer->setScissorRect({ 32, 32, 96, 96 });
        renderer->setPerspective({ 0, 0, -2 }, { 0, 0, 0 }, { 0, 1, 0 }, 1.570796327f, 1, 0.1f, 10);
        renderer->setDepthState(IRenderer::DepthState::Enable);
        require(renderer->drawModel(model), "Draw retained model");
        renderer->setRenderAttachment(target, nullptr);
        require(renderer->drawPostEffect(effect, IRenderer::BlendState::One, texture, IRenderer::SamplerState::PointClamp, nullptr, 0, nullptr, nullptr, 0),
            "Draw retained post-effect");
        renderer->setRenderAttachment(nullptr, nullptr);
        require(renderer->endBatch(), "End capture batch");
        require(runtime.swapChain()->saveSnapshotToFile(path), "Queue capture");
        require(runtime.submitFrame(true), "Submit capture frame");
    }

    int run(std::string_view scenario)
    {
        SdlRuntime sdl;
        require(sdl.initialize(), "Initialize SDL");
        SmartReference<IWindow> window;
        require(IWindow::create({ 128, 128 }, "SDL GPU recovery capture", WindowFrameStyle::Normal, true, window.put()), "Create window");
        auto runtime = IGraphicsRuntime::create(window.get());
        require(runtime->swapChain()->setCanvasSize({ 128, 128 }), "Set canvas size");
        int width{}, height{};
        require(SDL_GetWindowSizeInPixels(window->getSDLWindow(), &width, &height) && width > 0 && height > 0, "Get drawable size");
        require(runtime->swapChain()->setWindowMode({ static_cast<uint32_t>(width), static_cast<uint32_t>(height) }), "Set drawable size");
        auto* device = static_cast<Device*>(runtime->device());
        FileSystemManager::addSearchPath(LUASTG_GEOMETRY_FIXTURES);

        SmartReference<ITexture2D> texture;
        require(device->createTexture({ 2, 2 }, texture.put()), "Create dynamic texture");
        const std::array<uint8_t, 16> bgra{
            0,
            0,
            255,
            255,
            0,
            255,
            0,
            255,
            255,
            0,
            0,
            255,
            255,
            255,
            255,
            255,
        };
        require(texture->uploadPixelData({ { 0, 0 }, { 2, 2 } }, bgra.data(), 8), "Upload texture pixels");

        SmartReference<IRenderTarget> target;
        require(device->createRenderTarget({ 16, 16 }, target.put()), "Create retained render target");
        SmartReference<IModel> model;
        require(runtime->renderer()->createModel("solid-index8.gltf", model.put()), "Load model fixture");
        SmartReference<IPostEffectShader> effect;
        require(runtime->renderer()->createPostEffectShader("passthrough.hlsl", effect.put()), "Load shader fixture");
        const auto texture_id = texture->getDebugId();
        auto* const model_identity = model.get();
        auto* const effect_identity = effect.get();
        capture(*runtime, texture.get(), model.get(), target.get(), effect.get(), "recovery-before.png");
        require(target->getTexture()->saveToFile("recovery-target-before.png"), "Capture effect before recovery");

        if(scenario == "acquisition" || scenario == "second" || scenario == "reconstruction")
            device->inject(Device::FailurePoint::Acquisition);
        else if(scenario == "submission")
            device->inject(Device::FailurePoint::Submission);
        else if(scenario == "upload")
            device->inject(Device::FailurePoint::Upload);
        else
            throw std::invalid_argument("Use acquisition, submission, upload, second, or reconstruction");

        if(scenario == "reconstruction")
            device->fail_reconstruction = true;
        if(scenario == "upload") {
            require(!texture->uploadPixelData({ { 0, 0 }, { 2, 2 } }, bgra.data(), 8), "Injected upload did not fail");
        } else if(scenario == "submission") {
            require(runtime->beginFrame() == FrameStatus::Ready, "Begin injected submission frame");
            require(!runtime->submitFrame(true), "Injected submission did not fail");
        } else {
            require(runtime->beginFrame() == FrameStatus::Failed, "Injected acquisition did not fail");
        }
        const auto result = runtime->serviceRecovery();
        if(scenario == "reconstruction") {
            require(result == RecoveryResult::Fatal, "Reconstruction failure did not exit");
            return 2;
        }
        require(result == RecoveryResult::Recovered, "First recovery did not complete");
        require(texture->getDebugId() == texture_id && model.get() == model_identity && effect.get() == effect_identity,
            "Resource identity changed across recovery");
        require(static_cast<Texture2D*>(texture.get())->handle() != nullptr, "Texture was not rebuilt");
        require(!static_cast<Model*>(model.get())->buffers.empty(), "Model buffers were not rebuilt");
        require(target->getTexture()->saveToFile("recovery-target-cleared.png"), "Capture cleared render target");
        capture(*runtime, texture.get(), model.get(), target.get(), effect.get(), "recovery-after.png");
        require(target->getTexture()->saveToFile("recovery-target-after.png"), "Capture effect after recovery");
        if(scenario == "second") {
            device->inject(Device::FailurePoint::Acquisition);
            require(runtime->beginFrame() == FrameStatus::Failed, "Second injected acquisition did not fail");
            require(runtime->serviceRecovery() == RecoveryResult::Fatal, "Second failure did not exit");
            return 2;
        }
        return 0;
    }
}

int
main(int argc, char** argv)
{
    try {
        spdlog::set_default_logger(spdlog::stdout_color_mt("recovery-capture"));
        if(argc != 2)
            throw std::invalid_argument("Usage: Core.Graphics.SDLGPU.RecoveryCapture.exe acquisition|submission|upload|second|reconstruction");
        return run(argv[1]);
    } catch(const std::exception& error) {
        std::fprintf(stderr, "Recovery capture failed: %s\n", error.what());
        return 1;
    }
}
