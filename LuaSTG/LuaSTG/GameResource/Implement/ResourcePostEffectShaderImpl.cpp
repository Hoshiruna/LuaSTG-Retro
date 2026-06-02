#include "GameResource/Implement/ResourcePostEffectShaderImpl.hpp"
#include "AppFrame.h"

namespace luastg
{
	ResourcePostEffectShaderImpl::ResourcePostEffectShaderImpl(const char* name, const char* path)
		: ResourceBaseImpl(ResourceType::FX, name)
	{
		LAPP.GetAppModel()->getRenderer()->createPostEffectShader(path, m_shader.put());
	}

	ResourcePostEffectShaderImpl::ResourcePostEffectShaderImpl(const char* name, std::string_view source, bool from_source)
		: ResourceBaseImpl(ResourceType::FX, name)
	{
		if (from_source)
		{
			LAPP.GetAppModel()->getRenderer()->createPostEffectShaderFromSource(source, m_shader.put());
		}
		else
		{
			LAPP.GetAppModel()->getRenderer()->createPostEffectShader(source, m_shader.put());
		}
	}
}
