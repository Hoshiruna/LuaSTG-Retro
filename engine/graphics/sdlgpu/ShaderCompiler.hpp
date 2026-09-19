#pragma once

#include "Resources.hpp"
#include <span>
#include <vector>

namespace core::Graphics::SDLGPU
{
    class ShaderCompiler final
    {
    public:
        // driver decides which HLSL back ends have to be present; see the constructor.
        explicit ShaderCompiler(std::string_view driver);
        ~ShaderCompiler();
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;

        Shader compile(SDL_GPUDevice* device, const char* source, SDL_GPUShaderStage stage, const char* name) const;
        std::vector<uint32_t> compileLegacy(std::string_view source, std::string_view name) const;
        Shader create(SDL_GPUDevice* device, std::span<const uint32_t> spirv, SDL_GPUShaderStage stage, const char* name) const;

    private:
        using Library = std::unique_ptr<SDL_SharedObject, decltype(&SDL_UnloadObject)>;
        Library m_dxil{ nullptr, SDL_UnloadObject };
        Library m_dxc{ nullptr, SDL_UnloadObject };
    };
}
