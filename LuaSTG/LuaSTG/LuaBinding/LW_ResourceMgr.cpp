#include "LuaBinding/LuaWrapper.hpp"
#include "LuaBinding/AsyncResourceJob.hpp"
#include "LuaBinding/Resource.hpp"
#include "GameResource/AsyncResourceLoader.hpp"
#include "lua/plus.hpp"
#include "AppFrame.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

void luastg::binding::ResourceManager::Register(lua_State* L) noexcept
{
	struct Wrapper
	{
		static ResourcePool* BeginPoolCall(lua_State* L)
		{
			auto* pool = luastg::binding::checkResourcePool(L, 1);
			lua_remove(L, 1);
			return pool;
		}
		static int SetResLoadInfo(lua_State* L) noexcept {
			ResourceMgr::SetResourceLoadingLog((bool)lua_toboolean(L, 1));
			return 0;
		}
		static AsyncResourceRequest CreateAsyncRequest(lua_State* L, AsyncResourceRequestType type)
		{
			auto* pool = BeginPoolCall(L);
			AsyncResourceRequest request;
			request.type = type;
			request.pool_id = pool->GetId();
			request.name = luaL_checkstring(L, 1);
			return request;
		}
		static int PushAsyncJob(lua_State* L, AsyncResourceRequest request)
		{
			AsyncResourceJobBinding::createAndPush(L, LRES.SubmitAsyncResource(std::move(request)));
			return 1;
		}
		static int LoadTexture(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			if (!pActivedPool->LoadTexture(name, path, lua_toboolean(L, 3) == 0 ? false : true))
				return luaL_error(L, "can't load texture from file '%s'.", path);
			return 0;
		}
		static int LoadTextureAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Texture);
			request.path = luaL_checkstring(L, 2);
			request.mipmap = lua_toboolean(L, 3) != 0;
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadVideo(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			bool const loop = lua_gettop(L) >= 3 ? lua_toboolean(L, 3) != 0 : false;
			if (!pActivedPool->LoadVideo(name, path, loop))
				return luaL_error(L, "can't load video from file '%s'.", path);
			auto resource = pActivedPool->GetTexture(name);
			luastg::binding::pushResourceTexture(L, resource.get());
			return 1;
		}
		static int LoadVideoAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Video);
			request.path = luaL_checkstring(L, 2);
			request.loop = lua_gettop(L) >= 3 ? lua_toboolean(L, 3) != 0 : false;
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadSprite(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			auto* texture = luastg::binding::checkResourceTexture(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (!pActivedPool->CreateSprite(
				name,
				texture,
				luaL_checknumber(L, 3),
				luaL_checknumber(L, 4),
				luaL_checknumber(L, 5),
				luaL_checknumber(L, 6),
				luaL_optnumber(L, 7, 0.),
				luaL_optnumber(L, 8, 0.),
				lua_toboolean(L, 9) == 0 ? false : true
			))
			{
				return luaL_error(L, "load image failed (name='%s').", name);
			}
			return 0;
		}
		static int LoadSpriteAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Sprite);
			request.texture = luastg::binding::checkResourceTexture(L, 2);
			request.x = luaL_checknumber(L, 3);
			request.y = luaL_checknumber(L, 4);
			request.w = luaL_checknumber(L, 5);
			request.h = luaL_checknumber(L, 6);
			request.a = luaL_optnumber(L, 7, 0.);
			request.b = luaL_optnumber(L, 8, 0.);
			request.rect = lua_toboolean(L, 9) != 0;
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadAnimation(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (lua_istable(L, 2)) {
				std::vector<core::SmartReference<IResourceSprite>> sprites;
				sprites.reserve(lua_objlen(L, 2));
				for (int i = 1; i <= static_cast<int>(lua_objlen(L, 2)); i += 1) {
					lua_pushinteger(L, i);
					lua_gettable(L, 2);
					sprites.emplace_back(luastg::binding::checkResourceSprite(L, -1));
					lua_pop(L, 1);
				}
				if (!pActivedPool->CreateAnimation(
					name,
					sprites,
					luaL_checkinteger(L, 3),
					luaL_optnumber(L, 4, 0.0f),
					luaL_optnumber(L, 5, 0.0f),
					lua_toboolean(L, 6) != 0
				)) {
					return luaL_error(L, "load animation failed (name='%s').", name);
				}
			}
			else {
				auto* texture = luastg::binding::checkResourceTexture(L, 2);
				if (!pActivedPool->CreateAnimation(
					name,
					texture,
					luaL_checknumber(L, 3),
					luaL_checknumber(L, 4),
					luaL_checknumber(L, 5),
					luaL_checknumber(L, 6),
					(int)luaL_checkinteger(L, 7),
					(int)luaL_checkinteger(L, 8),
					(int)luaL_checkinteger(L, 9),
					luaL_optnumber(L, 10, 0.0f),
					luaL_optnumber(L, 11, 0.0f),
					lua_toboolean(L, 12) != 0
				)) {
					return luaL_error(L, "load animation failed (name='%s').", name);
				}
			}
			
			return 0;
		}
		static int LoadAnimationAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Animation);
			if (lua_istable(L, 2)) {
				request.animation_uses_sprite_list = true;
				int const count = static_cast<int>(lua_objlen(L, 2));
				request.sprites.reserve(count);
				for (int i = 1; i <= count; i += 1) {
					lua_pushinteger(L, i);
					lua_gettable(L, 2);
					request.sprites.emplace_back(luastg::binding::checkResourceSprite(L, -1));
					lua_pop(L, 1);
				}
				request.interval = static_cast<int>(luaL_checkinteger(L, 3));
				request.a = luaL_optnumber(L, 4, 0.0f);
				request.b = luaL_optnumber(L, 5, 0.0f);
				request.rect = lua_toboolean(L, 6) != 0;
			}
			else {
				request.texture = luastg::binding::checkResourceTexture(L, 2);
				request.x = luaL_checknumber(L, 3);
				request.y = luaL_checknumber(L, 4);
				request.w = luaL_checknumber(L, 5);
				request.h = luaL_checknumber(L, 6);
				request.columns = static_cast<int>(luaL_checkinteger(L, 7));
				request.rows = static_cast<int>(luaL_checkinteger(L, 8));
				request.interval = static_cast<int>(luaL_checkinteger(L, 9));
				request.a = luaL_optnumber(L, 10, 0.0f);
				request.b = luaL_optnumber(L, 11, 0.0f);
				request.rect = lua_toboolean(L, 12) != 0;
			}
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadPS(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			
			const char* name = luaL_checkstring(L, 1);
			auto* sprite = luastg::binding::checkResourceSprite(L, 3);
			if (lua_type(L, 2) == LUA_TTABLE) {
				hgeParticleSystemInfo info;
				bool ret = TranslateTableToParticleInfo(L, 2, info);
				if (!ret) return luaL_error(L, "load particle failed (name='%s', define=?).", name);
				if (!pActivedPool->LoadParticle(
					name,
					info,
					sprite,
					luaL_optnumber(L, 4, 0.0f),
					luaL_optnumber(L, 5, 0.0f),
					lua_toboolean(L, 6) == 0 ? false : true
				))
				{
					return luaL_error(L, "load particle failed (name='%s', define=table).", name);
				}
				lua_pushboolean(L, true);
				return 1;
			}
			else {
				const char* path = luaL_checkstring(L, 2);

				if (!pActivedPool->LoadParticle(
					name,
					path,
					sprite,
					luaL_optnumber(L, 4, 0.0f),
					luaL_optnumber(L, 5, 0.0f),
					lua_toboolean(L, 6) == 0 ? false : true
				))
				{
					return luaL_error(L, "load particle failed (name='%s', file='%s').", name, path);
				}
				lua_pushboolean(L, true);
				return 1;
			}
		}
		static int LoadPSAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Particle);
			request.sprite = luastg::binding::checkResourceSprite(L, 3);
			request.a = luaL_optnumber(L, 4, 0.0f);
			request.b = luaL_optnumber(L, 5, 0.0f);
			request.rect = lua_toboolean(L, 6) != 0;
			if (lua_type(L, 2) == LUA_TTABLE) {
				request.has_particle_info = true;
				if (!TranslateTableToParticleInfo(L, 2, request.particle_info)) {
					return luaL_error(L, "load particle failed (name='%s', define=?).", request.name.c_str());
				}
			}
			else {
				request.path = luaL_checkstring(L, 2);
			}
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadSound(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (!pActivedPool->LoadSoundEffect(name, path))
				return luaL_error(L, "load sound failed (name=%s, path=%s)", name, path);
			lua_pushboolean(L, true);
			return 1;
		}
		static int LoadSoundAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Sound);
			request.path = luaL_checkstring(L, 2);
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadMusic(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			double loop_end = luaL_checknumber(L, 3);
			double loop_duration = luaL_checknumber(L, 4);
			double loop_start = std::max(0., loop_end - loop_duration);

			if (!pActivedPool->LoadMusic(
				name,
				path,
				loop_start,
				loop_end,
				(lua_gettop(L) >= 5) ? lua_toboolean(L, 5) : false
				))
			{
				return luaL_error(L, "load music failed (name=%s, path=%s, loop=%f~%f)", name, path, loop_start, loop_end);
			}
			lua_pushboolean(L, true);
			return 1;
		}
		static int LoadMusicAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Music);
			request.path = luaL_checkstring(L, 2);
			double const loop_end = luaL_checknumber(L, 3);
			double const loop_duration = luaL_checknumber(L, 4);
			request.loop_end = loop_end;
			request.loop_start = std::max(0., loop_end - loop_duration);
			request.once_decode = (lua_gettop(L) >= 5) ? lua_toboolean(L, 5) != 0 : false;
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadFont(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			bool bSucceed = false;
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (lua_gettop(L) == 2)
			{
				// HGE字体 mipmap=true
				bSucceed = pActivedPool->LoadSpriteFont(name, path);
			}
			else
			{
				if (lua_isboolean(L, 3))
				{
					// HGE字体 mipmap=user_defined
					bSucceed = pActivedPool->LoadSpriteFont(name, path, lua_toboolean(L, 3) == 0 ? false : true);
				}
				else
				{
					// fancy2d字体
					const char* texpath = luaL_checkstring(L, 3);
					if (lua_gettop(L) == 4)
						bSucceed = pActivedPool->LoadSpriteFont(name, path, texpath, lua_toboolean(L, 4) == 0 ? false : true);
					else
						bSucceed = pActivedPool->LoadSpriteFont(name, path, texpath);
				}
			}

			if (!bSucceed)
				return luaL_error(L, "can't load font from file '%s'.", path);
			lua_pushboolean(L, true);
			return 1;
		}
		static int LoadFontAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::SpriteFont);
			request.path = luaL_checkstring(L, 2);
			request.mipmap = true;
			if (lua_gettop(L) >= 3) {
				if (lua_isboolean(L, 3)) {
					request.mipmap = lua_toboolean(L, 3) != 0;
				}
				else {
					request.has_texture_path = true;
					request.texture_path = luaL_checkstring(L, 3);
					if (lua_gettop(L) >= 4) {
						request.mipmap = lua_toboolean(L, 4) != 0;
					}
				}
			}
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadTTF(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			if (!pActivedPool) {
				return luaL_error(L, "can't load resource at this time.");
			}
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);
			bool result = pActivedPool->LoadTTFFont(name, path, (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4));
			lua_pushboolean(L, result);
			return 1;
		}
		static int LoadTTFAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::TrueTypeFont);
			request.path = luaL_checkstring(L, 2);
			request.font_width = (float)luaL_checknumber(L, 3);
			request.font_height = (float)luaL_checknumber(L, 4);
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadTrueTypeFont(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			lua::stack_t S(L);

			// 先检查有没有资源池
			if (!pActivedPool)
			{
				return luaL_error(L, "can't load resource at this time.");
			}
			
			// 第一个参数，资源名
			std::string_view const name = S.get_value<std::string_view>(1);
			
			// 第二个参数，字体组
			if (!lua_istable(L, 2))
			{
				return luaL_error(L, "invalid parameter #2, required table");
			}
			int const cnt = (int)lua_objlen(L, 2);
			std::vector<core::Graphics::TrueTypeFontInfo> fonts(cnt);
			for (int i = 1; i <= cnt; i += 1)
			{
				auto& font = fonts[i - 1];
				font.source = "";
				font.font_face = 0;
				font.font_size = core::Vector2F(0.0f, 0.0f);
				font.is_force_to_file = false;
				font.is_buffer = false;

				lua_pushinteger(L, i);		// name param fonts i
				lua_gettable(L, 2);			// name param fonts font
				if (!lua_istable(L, -1))
				{
					return luaL_error(L, "invalid value #%d in parameter #2, required table", i);
				}

				lua_getfield(L, -1, "source"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TSTRING) // name param fonts font v
				{
					font.source = S.get_value<std::string_view>(-1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_getfield(L, -1, "font_face"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TNUMBER) // name param fonts font v
				{
					font.font_face = (uint32_t)luaL_checkinteger(L, -1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_getfield(L, -1, "width"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TNUMBER) // name param fonts font v
				{
					font.font_size.x = (float)luaL_checknumber(L, -1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_getfield(L, -1, "height"); // name param fonts font ?
				if (lua_type(L, -1) == LUA_TNUMBER) // name param fonts font v
				{
					font.font_size.y = (float)luaL_checknumber(L, -1);
				}
				lua_pop(L, 1);				// name param fonts font

				lua_pop(L, 1);				// name param fonts
			}

			bool result = pActivedPool->LoadTrueTypeFont(name.data(), fonts.data(), fonts.size());
			lua_pushboolean(L, result);
			
			return 1;
		}
		static int LoadTrueTypeFontAsync(lua_State* L) noexcept
		{
			lua::stack_t S(L);
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::TrueTypeFont);
			if (!lua_istable(L, 2)) {
				return luaL_error(L, "invalid parameter #2, required table");
			}
			int const cnt = (int)lua_objlen(L, 2);
			request.fonts.resize(cnt);
			request.font_sources.resize(cnt);
			for (int i = 1; i <= cnt; i += 1) {
				auto& font = request.fonts[i - 1];
				font.source = "";
				font.font_face = 0;
				font.font_size = core::Vector2F(0.0f, 0.0f);
				font.is_force_to_file = false;
				font.is_buffer = false;

				lua_pushinteger(L, i);
				lua_gettable(L, 2);
				if (!lua_istable(L, -1)) {
					return luaL_error(L, "invalid value #%d in parameter #2, required table", i);
				}

				lua_getfield(L, -1, "source");
				if (lua_type(L, -1) == LUA_TSTRING) {
					request.font_sources[i - 1] = std::string(S.get_value<std::string_view>(-1));
					font.source = request.font_sources[i - 1];
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "font_face");
				if (lua_type(L, -1) == LUA_TNUMBER) {
					font.font_face = (uint32_t)luaL_checkinteger(L, -1);
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "width");
				if (lua_type(L, -1) == LUA_TNUMBER) {
					font.font_size.x = (float)luaL_checknumber(L, -1);
				}
				lua_pop(L, 1);

				lua_getfield(L, -1, "height");
				if (lua_type(L, -1) == LUA_TNUMBER) {
					font.font_size.y = (float)luaL_checknumber(L, -1);
				}
				lua_pop(L, 1);

				lua_pop(L, 1);
			}
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadFX(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			const char* path = luaL_checkstring(L, 2);

			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");

			if (!pActivedPool->LoadFX(name, path))
				return luaL_error(L, "load fx failed (name=%s, path=%s)", name, path);

			lua_pushboolean(L, true);
			return 1;
		}
		static int LoadFXAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::FX);
			request.path = luaL_checkstring(L, 2);
			return PushAsyncJob(L, std::move(request));
		}
		static int LoadModel(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			const char* model_path = luaL_checkstring(L, 2);
			
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			if (!pActivedPool->LoadModel(
				name,
				model_path))
			{
				return luaL_error(L, "load model failed (name='%s', model='%s').", name, model_path);
			}
			lua_pushboolean(L, true);
			return 1;
		}
		static int LoadModelAsync(lua_State* L) noexcept
		{
			auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Model);
			request.path = luaL_checkstring(L, 2);
			return PushAsyncJob(L, std::move(request));
		}
		static int CreateRenderTarget(lua_State* L) noexcept
		{
			ResourcePool* pActivedPool = BeginPoolCall(L);
			const char* name = luaL_checkstring(L, 1);
			
			if (!pActivedPool)
				return luaL_error(L, "can't load resource at this time.");
			
			if (lua_gettop(L) >= 3)
			{
				const int width = (int)luaL_checkinteger(L, 2);
				const int height = (int)luaL_checkinteger(L, 3);
				if (width < 1 || height < 1)
					return luaL_error(L, "invalid render target size (%dx%d).", width, height);
				bool depth_buffer = true;
				if (lua_gettop(L) >= 4)
					depth_buffer = lua_toboolean(L, 4);
				if (!pActivedPool->CreateRenderTarget(name, width, height, depth_buffer))
					return luaL_error(L, "can't create render target with name '%s'.", name);
			}
			else
			{
				if (!pActivedPool->CreateRenderTarget(name, 0, 0, true))
					return luaL_error(L, "can't create render target with name '%s'.", name);
			}
			auto resource = pActivedPool->GetTexture(name);
			luastg::binding::pushResourceTexture(L, resource.get());
			return 1;
		}
		static int IsRenderTarget(lua_State* L) noexcept
		{
			core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "render target '%s' not found.", luaL_checkstring(L, 1));
			lua_pushboolean(L, p->IsRenderTarget());
			return 1;
		}
		static int SetTexturePreMulAlphaState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
			if (p)
			{
				p->GetTexture()->setPremultipliedAlpha(lua_toboolean(L, 2));
				return 0;
			}
			return luaL_error(L, "texture '%s' not found.", luaL_checkstring(L, 1));
		}
		static int SetTextureSamplerState(lua_State* L)noexcept
		{
			lua::stack_t S(L);
			std::string_view const sampler_name = S.get_value<std::string_view>(2);
			if (sampler_name == "" || sampler_name == "point+wrap" || sampler_name == "point+clamp" || sampler_name == "linear+wrap" || sampler_name == "linear+clamp")
			{
				std::string_view const tex_name = S.get_value<std::string_view>(1);
				core::SmartReference<IResourceTexture> p = LRES.FindTexture(tex_name.data());
				if (!p)
				{
					spdlog::error("[luastg] lstg.SetTextureSamplerState failed: can't find texture '{}'", tex_name);
					return luaL_error(L, "can't find texture '%s'", tex_name.data());
				}

				// 映射
				core::Graphics::IRenderer::SamplerState state = core::Graphics::IRenderer::SamplerState::LinearClamp;
				if (sampler_name == "point+wrap") state = core::Graphics::IRenderer::SamplerState::PointWrap;
				else if (sampler_name == "point+clamp") state = core::Graphics::IRenderer::SamplerState::PointClamp;
				else if (sampler_name == "linear+wrap") state = core::Graphics::IRenderer::SamplerState::LinearWrap;
				else if (sampler_name == "" || sampler_name == "linear+clamp") state = core::Graphics::IRenderer::SamplerState::LinearClamp;
				else return luaL_error(L, "unknown sampler state '%s'", sampler_name.data());

				// 设置
				core::Graphics::ISamplerState* p_sampler = LAPP.GetRenderer2D()->getKnownSamplerState(state);
				p->GetTexture()->setSamplerState(p_sampler);

				return 0;
			}
			else
			{
				return luaL_error(L, "unsupported deprecated usage");
			}
		}
		static int GetTextureSize(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			core::Vector2U size;
			if (!LRES.GetTextureSize(name, size))
				return luaL_error(L, "texture '%s' not found.", name);
			lua_pushinteger(L, (lua_Integer)size.x);
			lua_pushinteger(L, (lua_Integer)size.y);
			return 2;
		}
		static int PlayVideo(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			bool const restart = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) != 0 : true;
			if (!video->Play(restart))
				return luaL_error(L, "can't play video '%s'.", name);
			return 0;
		}
		static int PauseVideo(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			if (!video->Pause())
				return luaL_error(L, "can't pause video '%s'.", name);
			return 0;
		}
		static int ResumeVideo(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			if (!video->Resume())
				return luaL_error(L, "can't resume video '%s'.", name);
			return 0;
		}
		static int StopVideo(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			if (!video->Stop())
				return luaL_error(L, "can't stop video '%s'.", name);
			return 0;
		}
		static int SeekVideo(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			if (!video->Seek(luaL_checknumber(L, 2)))
				return luaL_error(L, "can't seek video '%s'.", name);
			return 0;
		}
		static int GetVideoState(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			switch (video->GetVideoState())
			{
			case VideoPlaybackState::Stopped:
				lua_pushstring(L, "stopped");
				break;
			case VideoPlaybackState::Playing:
				lua_pushstring(L, "playing");
				break;
			case VideoPlaybackState::Paused:
				lua_pushstring(L, "paused");
				break;
			case VideoPlaybackState::Ended:
				lua_pushstring(L, "ended");
				break;
			default:
				lua_pushstring(L, "stopped");
				break;
			}
			return 1;
		}
		static int GetVideoTime(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			lua_pushnumber(L, video->GetVideoTime());
			return 1;
		}
		static int GetVideoDuration(lua_State* L) noexcept
		{
			const char* name = luaL_checkstring(L, 1);
			auto video = LRES.FindVideo(name);
			if (!video)
				return luaL_error(L, "video '%s' not found.", name);
			lua_pushnumber(L, video->GetVideoDuration());
			return 1;
		}
		static int SetImageScale(lua_State* L) noexcept
		{
			if (lua_gettop(L) <= 1)
			{
				float x = static_cast<float>(luaL_checknumber(L, 1));
				if (x == 0.f)
					return luaL_error(L, "invalid argument #1 for 'SetImageScale'.");
				LRES.SetGlobalImageScaleFactor(x);
			}
			else
			{
				core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
				if (!p)
					return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
				float x = (float)luaL_checknumber(L, 2);
				p->GetSprite()->setUnitsPerPixel(x);
			}
			return 0;
		}
		static int GetImageScale(lua_State* L) noexcept
		{
			if (lua_gettop(L) <= 0)
			{
				lua_Number ret = LRES.GetGlobalImageScaleFactor();
				lua_pushnumber(L, ret);
				return 1;
			}
			else
			{
				core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
				if (!p)
					return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
				lua_pushnumber(L, p->GetSprite()->getUnitsPerPixel());
				return 1;
			}
		}
		static int SetImageState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
				p->GetSprite()->setColor(*Color::Cast(L, 3));
			else if (lua_gettop(L) == 6)
			{
				core::Color4B tColors[] = {
					*Color::Cast(L, 3),
					*Color::Cast(L, 4),
					*Color::Cast(L, 5),
					*Color::Cast(L, 6)
				};
				p->GetSprite()->setColor(tColors);
			}
			return 0;
		}
		static int SetImageCenter(lua_State* L) noexcept
		{
			core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
			p->GetSprite()->setTextureCenter(core::Vector2F(
				static_cast<float>(luaL_checknumber(L, 2) + p->GetSprite()->getTextureRect().a.x),
				static_cast<float>(luaL_checknumber(L, 3) + p->GetSprite()->getTextureRect().a.y))
			);
			return 0;
		}

		static int SetAnimationScale(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsSpriteCloned())
				return luaL_error(L, "SetAnimationScale on animation '%s' is invalid, please set each sprite separately.");
			float x = (float)luaL_checknumber(L, 2);
			for (size_t i = 0; i < p->GetCount(); ++i)
				p->GetSprite((uint32_t)i)->GetSprite()->setUnitsPerPixel(x);
			return 0;
		}
		static int GetAnimationScale(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsSpriteCloned())
				return luaL_error(L, "GetAnimationScale on animation '%s' is invalid, please get from each sprite separately.");
			lua_pushnumber(L, p->GetSprite(0)->GetSprite()->getUnitsPerPixel());
			return 1;
		}
		static int SetAnimationState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
			{
				p->SetVertexColor(*Color::Cast(L, 3));
			}
			else if (lua_gettop(L) == 6)
			{
				core::Color4B tColors[] = {
					*Color::Cast(L, 3),
					*Color::Cast(L, 4),
					*Color::Cast(L, 5),
					*Color::Cast(L, 6)
				};
				p->SetVertexColor(tColors);
			}
			return 0;
		}
		static int SetAnimationCenter(lua_State* L) noexcept
		{
			core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
			if (!p->IsSpriteCloned())
				return luaL_error(L, "SetAnimationCenter on animation '%s' is invalid, please set each sprite separately.");
			for (size_t i = 0; i < p->GetCount(); ++i)
			{
				p->GetSprite((uint32_t)i)->GetSprite()->setTextureCenter(core::Vector2F(
					static_cast<float>(luaL_checknumber(L, 2) + p->GetSprite((uint32_t)i)->GetSprite()->getTextureRect().a.x),
					static_cast<float>(luaL_checknumber(L, 3) + p->GetSprite((uint32_t)i)->GetSprite()->getTextureRect().a.y)
				));
			}
			return 0;
		}

		static int SetFontState(lua_State* L) noexcept
		{
			core::SmartReference<IResourceFont> p = LRES.FindSpriteFont(luaL_checkstring(L, 1));
			if (!p)
				return luaL_error(L, "sprite font '%s' not found.", luaL_checkstring(L, 1));

			p->SetBlendMode(TranslateBlendMode(L, 2));
			if (lua_gettop(L) == 3)
			{
				p->SetBlendColor(*Color::Cast(L, 3));
			}
			return 0;
		}

		static int CacheTTFString(lua_State* L) {
			size_t len = 0;
			const char* str = luaL_checklstring(L, 2, &len);
			LRES.CacheTTFFontString(luaL_checkstring(L, 1), str, len);
			return 0;
		}
	};

	luaL_Reg const lib[] = {
		{ "SetResLoadInfo", &Wrapper::SetResLoadInfo },
		{ "IsRenderTarget", &Wrapper::IsRenderTarget },
		{ "SetTexturePreMulAlphaState", &Wrapper::SetTexturePreMulAlphaState },
		{ "SetTextureSamplerState", &Wrapper::SetTextureSamplerState },
		{ "GetTextureSize", &Wrapper::GetTextureSize },
		{ "PlayVideo", &Wrapper::PlayVideo },
		{ "PauseVideo", &Wrapper::PauseVideo },
		{ "ResumeVideo", &Wrapper::ResumeVideo },
		{ "StopVideo", &Wrapper::StopVideo },
		{ "SeekVideo", &Wrapper::SeekVideo },
		{ "GetVideoState", &Wrapper::GetVideoState },
		{ "GetVideoTime", &Wrapper::GetVideoTime },
		{ "GetVideoDuration", &Wrapper::GetVideoDuration },
		{ "SetImageScale", &Wrapper::SetImageScale },
		{ "GetImageScale", &Wrapper::GetImageScale },
		{ "SetImageState", &Wrapper::SetImageState },
		{ "SetImageCenter", &Wrapper::SetImageCenter },

		{ "SetAnimationScale", &Wrapper::SetAnimationScale },
		{ "GetAnimationScale", &Wrapper::GetAnimationScale },
		{ "SetAnimationState", &Wrapper::SetAnimationState },
		{ "SetAnimationCenter", &Wrapper::SetAnimationCenter },

		{ "SetFontState", &Wrapper::SetFontState },

		{ "CacheTTFString", &Wrapper::CacheTTFString },
		{ NULL, NULL },
	};

	luaL_Reg const pool_methods[] = {
		{ "loadTextureAsync", &Wrapper::LoadTextureAsync },
		{ "loadVideo", &Wrapper::LoadVideo },
		{ "loadVideoAsync", &Wrapper::LoadVideoAsync },
		{ "createSpriteAsync", &Wrapper::LoadSpriteAsync },
		{ "createAnimationAsync", &Wrapper::LoadAnimationAsync },
		{ "loadParticle", &Wrapper::LoadPS },
		{ "loadParticleAsync", &Wrapper::LoadPSAsync },
		{ "loadSound", &Wrapper::LoadSound },
		{ "loadSoundAsync", &Wrapper::LoadSoundAsync },
		{ "loadMusic", &Wrapper::LoadMusic },
		{ "loadMusicAsync", &Wrapper::LoadMusicAsync },
		{ "loadSpriteFont", &Wrapper::LoadFont },
		{ "loadSpriteFontAsync", &Wrapper::LoadFontAsync },
		{ "loadTTF", &Wrapper::LoadTTF },
		{ "loadTTFAsync", &Wrapper::LoadTTFAsync },
		{ "loadTrueTypeFont", &Wrapper::LoadTrueTypeFont },
		{ "loadTrueTypeFontAsync", &Wrapper::LoadTrueTypeFontAsync },
		{ "loadFX", &Wrapper::LoadFX },
		{ "loadFXAsync", &Wrapper::LoadFXAsync },
		{ "loadModel", &Wrapper::LoadModel },
		{ "loadModelAsync", &Wrapper::LoadModelAsync },
		{ "createRenderTarget", &Wrapper::CreateRenderTarget },
		{ NULL, NULL },
	};

	luaL_register(L, LUASTG_LUA_LIBNAME, lib);
	lua_pop(L, 1);
	luastg::binding::registerResourcePoolMethods(L, pool_methods);
}
