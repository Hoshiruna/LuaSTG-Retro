#include "ShaderCompiler.hpp"
#include "core/Logger.hpp"
#include <SDL3_shadercross/SDL_shadercross.h>

namespace core::Graphics::SDLGPU
{
    ShaderCompiler::ShaderCompiler()
    {
        const std::string directory = require(SDL_GetBasePath(), "SDL_GetBasePath");
        m_dxil.reset(require(SDL_LoadObject((directory + "dxil.dll").c_str()), "Load packaged dxil.dll"));
        m_dxc.reset(require(SDL_LoadObject((directory + "dxcompiler.dll").c_str()), "Load packaged dxcompiler.dll"));
        require(SDL_LoadFunction(m_dxc.get(), "DxcCreateInstance"), "Find DxcCreateInstance in dxcompiler.dll");
        check(SDL_ShaderCross_Init(), "SDL_ShaderCross_Init");
    }

    ShaderCompiler::~ShaderCompiler()
    {
        SDL_ShaderCross_Quit();
    }

    Shader ShaderCompiler::compile(SDL_GPUDevice* const device, const char* const source, const SDL_GPUShaderStage stage, const char* const name) const
    {
        Logger::info("[sdlgpu] Compiling shader: {}", name);
        SDL_ShaderCross_HLSL_Info input{};
        input.source = source;
        input.entrypoint = "main";
        input.shader_stage = stage == SDL_GPU_SHADERSTAGE_VERTEX ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
        size_t size{};
        const std::unique_ptr<void, decltype(&SDL_free)> spirv(
            require(SDL_ShaderCross_CompileSPIRVFromHLSL(&input, &size), std::string("Compile HLSL ") + name), SDL_free);
        const std::unique_ptr<SDL_ShaderCross_GraphicsShaderMetadata, decltype(&SDL_free)> metadata(
            require(SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(spirv.get()), size, 0), std::string("Reflect shader ") + name), SDL_free);
        SDL_ShaderCross_SPIRV_Info translated{};
        translated.bytecode = static_cast<const Uint8*>(spirv.get());
        translated.bytecode_size = size;
        translated.entrypoint = "main";
        translated.shader_stage = input.shader_stage;
        if(std::string_view(SDL_GetGPUDeviceDriver(device)) == "direct3d12") {
            // Exercise the packaged DXC path even when system FXC is available.
            size_t dxil_size{};
            const std::unique_ptr<void, decltype(&SDL_free)> dxil(
                require(SDL_ShaderCross_CompileDXILFromSPIRV(&translated, &dxil_size), std::string("Compile DXIL ") + name), SDL_free);
            SDL_GPUShaderCreateInfo info{};
            info.code = static_cast<const Uint8*>(dxil.get());
            info.code_size = dxil_size;
            info.entrypoint = "main";
            info.format = SDL_GPU_SHADERFORMAT_DXIL;
            info.stage = stage;
            info.num_samplers = metadata->resource_info.num_samplers;
            info.num_storage_textures = metadata->resource_info.num_storage_textures;
            info.num_storage_buffers = metadata->resource_info.num_storage_buffers;
            info.num_uniform_buffers = metadata->resource_info.num_uniform_buffers;
            return { require(SDL_CreateGPUShader(device, &info), std::string("Create DXIL shader ") + name), { device } };
        }
        return {
            require(SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &translated, &metadata->resource_info, 0), std::string("Create GPU shader ") + name),
            { device },
        };
    }
}
