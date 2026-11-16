#include "LuaBinding/LuaWrapper.hpp"
#include "LuaBinding/AsyncResourceJob.hpp"
#include "LuaBinding/Resource.hpp"
#include "LuaBinding/modern/DynamicFont.hpp"
#include "GameResource/AsyncResourceLoader.hpp"
#include "lua/plus.hpp"
#include "AppFrame.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    struct ParsedDynamicFontDescriptor
    {
        std::vector<core::Graphics::TrueTypeFontInfo> fonts;
        std::vector<std::string> paths;
        luastg::DynamicFontLoadOptions options;
    };

    int absoluteIndex(lua_State* const L, int const index) noexcept
    {
        return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(L) + index + 1;
    }

    float optionalNumberField(lua_State* const L, int const table_index, char const* const name, float const fallback)
    {
        lua_getfield(L, table_index, name);
        auto const value = lua_isnil(L, -1) ? fallback : static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        return value;
    }

    uint32_t optionalIntegerField(lua_State* const L, int const table_index, char const* const name, uint32_t const fallback)
    {
        lua_getfield(L, table_index, name);
        if(lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return fallback;
        }
        auto const value = luaL_checknumber(L, -1);
        luaL_argcheck(
            L,
            std::isfinite(value) && value >= 0 && value <= std::numeric_limits<uint32_t>::max() && std::floor(value) == value,
            table_index,
            "faceIndex must be a non-negative 32-bit integer");
        lua_pop(L, 1);
        return static_cast<uint32_t>(value);
    }

    std::string_view optionalStringField(lua_State* const L, int const table_index, char const* const name, std::string_view const fallback, bool* const was_present = nullptr)
    {
        lua_getfield(L, table_index, name);
        if(lua_isnil(L, -1)) {
            lua_pop(L, 1);
            if(was_present) {
                *was_present = false;
            }
            return fallback;
        }
        size_t length{};
        auto const* const value = luaL_checklstring(L, -1, &length);
        lua_pop(L, 1);
        if(was_present) {
            *was_present = true;
        }
        return std::string_view(value, length);
    }

    uint8_t optionalAlphaThreshold(lua_State* const L, int const table_index)
    {
        lua_getfield(L, table_index, "alphaThreshold");
        if(lua_isnil(L, -1)) {
            lua_pop(L, 1);
            return 0;
        }
        auto const value = luaL_checknumber(L, -1);
        luaL_argcheck(L, std::isfinite(value) && value >= 0 && value <= 255 && std::floor(value) == value, table_index, "alphaThreshold must be an integer from 0 through 255");
        lua_pop(L, 1);
        return static_cast<uint8_t>(value);
    }

    std::string_view checkDynamicFontName(lua_State* const L, int const index)
    {
        size_t length{};
        auto const* const value = luaL_checklstring(L, index, &length);
        std::string_view const name(value, length);
        luaL_argcheck(
            L,
            !name.empty() && name.find('\0') == std::string_view::npos,
            index,
            "dynamic font name must be non-empty and cannot contain NUL bytes");
        return name;
    }

    ParsedDynamicFontDescriptor parseDynamicFontDescriptor(lua_State* const L, int const descriptor_index)
    {
        auto const descriptor = absoluteIndex(L, descriptor_index);
        luaL_checktype(L, descriptor, LUA_TTABLE);

        lua_getfield(L, descriptor, "pixelHeight");
        auto const default_height = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        auto const default_width = optionalNumberField(L, descriptor, "pixelWidth", 0.0f);
        luaL_argcheck(L, std::isfinite(default_width) && default_width >= 0.0f, descriptor_index, "pixelWidth must be finite and non-negative");
        luaL_argcheck(L, std::isfinite(default_height) && default_height > 0.0f, descriptor_index, "pixelHeight must be finite and greater than zero");

        lua_getfield(L, descriptor, "sources");
        auto const sources_index = absoluteIndex(L, -1);
        luaL_checktype(L, sources_index, LUA_TTABLE);
        auto const count = static_cast<size_t>(lua_objlen(L, sources_index));
        luaL_argcheck(L, count > 0, descriptor_index, "sources must contain at least one font source");

        ParsedDynamicFontDescriptor result;
        auto const raster_mode = optionalStringField(L, descriptor, "rasterMode", "grayscale");
        if(raster_mode == "grayscale") {
            result.options.rasterization.raster_mode = core::Graphics::GlyphRasterMode::Grayscale;
        } else if(raster_mode == "monochrome") {
            result.options.rasterization.raster_mode = core::Graphics::GlyphRasterMode::Monochrome;
        } else {
            luaL_argerror(L, descriptor_index, "rasterMode must be 'grayscale' or 'monochrome'");
        }

        auto const hinting = optionalStringField(L, descriptor, "hinting", "native");
        if(hinting == "native") {
            result.options.rasterization.hinting = core::Graphics::GlyphHintingMode::Native;
        } else if(hinting == "auto") {
            result.options.rasterization.hinting = core::Graphics::GlyphHintingMode::Auto;
        } else if(hinting == "none") {
            result.options.rasterization.hinting = core::Graphics::GlyphHintingMode::None;
        } else {
            luaL_argerror(L, descriptor_index, "hinting must be 'native', 'auto', or 'none'");
        }

        auto const default_target = result.options.rasterization.raster_mode == core::Graphics::GlyphRasterMode::Monochrome ? std::string_view("monochrome") : std::string_view("normal");
        auto const hinting_target = optionalStringField(L, descriptor, "hintingTarget", default_target);
        if(hinting_target == "normal") {
            result.options.rasterization.hinting_target = core::Graphics::GlyphHintingTarget::Normal;
        } else if(hinting_target == "light") {
            result.options.rasterization.hinting_target = core::Graphics::GlyphHintingTarget::Light;
        } else if(hinting_target == "monochrome") {
            result.options.rasterization.hinting_target = core::Graphics::GlyphHintingTarget::Monochrome;
        } else {
            luaL_argerror(L, descriptor_index, "hintingTarget must be 'normal', 'light', or 'monochrome'");
        }
        result.options.rasterization.alpha_threshold = optionalAlphaThreshold(L, descriptor);

        auto const sampler = optionalStringField(L, descriptor, "sampler", "linear+clamp");
        if(sampler == "point+wrap") {
            result.options.sampler = core::Graphics::IRenderer::SamplerState::PointWrap;
        } else if(sampler == "point+clamp") {
            result.options.sampler = core::Graphics::IRenderer::SamplerState::PointClamp;
        } else if(sampler == "linear+wrap") {
            result.options.sampler = core::Graphics::IRenderer::SamplerState::LinearWrap;
        } else if(sampler == "linear+clamp") {
            result.options.sampler = core::Graphics::IRenderer::SamplerState::LinearClamp;
        } else {
            luaL_argerror(L, descriptor_index, "sampler must be 'point+wrap', 'point+clamp', 'linear+wrap', or 'linear+clamp'");
        }

        result.fonts.resize(count);
        result.paths.resize(count);
        for(size_t i = 0; i < count; ++i) {
            lua_rawgeti(L, sources_index, static_cast<int>(i + 1));
            auto const source_index = absoluteIndex(L, -1);
            luaL_checktype(L, source_index, LUA_TTABLE);

            lua_getfield(L, source_index, "path");
            size_t path_length{};
            auto const* const path = luaL_checklstring(L, -1, &path_length);
            result.paths[i].assign(path, path_length);
            lua_pop(L, 1);
            luaL_argcheck(
                L,
                !result.paths[i].empty() && result.paths[i].find('\0') == std::string::npos,
                descriptor_index,
                "font source paths must be non-empty and cannot contain NUL bytes");

            auto& font = result.fonts[i];
            font.source = result.paths[i];
            font.font_face = optionalIntegerField(L, source_index, "faceIndex", 0);
            font.font_size.x = optionalNumberField(L, source_index, "pixelWidth", default_width);
            font.font_size.y = optionalNumberField(L, source_index, "pixelHeight", default_height);
            font.is_force_to_file = false;
            font.is_buffer = false;
            luaL_argcheck(
                L,
                std::isfinite(font.font_size.x) && font.font_size.x >= 0.0f,
                descriptor_index,
                "source pixelWidth must be finite and non-negative");
            luaL_argcheck(
                L,
                std::isfinite(font.font_size.y) && font.font_size.y > 0.0f,
                descriptor_index,
                "source pixelHeight must be finite and greater than zero");
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        return result;
    }

}

