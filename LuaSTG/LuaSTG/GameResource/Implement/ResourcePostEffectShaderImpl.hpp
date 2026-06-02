#pragma once
#include "core/SmartReference.hpp"
#include "GameResource/ResourcePostEffectShader.hpp"
#include "GameResource/Implement/ResourceBaseImpl.hpp"
#include <string_view>

namespace luastg
{
    class ResourcePostEffectShaderImpl : public ResourceBaseImpl<IResourcePostEffectShader>
    {
    private:
        core::SmartReference<core::Graphics::IPostEffectShader> m_shader;
    public:
		core::Graphics::IPostEffectShader* GetPostEffectShader() noexcept { return *m_shader; }
	public:
		ResourcePostEffectShaderImpl(const char* name, const char* path);
		ResourcePostEffectShaderImpl(const char* name, std::string_view source, bool from_source);
	};
}
