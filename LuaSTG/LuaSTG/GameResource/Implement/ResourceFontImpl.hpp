#pragma once
#include "core/SmartReference.hpp"
#include "GameResource/ResourceFont.hpp"
#include "GameResource/Implement/ResourceBaseImpl.hpp"

#include <string_view>

namespace core
{
	struct IData;
}

namespace luastg
{
	class ResourceFontImpl : public ResourceBaseImpl<IResourceFont>
	{
	private:
		core::SmartReference<core::Graphics::IGlyphManager> m_glyphmgr;
		core::Color4B m_BlendColor;
		BlendMode m_BlendMode;

	public:
		core::Graphics::IGlyphManager* GetGlyphManager() { return m_glyphmgr.get(); }
		BlendMode GetBlendMode() { return m_BlendMode; }
		void SetBlendMode(BlendMode m) { m_BlendMode = m; }
		core::Color4B GetBlendColor() { return m_BlendColor; }
		void SetBlendColor(core::Color4B c) { m_BlendColor = c; }

	public:
		ResourceFontImpl(const char* name, std::string_view hge_path, bool mipmap);
		ResourceFontImpl(const char* name, std::string_view hge_path, core::IData* hge_data, core::IData* texture_data, std::string_view texture_path, bool mipmap);
		ResourceFontImpl(const char* name, std::string_view f2d_path, std::string_view tex_path, bool mipmap);
		ResourceFontImpl(const char* name, std::string_view f2d_path, core::IData* f2d_data, std::string_view tex_path, core::IData* texture_data, bool mipmap);
		ResourceFontImpl(const char* name, core::Graphics::IGlyphManager* p_mgr);
	};
}