void
luastg::binding::ResourceManager::Register(lua_State* L) noexcept
{
    struct Wrapper
    {
        static ResourcePool* BeginPoolCall(lua_State* L)
        {
            ResourcePool* pool = nullptr;
            if(lua_type(L, 1) == LUA_TSTRING) {
                auto const pool_name = std::string_view(luaL_checkstring(L, 1));
                pool = LRES.GetResourcePool(pool_name);
                if(!pool) {
                    luaL_error(L, "specified resource pool '%s' was not found", pool_name.data());
                }
            } else {
                pool = luastg::binding::checkResourcePool(L, 1);
            }
            lua_remove(L, 1);
            return pool;
        }
        static int SetResLoadInfo(lua_State* L) noexcept
        {
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");
            if(!pActivedPool->LoadTexture(name, path, lua_toboolean(L, 3) == 0 ? false : true))
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");
            bool const loop = lua_gettop(L) >= 3 ? lua_toboolean(L, 3) != 0 : false;
            if(!pActivedPool->LoadVideo(name, path, loop))
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            if(!pActivedPool->CreateSprite(
                   name,
                   texture,
                   luaL_checknumber(L, 3),
                   luaL_checknumber(L, 4),
                   luaL_checknumber(L, 5),
                   luaL_checknumber(L, 6),
                   luaL_optnumber(L, 7, 0.),
                   luaL_optnumber(L, 8, 0.),
                   lua_toboolean(L, 9) == 0 ? false : true)) {
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            if(lua_istable(L, 2)) {
                std::vector<core::SmartReference<IResourceSprite>> sprites;
                sprites.reserve(lua_objlen(L, 2));
                for(int i = 1; i <= static_cast<int>(lua_objlen(L, 2)); i += 1) {
                    lua_pushinteger(L, i);
                    lua_gettable(L, 2);
                    sprites.emplace_back(luastg::binding::checkResourceSprite(L, -1));
                    lua_pop(L, 1);
                }
                if(!pActivedPool->CreateAnimation(
                       name,
                       sprites,
                       luaL_checkinteger(L, 3),
                       luaL_optnumber(L, 4, 0.0f),
                       luaL_optnumber(L, 5, 0.0f),
                       lua_toboolean(L, 6) != 0)) {
                    return luaL_error(L, "load animation failed (name='%s').", name);
                }
            } else {
                auto* texture = luastg::binding::checkResourceTexture(L, 2);
                if(!pActivedPool->CreateAnimation(
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
                       lua_toboolean(L, 12) != 0)) {
                    return luaL_error(L, "load animation failed (name='%s').", name);
                }
            }

            return 0;
        }
        static int LoadAnimationAsync(lua_State* L) noexcept
        {
            auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Animation);
            if(lua_istable(L, 2)) {
                request.animation_uses_sprite_list = true;
                int const count = static_cast<int>(lua_objlen(L, 2));
                request.sprites.reserve(count);
                for(int i = 1; i <= count; i += 1) {
                    lua_pushinteger(L, i);
                    lua_gettable(L, 2);
                    request.sprites.emplace_back(luastg::binding::checkResourceSprite(L, -1));
                    lua_pop(L, 1);
                }
                request.interval = static_cast<int>(luaL_checkinteger(L, 3));
                request.a = luaL_optnumber(L, 4, 0.0f);
                request.b = luaL_optnumber(L, 5, 0.0f);
                request.rect = lua_toboolean(L, 6) != 0;
            } else {
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
            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            const char* name = luaL_checkstring(L, 1);
            auto* sprite = luastg::binding::checkResourceSprite(L, 3);
            if(lua_type(L, 2) == LUA_TTABLE) {
                hgeParticleSystemInfo info;
                bool ret = TranslateTableToParticleInfo(L, 2, info);
                if(!ret)
                    return luaL_error(L, "load particle failed (name='%s', define=?).", name);
                if(!pActivedPool->LoadParticle(
                       name,
                       info,
                       sprite,
                       luaL_optnumber(L, 4, 0.0f),
                       luaL_optnumber(L, 5, 0.0f),
                       lua_toboolean(L, 6) == 0 ? false : true)) {
                    return luaL_error(L, "load particle failed (name='%s', define=table).", name);
                }
                lua_pushboolean(L, true);
                return 1;
            } else {
                const char* path = luaL_checkstring(L, 2);

                if(!pActivedPool->LoadParticle(
                       name,
                       path,
                       sprite,
                       luaL_optnumber(L, 4, 0.0f),
                       luaL_optnumber(L, 5, 0.0f),
                       lua_toboolean(L, 6) == 0 ? false : true)) {
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
            if(lua_type(L, 2) == LUA_TTABLE) {
                request.has_particle_info = true;
                if(!TranslateTableToParticleInfo(L, 2, request.particle_info)) {
                    return luaL_error(L, "load particle failed (name='%s', define=?).", request.name.c_str());
                }
            } else {
                request.path = luaL_checkstring(L, 2);
            }
            return PushAsyncJob(L, std::move(request));
        }
        static int LoadSound(lua_State* L) noexcept
        {
            ResourcePool* pActivedPool = BeginPoolCall(L);
            const char* name = luaL_checkstring(L, 1);
            const char* path = luaL_checkstring(L, 2);

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            if(!pActivedPool->LoadSoundEffect(name, path))
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            core::AudioFrameRange loop_range{};
            if(!lua_isnoneornil(L, 3)) {
                luaL_checktype(L, 3, LUA_TTABLE);
                lua_getfield(L, 3, "start_frame");
                auto const start = luaL_optnumber(L, -1, 0);
                lua_pop(L, 1);
                lua_getfield(L, 3, "end_frame");
                auto const end = luaL_optnumber(L, -1, 0);
                lua_pop(L, 1);
                if(start < 0 || end < 0)
                    return luaL_error(L, "music loop frames cannot be negative");
                loop_range = { static_cast<uint64_t>(start), static_cast<uint64_t>(end) };
            }

            if(!pActivedPool->LoadMusic(
                   name,
                   path,
                   loop_range)) {
                return luaL_error(L, "load music failed (name=%s, path=%s)", name, path);
            }
            lua_pushboolean(L, true);
            return 1;
        }
        static int LoadMusicAsync(lua_State* L) noexcept
        {
            auto request = CreateAsyncRequest(L, AsyncResourceRequestType::Music);
            request.path = luaL_checkstring(L, 2);
            if(!lua_isnoneornil(L, 3)) {
                luaL_checktype(L, 3, LUA_TTABLE);
                lua_getfield(L, 3, "start_frame");
                auto const start = luaL_optnumber(L, -1, 0);
                lua_pop(L, 1);
                lua_getfield(L, 3, "end_frame");
                auto const end = luaL_optnumber(L, -1, 0);
                lua_pop(L, 1);
                if(start < 0 || end < 0)
                    return luaL_error(L, "music loop frames cannot be negative");
                request.loop_start_frame = static_cast<uint64_t>(start);
                request.loop_end_frame = static_cast<uint64_t>(end);
            }
            return PushAsyncJob(L, std::move(request));
        }
        static int LoadFont(lua_State* L) noexcept
        {
            ResourcePool* pActivedPool = BeginPoolCall(L);
            bool bSucceed = false;
            const char* name = luaL_checkstring(L, 1);
            const char* path = luaL_checkstring(L, 2);

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            if(lua_gettop(L) == 2) {
                // HGE字体 mipmap=true
                bSucceed = pActivedPool->LoadSpriteFont(name, path);
            } else {
                if(lua_isboolean(L, 3)) {
                    // HGE字体 mipmap=user_defined
                    bSucceed = pActivedPool->LoadSpriteFont(name, path, lua_toboolean(L, 3) == 0 ? false : true);
                } else {
                    // fancy2d字体
                    const char* texpath = luaL_checkstring(L, 3);
                    if(lua_gettop(L) == 4)
                        bSucceed = pActivedPool->LoadSpriteFont(name, path, texpath, lua_toboolean(L, 4) == 0 ? false : true);
                    else
                        bSucceed = pActivedPool->LoadSpriteFont(name, path, texpath);
                }
            }

            if(!bSucceed)
                return luaL_error(L, "can't load font from file '%s'.", path);
            lua_pushboolean(L, true);
            return 1;
        }
        static int LoadFontAsync(lua_State* L) noexcept
        {
            auto request = CreateAsyncRequest(L, AsyncResourceRequestType::SpriteFont);
            request.path = luaL_checkstring(L, 2);
            request.mipmap = true;
            if(lua_gettop(L) >= 3) {
                if(lua_isboolean(L, 3)) {
                    request.mipmap = lua_toboolean(L, 3) != 0;
                } else {
                    request.has_texture_path = true;
                    request.texture_path = luaL_checkstring(L, 3);
                    if(lua_gettop(L) >= 4) {
                        request.mipmap = lua_toboolean(L, 4) != 0;
                    }
                }
            }
            return PushAsyncJob(L, std::move(request));
        }
        static int LoadDynamicFont(lua_State* L) noexcept
        {
            auto* const pool = BeginPoolCall(L);
            auto const name = checkDynamicFontName(L, 1);
            auto descriptor = parseDynamicFontDescriptor(L, 2);
            if(!pool->LoadDynamicFont(
                   name.data(), descriptor.fonts.data(), descriptor.fonts.size(), descriptor.options)) {
                lua_pushnil(L);
                lua_pushfstring(L, "failed to load dynamic font '%s'", name.data());
                return 2;
            }
            auto resource = pool->GetTTFFont(name);
            DynamicFont::push(L, resource.get());
            return 1;
        }
        static int LoadDynamicFontAsync(lua_State* L) noexcept
        {
            auto* const pool = BeginPoolCall(L);
            auto const name = checkDynamicFontName(L, 1);
            AsyncResourceRequest request;
            request.type = AsyncResourceRequestType::TrueTypeFont;
            request.pool_id = pool->GetId();
            request.name.assign(name);
            auto descriptor = parseDynamicFontDescriptor(L, 2);
            for(auto& font : descriptor.fonts) {
                font.source = {};
            }
            request.fonts = std::move(descriptor.fonts);
            request.font_sources = std::move(descriptor.paths);
            request.dynamic_font_options = descriptor.options;
            return PushAsyncJob(L, std::move(request));
        }
        static int GetDynamicFont(lua_State* L) noexcept
        {
            auto* const pool = BeginPoolCall(L);
            auto resource = pool->GetTTFFont(checkDynamicFontName(L, 1));
            if(!resource) {
                lua_pushnil(L);
                return 1;
            }
            DynamicFont::push(L, resource.get());
            return 1;
        }
        static int HasDynamicFont(lua_State* L) noexcept
        {
            auto* const pool = BeginPoolCall(L);
            lua_pushboolean(L, static_cast<bool>(pool->GetTTFFont(checkDynamicFontName(L, 1))));
            return 1;
        }
        static int LoadFX(lua_State* L) noexcept
        {
            ResourcePool* pActivedPool = BeginPoolCall(L);
            const char* name = luaL_checkstring(L, 1);
            const char* path = luaL_checkstring(L, 2);

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            if(!pActivedPool->LoadFX(name, path))
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");
            if(!pActivedPool->LoadModel(
                   name,
                   model_path)) {
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

            if(!pActivedPool)
                return luaL_error(L, "can't load resource at this time.");

            if(lua_gettop(L) >= 3) {
                const int width = (int)luaL_checkinteger(L, 2);
                const int height = (int)luaL_checkinteger(L, 3);
                if(width < 1 || height < 1)
                    return luaL_error(L, "invalid render target size (%dx%d).", width, height);
                bool depth_buffer = true;
                if(lua_gettop(L) >= 4)
                    depth_buffer = lua_toboolean(L, 4);
                if(!pActivedPool->CreateRenderTarget(name, width, height, depth_buffer))
                    return luaL_error(L, "can't create render target with name '%s'.", name);
            } else {
                if(!pActivedPool->CreateRenderTarget(name, 0, 0, true))
                    return luaL_error(L, "can't create render target with name '%s'.", name);
            }
            auto resource = pActivedPool->GetTexture(name);
            luastg::binding::pushResourceTexture(L, resource.get());
            return 1;
        }
        static int IsRenderTarget(lua_State* L) noexcept
        {
            core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
            if(!p)
                return luaL_error(L, "render target '%s' not found.", luaL_checkstring(L, 1));
            lua_pushboolean(L, p->IsRenderTarget());
            return 1;
        }
        static int SetTexturePreMulAlphaState(lua_State* L) noexcept
        {
            core::SmartReference<IResourceTexture> p = LRES.FindTexture(luaL_checkstring(L, 1));
            if(p) {
                p->GetTexture()->setPremultipliedAlpha(lua_toboolean(L, 2));
                return 0;
            }
            return luaL_error(L, "texture '%s' not found.", luaL_checkstring(L, 1));
        }
        static int SetTextureSamplerState(lua_State* L) noexcept
        {
            lua::stack_t S(L);
            std::string_view const sampler_name = S.get_value<std::string_view>(2);
            if(sampler_name == "" || sampler_name == "point+wrap" || sampler_name == "point+clamp" || sampler_name == "linear+wrap" || sampler_name == "linear+clamp") {
                std::string_view const tex_name = S.get_value<std::string_view>(1);
                core::SmartReference<IResourceTexture> p = LRES.FindTexture(tex_name.data());
                if(!p) {
                    spdlog::error("[luastg] lstg.SetTextureSamplerState failed: can't find texture '{}'", tex_name);
                    return luaL_error(L, "can't find texture '%s'", tex_name.data());
                }

                // 映射
                core::Graphics::IRenderer::SamplerState state = core::Graphics::IRenderer::SamplerState::LinearClamp;
                if(sampler_name == "point+wrap")
                    state = core::Graphics::IRenderer::SamplerState::PointWrap;
                else if(sampler_name == "point+clamp")
                    state = core::Graphics::IRenderer::SamplerState::PointClamp;
                else if(sampler_name == "linear+wrap")
                    state = core::Graphics::IRenderer::SamplerState::LinearWrap;
                else if(sampler_name == "" || sampler_name == "linear+clamp")
                    state = core::Graphics::IRenderer::SamplerState::LinearClamp;
                else
                    return luaL_error(L, "unknown sampler state '%s'", sampler_name.data());

                // 设置
                core::Graphics::ISamplerState* p_sampler = LAPP.GetRenderer2D()->getKnownSamplerState(state);
                p->GetTexture()->setSamplerState(p_sampler);

                return 0;
            } else {
                return luaL_error(L, "unsupported deprecated usage");
            }
        }
        static int GetTextureSize(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            core::Vector2U size;
            if(!LRES.GetTextureSize(name, size))
                return luaL_error(L, "texture '%s' not found.", name);
            lua_pushinteger(L, (lua_Integer)size.x);
            lua_pushinteger(L, (lua_Integer)size.y);
            return 2;
        }
        static int PlayVideo(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            bool const restart = lua_gettop(L) >= 2 ? lua_toboolean(L, 2) != 0 : true;
            if(!video->Play(restart))
                return luaL_error(L, "can't play video '%s'.", name);
            return 0;
        }
        static int PauseVideo(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            if(!video->Pause())
                return luaL_error(L, "can't pause video '%s'.", name);
            return 0;
        }
        static int ResumeVideo(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            if(!video->Resume())
                return luaL_error(L, "can't resume video '%s'.", name);
            return 0;
        }
        static int StopVideo(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            if(!video->Stop())
                return luaL_error(L, "can't stop video '%s'.", name);
            return 0;
        }
        static int SeekVideo(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            if(!video->Seek(luaL_checknumber(L, 2)))
                return luaL_error(L, "can't seek video '%s'.", name);
            return 0;
        }
        static int GetVideoState(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            switch(video->GetVideoState()) {
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
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            lua_pushnumber(L, video->GetVideoTime());
            return 1;
        }
        static int GetVideoDuration(lua_State* L) noexcept
        {
            const char* name = luaL_checkstring(L, 1);
            auto video = LRES.FindVideo(name);
            if(!video)
                return luaL_error(L, "video '%s' not found.", name);
            lua_pushnumber(L, video->GetVideoDuration());
            return 1;
        }
        static int SetImageScale(lua_State* L) noexcept
        {
            if(lua_gettop(L) <= 1) {
                float x = static_cast<float>(luaL_checknumber(L, 1));
                if(x == 0.f)
                    return luaL_error(L, "invalid argument #1 for 'SetImageScale'.");
                LRES.SetGlobalImageScaleFactor(x);
            } else {
                core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
                if(!p)
                    return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
                float x = (float)luaL_checknumber(L, 2);
                p->GetSprite()->setUnitsPerPixel(x);
            }
            return 0;
        }
        static int GetImageScale(lua_State* L) noexcept
        {
            if(lua_gettop(L) <= 0) {
                lua_Number ret = LRES.GetGlobalImageScaleFactor();
                lua_pushnumber(L, ret);
                return 1;
            } else {
                core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
                if(!p)
                    return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
                lua_pushnumber(L, p->GetSprite()->getUnitsPerPixel());
                return 1;
            }
        }
        static int SetImageState(lua_State* L) noexcept
        {
            core::SmartReference<IResourceSprite> p = LRES.FindSprite(luaL_checkstring(L, 1));
            if(!p)
                return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));

            p->SetBlendMode(TranslateBlendMode(L, 2));
            if(lua_gettop(L) == 3)
                p->GetSprite()->setColor(*Color::Cast(L, 3));
            else if(lua_gettop(L) == 6) {
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
            if(!p)
                return luaL_error(L, "image '%s' not found.", luaL_checkstring(L, 1));
            p->GetSprite()->setTextureCenter(core::Vector2F(
                static_cast<float>(luaL_checknumber(L, 2) + p->GetSprite()->getTextureRect().a.x),
                static_cast<float>(luaL_checknumber(L, 3) + p->GetSprite()->getTextureRect().a.y)));
            return 0;
        }

        static int SetAnimationScale(lua_State* L) noexcept
        {
            core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
            if(!p)
                return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
            if(!p->IsSpriteCloned())
                return luaL_error(L, "SetAnimationScale on animation '%s' is invalid, please set each sprite separately.");
            float x = (float)luaL_checknumber(L, 2);
            for(size_t i = 0; i < p->GetCount(); ++i)
                p->GetSprite((uint32_t)i)->GetSprite()->setUnitsPerPixel(x);
            return 0;
        }
        static int GetAnimationScale(lua_State* L) noexcept
        {
            core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
            if(!p)
                return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
            if(!p->IsSpriteCloned())
                return luaL_error(L, "GetAnimationScale on animation '%s' is invalid, please get from each sprite separately.");
            lua_pushnumber(L, p->GetSprite(0)->GetSprite()->getUnitsPerPixel());
            return 1;
        }
        static int SetAnimationState(lua_State* L) noexcept
        {
            core::SmartReference<IResourceAnimation> p = LRES.FindAnimation(luaL_checkstring(L, 1));
            if(!p)
                return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));

            p->SetBlendMode(TranslateBlendMode(L, 2));
            if(lua_gettop(L) == 3) {
                p->SetVertexColor(*Color::Cast(L, 3));
            } else if(lua_gettop(L) == 6) {
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
            if(!p)
                return luaL_error(L, "animation '%s' not found.", luaL_checkstring(L, 1));
            if(!p->IsSpriteCloned())
                return luaL_error(L, "SetAnimationCenter on animation '%s' is invalid, please set each sprite separately.");
            for(size_t i = 0; i < p->GetCount(); ++i) {
                p->GetSprite((uint32_t)i)->GetSprite()->setTextureCenter(core::Vector2F(static_cast<float>(luaL_checknumber(L, 2) + p->GetSprite((uint32_t)i)->GetSprite()->getTextureRect().a.x), static_cast<float>(luaL_checknumber(L, 3) + p->GetSprite((uint32_t)i)->GetSprite()->getTextureRect().a.y)));
            }
            return 0;
        }

        static int SetFontState(lua_State* L) noexcept
        {
            core::SmartReference<IResourceFont> p = LRES.FindSpriteFont(luaL_checkstring(L, 1));
            if(!p)
                return luaL_error(L, "sprite font '%s' not found.", luaL_checkstring(L, 1));

            p->SetBlendMode(TranslateBlendMode(L, 2));
            if(lua_gettop(L) == 3) {
                p->SetBlendColor(*Color::Cast(L, 3));
            }
            return 0;
        }
    };

    luaL_Reg const lib[] = {
        { "SetResLoadInfo", &Wrapper::SetResLoadInfo },
        { "LoadTexture", &Wrapper::LoadTexture },
        { "LoadTextureAsync", &Wrapper::LoadTextureAsync },
        { "LoadVideo", &Wrapper::LoadVideo },
        { "LoadVideoAsync", &Wrapper::LoadVideoAsync },
        { "LoadImage", &Wrapper::LoadSprite },
        { "LoadImageAsync", &Wrapper::LoadSpriteAsync },
        { "LoadAnimation", &Wrapper::LoadAnimation },
        { "LoadAnimationAsync", &Wrapper::LoadAnimationAsync },
        { "LoadPS", &Wrapper::LoadPS },
        { "LoadPSAsync", &Wrapper::LoadPSAsync },
        { "LoadFont", &Wrapper::LoadFont },
        { "LoadFontAsync", &Wrapper::LoadFontAsync },
        { "LoadDynamicFont", &Wrapper::LoadDynamicFont },
        { "LoadFX", &Wrapper::LoadFX },
        { "LoadFXAsync", &Wrapper::LoadFXAsync },
        { "LoadModel", &Wrapper::LoadModel },
        { "LoadModelAsync", &Wrapper::LoadModelAsync },
        { "CreateRenderTarget", &Wrapper::CreateRenderTarget },
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
        { "loadDynamicFont", &Wrapper::LoadDynamicFont },
        { "loadDynamicFontAsync", &Wrapper::LoadDynamicFontAsync },
        { "getDynamicFont", &Wrapper::GetDynamicFont },
        { "hasDynamicFont", &Wrapper::HasDynamicFont },
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
