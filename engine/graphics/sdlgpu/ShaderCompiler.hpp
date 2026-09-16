#pragma once

#include "Resources.hpp"

namespace core::Graphics::SDLGPU
{
    class ShaderCompiler final
    {
    public:
        ShaderCompiler();
        ~ShaderCompiler();
        ShaderCompiler(const ShaderCompiler&) = delete;
        ShaderCompiler& operator=(const ShaderCompiler&) = delete;

        Shader compile(SDL_GPUDevice* device, const char* source, SDL_GPUShaderStage stage, const char* name) const;

    private:
        using Library = std::unique_ptr<SDL_SharedObject, decltype(&SDL_UnloadObject)>;
        Library m_dxil{ nullptr, SDL_UnloadObject };
        Library m_dxc{ nullptr, SDL_UnloadObject };
    };
}
