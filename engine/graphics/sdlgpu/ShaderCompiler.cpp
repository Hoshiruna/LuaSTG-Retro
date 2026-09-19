#include "ShaderCompiler.hpp"
#include "core/Logger.hpp"
#include <SDL3_shadercross/SDL_shadercross.h>
#if defined(_WIN32)
#include <unknwn.h>
#endif
#include <dxcapi.h>
#include <cstring>
#include <iterator>

namespace core::Graphics::SDLGPU
{
    ShaderCompiler::ShaderCompiler(const std::string_view driver)
    {
        const std::string directory = require(SDL_GetBasePath(), "SDL_GetBasePath");
        // DXC is the HLSL front end on every driver, because shadercross is built with
        // SDL_SHADERCROSS_DXC: even the Vulkan path goes through HLSL -> SPIR-V in DXC.
        m_dxc.reset(require(SDL_LoadObject((directory + "dxcompiler.dll").c_str()), "Load packaged dxcompiler.dll"));
        require(SDL_LoadFunction(m_dxc.get(), "DxcCreateInstance"), "Find DxcCreateInstance in dxcompiler.dll");
        // Only DXIL signing is Direct3D-specific, so a machine that runs Vulkan must not be
        // held back by a missing dxil.dll.
        if(driver == "direct3d12") {
            m_dxil.reset(SDL_LoadObject((directory + "dxil.dll").c_str()));
            if(m_dxil == nullptr) {
                throw std::runtime_error(std::string("Load packaged dxil.dll: ") + SDL_GetError() + ". Restore the packaged dxil.dll beside the executable.");
            }
        }
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
        return create(device, { static_cast<const uint32_t*>(spirv.get()), size / sizeof(uint32_t) }, stage, name);
    }

    Shader ShaderCompiler::create(SDL_GPUDevice* const device, std::span<const uint32_t> spirv, SDL_GPUShaderStage stage, const char* name) const
    {
        const auto* bytes = reinterpret_cast<const Uint8*>(spirv.data());
        const auto size = spirv.size_bytes();
        const std::unique_ptr<SDL_ShaderCross_GraphicsShaderMetadata, decltype(&SDL_free)> metadata(
            require(SDL_ShaderCross_ReflectGraphicsSPIRV(bytes, size, 0), std::string("Reflect shader ") + name), SDL_free);
        SDL_ShaderCross_SPIRV_Info translated{};
        translated.bytecode = bytes;
        translated.bytecode_size = size;
        translated.entrypoint = "main";
        translated.shader_stage = stage == SDL_GPU_SHADERSTAGE_VERTEX ? SDL_SHADERCROSS_SHADERSTAGE_VERTEX : SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
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

    namespace
    {
        template<typename T>
        struct ReleaseCom
        {
            void operator()(T* value) const noexcept { value->Release(); }
        };
        template<typename T>
        using ComOwner = std::unique_ptr<T, ReleaseCom<T>>;
    }

    std::vector<uint32_t> ShaderCompiler::compileLegacy(std::string_view source, std::string_view name) const
    {
        IDxcCompiler3* raw_compiler{};
        if(FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&raw_compiler))))
            throw std::runtime_error("Create DXC compiler for " + std::string(name));
        ComOwner<IDxcCompiler3> compiler(raw_compiler);
        // Reflection preserves HLSL semantics; DX layout preserves legacy cbuffer offsets.
        const wchar_t* arguments[]{ L"-E", L"main", L"-T", L"ps_6_0", L"-HV", L"2016", L"-spirv", L"-fvk-use-dx-layout", L"-fspv-reflect", L"-fspv-preserve-bindings", L"-fspv-preserve-interface" };
        DxcBuffer input{ source.data(), source.size(), DXC_CP_UTF8 };
        IDxcResult* raw_result{};
        if(FAILED(compiler->Compile(&input, arguments, static_cast<UINT32>(std::size(arguments)), nullptr, IID_PPV_ARGS(&raw_result))))
            throw std::runtime_error("DXC compile invocation failed for " + std::string(name));
        ComOwner<IDxcResult> result(raw_result);
        IDxcBlobUtf8* raw_errors{};
        result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&raw_errors), nullptr);
        ComOwner<IDxcBlobUtf8> errors(raw_errors);
        const std::string diagnostic = errors && errors->GetStringLength() ? std::string(errors->GetStringPointer(), errors->GetStringLength()) : std::string();
        HRESULT status{};
        if(FAILED(result->GetStatus(&status)) || FAILED(status))
            throw std::runtime_error("Compile post-effect " + std::string(name) + ": " + diagnostic);
        if(!diagnostic.empty())
            Logger::warn("[sdlgpu] {}: {}", name, diagnostic);
        IDxcBlob* raw_code{};
        if(FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&raw_code), nullptr)))
            throw std::runtime_error("Read compiled post-effect " + std::string(name));
        ComOwner<IDxcBlob> code(raw_code);
        std::vector<uint32_t> words(code->GetBufferSize() / sizeof(uint32_t));
        std::memcpy(words.data(), code->GetBufferPointer(), code->GetBufferSize());
        return words;
    }
}
