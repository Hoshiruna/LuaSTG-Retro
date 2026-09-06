#include "GameResource/ResourceManager.h"
#include "GameResource/Implement/ResourceTextureImpl.hpp"
#include "GameResource/Implement/ResourceVideoImpl.hpp"
#include "GameResource/Implement/ResourceSpriteImpl.hpp"
#include "GameResource/Implement/ResourceAnimationImpl.hpp"
#include "GameResource/Implement/ResourceMusicImpl.hpp"
#include "GameResource/Implement/ResourceSoundEffectImpl.hpp"
#include "GameResource/Implement/ResourceParticleImpl.hpp"
#include "GameResource/Implement/ResourceFontImpl.hpp"
#include "GameResource/Implement/ResourcePostEffectShaderImpl.hpp"
#include "GameResource/Implement/ResourceModelImpl.hpp"
#include "core/AudioSystem.hpp"
#include "core/FileSystem.hpp"
#include "core/VideoDecoder.hpp"
#include "AppFrame.h"
#include "lua/plus.hpp"
#include <utility>

namespace luastg
{
    // Resource pool management

    void ResourcePool::Clear() noexcept
    {
        if(m_pMgr) {
            m_pMgr->CancelAsyncResourceLoading(m_id);
        }
        ++m_generation;
        m_TexturePool.clear();
        m_SpritePool.clear();
        m_AnimationPool.clear();
        m_MusicPool.clear();
        m_SoundSpritePool.clear();
        m_ParticlePool.clear();
        m_SpriteFontPool.clear();
        m_TTFFontPool.clear();
        m_FXPool.clear();
        m_ModelPool.clear();
        spdlog::info("[luastg] Resource pool cleared '{}'", getResourcePoolName());
    }

    template<typename T>
    inline void removeResource(T& pool, const char* name, const char* pool_name)
    {
        auto i = pool.find(std::string_view(name));
        if(i == pool.end()) {
            spdlog::warn("[luastg] RemoveResource: attempting to unload a resource that does not exist '{}' (resource pool '{}')", name, pool_name);
            return;
        }
        pool.erase(i);
        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] RemoveResource: resource '{}' has been unloaded (resource pool '{}')", name, pool_name);
        }
    }

    void ResourcePool::RemoveResource(ResourceType t, const char* name) noexcept
    {
        switch(t) {
            case ResourceType::Texture:
                removeResource(m_TexturePool, name, getResourcePoolName());
                break;
            case ResourceType::Sprite:
                removeResource(m_SpritePool, name, getResourcePoolName());
                break;
            case ResourceType::Animation:
                removeResource(m_AnimationPool, name, getResourcePoolName());
                break;
            case ResourceType::Music:
                removeResource(m_MusicPool, name, getResourcePoolName());
                break;
            case ResourceType::SoundEffect:
                removeResource(m_SoundSpritePool, name, getResourcePoolName());
                break;
            case ResourceType::Particle:
                removeResource(m_ParticlePool, name, getResourcePoolName());
                break;
            case ResourceType::SpriteFont:
                removeResource(m_SpriteFontPool, name, getResourcePoolName());
                break;
            case ResourceType::TrueTypeFont:
                removeResource(m_TTFFontPool, name, getResourcePoolName());
                break;
            case ResourceType::FX:
                removeResource(m_FXPool, name, getResourcePoolName());
                break;
            case ResourceType::Model:
                removeResource(m_ModelPool, name, getResourcePoolName());
                break;
            default:
                spdlog::warn("[luastg] RemoveResource: attempt to remove a resource type that does not exist ({}) (resource pool '{}')", (int)t, getResourcePoolName());
                return;
        }
    }

    bool ResourcePool::CheckResourceExists(ResourceType t, std::string_view name) const noexcept
    {
        switch(t) {
            case ResourceType::Texture:
                return m_TexturePool.find(name) != m_TexturePool.end();
            case ResourceType::Sprite:
                return m_SpritePool.find(name) != m_SpritePool.end();
            case ResourceType::Animation:
                return m_AnimationPool.find(name) != m_AnimationPool.end();
            case ResourceType::Music:
                return m_MusicPool.find(name) != m_MusicPool.end();
            case ResourceType::SoundEffect:
                return m_SoundSpritePool.find(name) != m_SoundSpritePool.end();
            case ResourceType::Particle:
                return m_ParticlePool.find(name) != m_ParticlePool.end();
            case ResourceType::SpriteFont:
                return m_SpriteFontPool.find(name) != m_SpriteFontPool.end();
            case ResourceType::TrueTypeFont:
                return m_TTFFontPool.find(name) != m_TTFFontPool.end();
            case ResourceType::FX:
                return m_FXPool.find(name) != m_FXPool.end();
            case ResourceType::Model:
                return m_ModelPool.find(name) != m_ModelPool.end();
            default:
                spdlog::warn("[luastg] CheckRes: attempt to retrieve a resource type that does not exist ({}) (resource pool '{}')", (int)t, getResourcePoolName());
                break;
        }
        return false;
    }

    template<typename T>
    inline void listResourceName(lua_State* L, T& resource_set)
    {
        lua::stack_t S(L);
        int index = 0;
        S.create_array(resource_set.size());
        for(auto& i : resource_set) {
            auto ptr = i.second;
            index += 1;
            S.set_array_value<std::string_view>(index, ptr->GetResName());
        }
    }

    int ResourcePool::ExportResourceList(lua_State* L, ResourceType t) const noexcept
    {
        lua::stack_t S(L);
        switch(t) {
            case ResourceType::Texture:
                listResourceName(L, m_TexturePool);
                break;
            case ResourceType::Sprite:
                listResourceName(L, m_SpritePool);
                break;
            case ResourceType::Animation:
                listResourceName(L, m_AnimationPool);
                break;
            case ResourceType::Music:
                listResourceName(L, m_MusicPool);
                break;
            case ResourceType::SoundEffect:
                listResourceName(L, m_SoundSpritePool);
                break;
            case ResourceType::Particle:
                listResourceName(L, m_ParticlePool);
                break;
            case ResourceType::SpriteFont:
                listResourceName(L, m_SpriteFontPool);
                break;
            case ResourceType::TrueTypeFont:
                listResourceName(L, m_TTFFontPool);
                break;
            case ResourceType::FX:
                listResourceName(L, m_FXPool);
                break;
            case ResourceType::Model:
                listResourceName(L, m_ModelPool);
                break;
            default:
                spdlog::warn("[luastg] EnumRes: attempt to enumerate a resource type that does not exist ({}) (resource pool '{}')", (int)t, getResourcePoolName());
                S.create_array(0);
                break;
        }
        return 1;
    }

    // Load textures

    bool ResourcePool::LoadTexture(const char* name, const char* path, bool mipmaps) noexcept
    {
        if(m_TexturePool.find(std::string_view(name)) != m_TexturePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadTexture: texture '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        core::SmartReference<core::Graphics::ITexture2D> p_texture;
        if(!LAPP.GetAppModel()->getDevice()->createTextureFromFile(path, mipmaps, p_texture.put())) {
            spdlog::error("[luastg] failed to create texture from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
            return false;
        }

        try {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceTextureImpl(name, p_texture.get()));
            m_TexturePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadTexture: failed to create texture '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadTexture: loaded texture from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadTexture(const char* name, core::IData* data, const char* path, bool mipmaps) noexcept
    {
        if(m_TexturePool.find(std::string_view(name)) != m_TexturePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadTexture: texture '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        core::SmartReference<core::Graphics::ITexture2D> p_texture;
        if(!LAPP.GetAppModel()->getDevice()->createTextureFromData(data, mipmaps, p_texture.put())) {
            spdlog::error("[luastg] failed to create texture from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
            return false;
        }

        try {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceTextureImpl(name, p_texture.get()));
            m_TexturePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadTexture: failed to create texture '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadTexture: loaded texture from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadVideo(const char* name, const char* path, bool loop) noexcept
    {
        if(m_TexturePool.find(std::string_view(name)) != m_TexturePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadVideo: video texture '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceVideoImpl(name, path, loop));
            m_TexturePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadVideo: failed to load video texture from '{}' as '{}' ({}) (resource pool '{}')", path, name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadVideo: loaded video texture from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadVideo(const char* name, core::IVideoDecoder* decoder, bool loop) noexcept
    {
        if(m_TexturePool.find(std::string_view(name)) != m_TexturePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadVideo: video texture '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceVideoImpl(name, decoder, loop));
            m_TexturePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadVideo: failed to load video texture '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadVideo: loaded video texture '{}' (resource pool '{}')", name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::CreateTexture(const char* name, int width, int height) noexcept
    {
        if(m_TexturePool.find(std::string_view(name)) != m_TexturePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadTexture: texture '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        core::SmartReference<core::Graphics::ITexture2D> p_texture;
        if(!LAPP.GetAppModel()->getDevice()->createTexture(core::Vector2U((uint32_t)width, (uint32_t)height), p_texture.put())) {
            spdlog::error("[luastg] failed to create texture '{}' ({}x{}) (resource pool '{}')", name, width, height, getResourcePoolName());
            return false;
        }

        try {
            core::SmartReference<IResourceTexture> tRes;
            tRes.attach(new ResourceTextureImpl(name, p_texture.get()));
            m_TexturePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadTexture: {} (resource pool '{}')", e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadTexture: created texture '{}' ({}x{}) (resource pool '{}')", name, width, height, getResourcePoolName());
        }

        return true;
    }

    // Create render targets

    bool ResourcePool::CreateRenderTarget(const char* name, int width, int height, bool depth_buffer) noexcept
    {
        if(m_TexturePool.find(std::string_view(name)) != m_TexturePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] CreateRenderTarget: render target '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        std::string_view ds_info(" and depth buffer");

        try {
            core::SmartReference<IResourceTexture> tRes;
            if(width <= 0 || height <= 0) {
                tRes.attach(new ResourceTextureImpl(name, depth_buffer));
            } else {
                tRes.attach(new ResourceTextureImpl(name, width, height, depth_buffer));
            }
            m_TexturePool.emplace(name, tRes);
        } catch(std::runtime_error const& e) {
            spdlog::error("[luastg] CreateRenderTarget: failed to create render target '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            if(width <= 0 || height <= 0) {
                spdlog::info("[luastg] CreateRenderTarget: created render target{} '{}' (resource pool '{}')", ds_info, name, getResourcePoolName());
            } else {
                spdlog::info("[luastg] CreateRenderTarget: created render target{} '{}' ({}x{}) (resource pool '{}')", ds_info, name, width, height, getResourcePoolName());
            }
        }

        return true;
    }

    // Create sprites

    bool ResourcePool::CreateSprite(const char* name, IResourceTexture* texture, double x, double y, double w, double h, double a, double b, bool rect) noexcept
    {
        if(m_SpritePool.find(std::string_view(name)) != m_SpritePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] CreateSprite: sprite '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        if(!texture) {
            spdlog::error("[luastg] CreateSprite: texture is null for '{}' (resource pool '{}')", name, getResourcePoolName());
            return false;
        }
        auto const texname = texture->GetResName();

        core::SmartReference<core::Graphics::ISprite> p_sprite;
        if(!core::Graphics::ISprite::create(
               LAPP.GetAppModel()->getRenderer(),
               texture->GetTexture(),
               p_sprite.put())) {
            spdlog::error("[luastg] failed to create sprite from texture '{}' as '{}' (resource pool '{}')", texname, name, getResourcePoolName());
            return false;
        }
        p_sprite->setTextureRect(core::RectF((float)x, (float)y, (float)(x + w), (float)(y + h)));
        p_sprite->setTextureCenter(core::Vector2F((float)(x + w * 0.5), (float)(y + h * 0.5)));

        try {
            core::SmartReference<IResourceSprite> tRes;
            tRes.attach(new ResourceSpriteImpl(name, p_sprite.get(), a, b, rect));
            m_SpritePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] CreateSprite: failed to create sprite '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] CreateSprite: created sprite from texture '{}' as '{}' (resource pool '{}')", texname, name, getResourcePoolName());
        }

        return true;
    }

    // Create animation sprites

    bool ResourcePool::CreateAnimation(const char* name, IResourceTexture* texture, double x, double y, double w, double h, int n, int m, int intv, double a, double b, bool rect) noexcept
    {
        if(m_AnimationPool.find(std::string_view(name)) != m_AnimationPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] CreateAnimation: animation sprite '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        if(!texture) {
            spdlog::error("[luastg] CreateAnimation: texture is null for '{}' (resource pool '{}')", name, getResourcePoolName());
            return false;
        }
        auto const texname = texture->GetResName();
        core::SmartReference<IResourceTexture> pTex(texture);

        try {
            core::SmartReference<IResourceAnimation> tRes;
            tRes.attach(
                new ResourceAnimationImpl(name, pTex, (float)x, (float)y, (float)w, (float)h, n, m, intv, a, b, rect));
            m_AnimationPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] CreateAnimation: failed to create animation sprite '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] CreateAnimation: created animation sprite from '{}' as '{}' (resource pool '{}')", texname, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::CreateAnimation(const char* name,
        std::vector<core::SmartReference<IResourceSprite>> const& sprite_list,
        int intv,
        double a,
        double b,
        bool rect) noexcept
    {
        if(m_AnimationPool.find(std::string_view(name)) != m_AnimationPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] CreateAnimation: animation sprite '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourceAnimation> tRes;
            tRes.attach(
                new ResourceAnimationImpl(name, sprite_list, intv, a, b, rect));
            m_AnimationPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] CreateAnimation: failed to create animation sprite '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] CreateAnimation: created animation sprite '{}' (resource pool '{}')", name, getResourcePoolName());
        }

        return true;
    }

    // Load music

    bool ResourcePool::LoadMusic(const char* name, const char* path, core::AudioFrameRange loop_range) noexcept
    {
        core::SmartReference<core::IData> data;
        if(!core::FileSystemManager::readFile(path, data.put())) {
            spdlog::error("[luastg] LoadMusic: failed to read file '{}' (resource pool '{}')", path, getResourcePoolName());
            return false;
        }
        return LoadMusic(name, data.get(), path, loop_range);
    }

    bool ResourcePool::LoadMusic(const char* name, core::IData* data, const char* path, core::AudioFrameRange loop_range) noexcept
    {
        if(m_MusicPool.find(std::string_view(name)) != m_MusicPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog())
                spdlog::warn("[luastg] LoadMusic: music '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            return false;
        }

        core::SmartReference<core::IAudioAsset> asset;
        if(!LAPP.getAudioSystem()->createAudioAsset(path, data, core::AudioLoadMode::streaming, core::AudioBus::music, asset.put())) {
            spdlog::error("[luastg] LoadMusic: failed to decode file '{}'; expected WAV/OGG/FLAC format (resource pool '{}')", path, getResourcePoolName());
            return false;
        }

        if(loop_range.begin == 0 && loop_range.end == 0)
            loop_range.end = asset->getFrameCount();
        if(loop_range.begin >= loop_range.end || loop_range.end > asset->getFrameCount()) {
            spdlog::error("[luastg] LoadMusic: invalid loop range for music '{}' (start_frame = {}, end_frame = {}) (resource pool '{}')", name, loop_range.begin, loop_range.end, getResourcePoolName());
            return false;
        }

        try {
            core::SmartReference<IResourceMusic> resource;
            resource.attach(new ResourceMusicImpl(name, asset.get(), loop_range));
            m_MusicPool.emplace(name, resource);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadMusic: failed to load music '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }
        if(ResourceMgr::GetResourceLoadingLog())
            spdlog::info("[luastg] LoadMusic: loaded music from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        return true;
    }

    // Load sound effects

    bool ResourcePool::LoadSoundEffect(const char* name, const char* path) noexcept
    {
        if(m_SoundSpritePool.find(std::string_view(name)) != m_SoundSpritePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadSoundEffect: sound effect '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        core::SmartReference<core::IData> data;
        if(!core::FileSystemManager::readFile(path, data.put())) {
            spdlog::error("[luastg] LoadSoundEffect: failed to read file '{}' (resource pool '{}')", path, getResourcePoolName());
            return false;
        }
        return LoadSoundEffect(name, data.get(), path);
    }

    bool ResourcePool::LoadSoundEffect(const char* name, core::IData* data, const char* path) noexcept
    {
        if(m_SoundSpritePool.find(std::string_view(name)) != m_SoundSpritePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadSoundEffect: sound effect '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        core::SmartReference<core::IAudioAsset> asset;
        if(!LAPP.getAudioSystem()->createAudioAsset(path, data, core::AudioLoadMode::decoded, core::AudioBus::sound_effect, asset.put())) {
            spdlog::error("[luastg] LoadSoundEffect: failed to decode file '{}'; expected WAV/OGG/FLAC format (resource pool '{}')", path, getResourcePoolName());
            return false;
        }

        try {
            core::SmartReference<IResourceSoundEffect> tRes;
            tRes.attach(new ResourceSoundEffectImpl(name, asset.get()));
            m_SoundSpritePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadSoundEffect: failed to load sound effect '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadSoundEffect: loaded sound effect from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    // Create particle effects

    bool ResourcePool::LoadParticle(const char* name, const hgeParticleSystemInfo& info, IResourceSprite* sprite, double a, double b, bool rect, bool _nolog) noexcept
    {
        if(m_ParticlePool.find(std::string_view(name)) != m_ParticlePool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadParticle: particle effect '{}' already exists; creation operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        if(!sprite) {
            spdlog::error("[luastg] LoadParticle: sprite is null for '{}' (resource pool '{}')", name, getResourcePoolName());
            return false;
        }
        auto const img_name = sprite->GetResName();

        core::SmartReference<core::Graphics::ISprite> p_sprite;
        if(!sprite->GetSprite()->clone(p_sprite.put())) {
            spdlog::error("[luastg] LoadParticle: failed to create particle effect '{}': failed to clone sprite '{}' (resource pool '{}')", name, img_name, getResourcePoolName());
            return false;
        }

        try {
            core::SmartReference<IResourceParticle> tRes;
            tRes.attach(new ResourceParticleImpl(name, info, p_sprite.get(), a, b, rect));
            m_ParticlePool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadParticle: failed to create particle effect '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(!_nolog && ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadParticle: created particle effect '{}' (resource pool '{}')", name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadParticle(const char* name, const char* path, IResourceSprite* sprite, double a, double b, bool rect) noexcept
    {
        core::SmartReference<core::IData> src;
        if(!core::FileSystemManager::readFile(path, src.put())) {
            spdlog::error("[luastg] LoadParticle: failed to load particle effect from '{}' as '{}': failed to read file (resource pool '{}')", path, name, getResourcePoolName());
            return false;
        }

        if(src->size() != sizeof(hgeParticleSystemInfo)) {
            spdlog::error("[luastg] LoadParticle: invalid format for particle effect definition file '{}' (resource pool '{}')", path, getResourcePoolName());
            return false;
        }
        hgeParticleSystemInfo tInfo;
        std::memcpy(&tInfo, src->data(), sizeof(hgeParticleSystemInfo));

        if(!LoadParticle(name, tInfo, sprite, a, b, rect, /* _nolog */ true)) {
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadParticle: created particle effect from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    // Load HGE sprite fonts

    bool ResourcePool::LoadSpriteFont(const char* name, const char* path, bool mipmaps) noexcept
    {
        if(m_SpriteFontPool.find(std::string_view(name)) != m_SpriteFontPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadSpriteFont: sprite font '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        // Create the font resource
        try {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, path, mipmaps));
            m_SpriteFontPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadSpriteFont: failed to load HGE sprite font '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadSpriteFont: loaded HGE sprite font from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadSpriteFont(const char* name, core::IData* font_data, const char* path, core::IData* texture_data, const char* texture_path, bool mipmaps) noexcept
    {
        if(m_SpriteFontPool.find(std::string_view(name)) != m_SpriteFontPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadSpriteFont: sprite font '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, path, font_data, texture_data, texture_path, mipmaps));
            m_SpriteFontPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadSpriteFont: failed to load HGE sprite font '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadSpriteFont: loaded HGE sprite font from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    // Load fancy2d sprite fonts

    bool ResourcePool::LoadSpriteFont(const char* name, const char* path, const char* tex_path, bool mipmaps) noexcept
    {
        if(m_SpriteFontPool.find(std::string_view(name)) != m_SpriteFontPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadSpriteFont: sprite font '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        // Create the font resource
        try {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, path, tex_path, mipmaps));
            m_SpriteFontPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadSpriteFont: failed to load fancy2d sprite font '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadSpriteFont: loaded fancy2d sprite font from '{}' and '{}' as '{}' (resource pool '{}')", path, tex_path, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadSpriteFont(const char* name, core::IData* font_data, const char* path, const char* tex_path, core::IData* texture_data, bool mipmaps) noexcept
    {
        if(m_SpriteFontPool.find(std::string_view(name)) != m_SpriteFontPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadSpriteFont: sprite font '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, path, font_data, tex_path, texture_data, mipmaps));
            m_SpriteFontPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadSpriteFont: failed to load fancy2d sprite font '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadSpriteFont: loaded fancy2d sprite font from '{}' and '{}' as '{}' (resource pool '{}')", path, tex_path, name, getResourcePoolName());
        }

        return true;
    }

    // Load TrueType fonts

    bool ResourcePool::LoadDynamicFont(const char* name, core::Graphics::TrueTypeFontInfo const* fonts, size_t count, DynamicFontLoadOptions const& options) noexcept
    {
        if(!fonts || count == 0) {
            spdlog::error("[luastg] LoadDynamicFont: no font sources were provided for '{}' (resource pool '{}')", name, getResourcePoolName());
            return false;
        }
        if(m_TTFFontPool.find(std::string_view(name)) != m_TTFFontPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadDynamicFont: dynamic font '{}' already exists (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        std::vector<core::SmartReference<core::IData>> source_data(count);
        std::vector<core::Graphics::TrueTypeFontInfo> buffered_fonts(fonts, fonts + count);
        for(size_t i = 0; i < count; ++i) {
            if(fonts[i].is_buffer) {
                continue;
            }
            if(!core::FileSystemManager::readFile(fonts[i].source, source_data[i].put())) {
                spdlog::error("[luastg] LoadDynamicFont: failed to read font source '{}' (resource pool '{}')", fonts[i].source, getResourcePoolName());
                return false;
            }
            buffered_fonts[i].source = core::StringView(
                static_cast<char const*>(source_data[i]->data()),
                source_data[i]->size());
            buffered_fonts[i].is_buffer = true;
            buffered_fonts[i].is_force_to_file = false;
        }
        return LoadTrueTypeFont(name, buffered_fonts.data(), buffered_fonts.size(), options);
    }

    bool ResourcePool::LoadTrueTypeFont(const char* name, core::Graphics::TrueTypeFontInfo* fonts, size_t count, DynamicFontLoadOptions const& options) noexcept
    {
        if(m_TTFFontPool.find(std::string_view(name)) != m_TTFFontPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadTrueTypeFont: TrueType font collection '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        core::SmartReference<core::Graphics::IGlyphManager> p_glyphmgr;
        auto* const sampler = LAPP.GetRenderer2D()->getKnownSamplerState(options.sampler);
        if(!core::Graphics::IGlyphManager::create(
               LAPP.GetAppModel()->getDevice(), fonts, count, options.rasterization, sampler, p_glyphmgr.put())) {
            spdlog::error("[luastg] LoadTrueTypeFont: failed to load TrueType font collection '{}' (resource pool '{}')", name, getResourcePoolName());
            return false;
        }

        // Create the font resource
        try {
            core::SmartReference<IResourceFont> tRes;
            tRes.attach(new ResourceFontImpl(name, p_glyphmgr.get()));
            m_TTFFontPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadTrueTypeFont: failed to load TrueType font collection '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadTrueTypeFont: loaded TrueType font collection '{}' (resource pool '{}')", name, getResourcePoolName());
        }

        return true;
    }

    // Load post-processing effects

    bool ResourcePool::LoadFX(const char* name, const char* path) noexcept
    {
        if(m_FXPool.find(std::string_view(name)) != m_FXPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadFX: post-processing effect '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourcePostEffectShader> tRes;
            tRes.attach(new ResourcePostEffectShaderImpl(name, path));
            if(!tRes->GetPostEffectShader()) {
                spdlog::error("[luastg] LoadFX: failed to load post-processing effect from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
                return false;
            }
            m_FXPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadFX: failed to load post-processing effect '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadFX: loaded post-processing effect from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    bool ResourcePool::LoadFXFromSource(const char* name, std::string_view source, const char* path) noexcept
    {
        if(m_FXPool.find(std::string_view(name)) != m_FXPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadFX: post-processing effect '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourcePostEffectShader> tRes;
            tRes.attach(new ResourcePostEffectShaderImpl(name, source, true));
            if(!tRes->GetPostEffectShader()) {
                spdlog::error("[luastg] LoadFX: failed to load post-processing effect from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
                return false;
            }
            m_FXPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadFX: failed to load post-processing effect '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadFX: loaded post-processing effect from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    // Load models

    bool ResourcePool::LoadModel(const char* name, const char* path) noexcept
    {
        if(m_ModelPool.find(std::string_view(name)) != m_ModelPool.end()) {
            if(ResourceMgr::GetResourceLoadingLog()) {
                spdlog::warn("[luastg] LoadModel: model '{}' already exists; load operation has been canceled (resource pool '{}')", name, getResourcePoolName());
            }
            return false;
        }

        try {
            core::SmartReference<IResourceModel> tRes;
            tRes.attach(new ResourceModelImpl(name, path));
            m_ModelPool.emplace(name, tRes);
        } catch(std::exception const& e) {
            spdlog::error("[luastg] LoadModel: failed to load model '{}' ({}) (resource pool '{}')", name, e.what(), getResourcePoolName());
            return false;
        }

        if(ResourceMgr::GetResourceLoadingLog()) {
            spdlog::info("[luastg] LoadModel: loaded model from '{}' as '{}' (resource pool '{}')", path, name, getResourcePoolName());
        }

        return true;
    }

    // Find and retrieve resources

    template<typename T>
    inline T::value_type::second_type findResource(T& resource_set, std::string_view name)
    {
        auto i = resource_set.find(name);
        if(i == resource_set.end())
            return {};
        else
            return i->second;
    }

    core::SmartReference<IResourceTexture> ResourcePool::GetTexture(std::string_view name) noexcept
    {
        return findResource(m_TexturePool, name);
    }

    core::SmartReference<IResourceVideo> ResourcePool::GetVideo(std::string_view name) noexcept
    {
        auto texture = GetTexture(name);
        if(!texture) {
            return {};
        }
        auto* video = dynamic_cast<IResourceVideo*>(texture.get());
        if(!video) {
            return {};
        }
        core::SmartReference<IResourceVideo> result;
        result = video;
        return result;
    }

    core::SmartReference<IResourceSprite> ResourcePool::GetSprite(std::string_view name) noexcept
    {
        return findResource(m_SpritePool, name);
    }

    core::SmartReference<IResourceAnimation> ResourcePool::GetAnimation(std::string_view name) noexcept
    {
        return findResource(m_AnimationPool, name);
    }

    core::SmartReference<IResourceMusic> ResourcePool::GetMusic(std::string_view name) noexcept
    {
        return findResource(m_MusicPool, name);
    }

    core::SmartReference<IResourceSoundEffect> ResourcePool::GetSound(std::string_view name) noexcept
    {
        return findResource(m_SoundSpritePool, name);
    }

    core::SmartReference<IResourceParticle> ResourcePool::GetParticle(std::string_view name) noexcept
    {
        return findResource(m_ParticlePool, name);
    }

    core::SmartReference<IResourceFont> ResourcePool::GetSpriteFont(std::string_view name) noexcept
    {
        return findResource(m_SpriteFontPool, name);
    }

    core::SmartReference<IResourceFont> ResourcePool::GetTTFFont(std::string_view name) noexcept
    {
        return findResource(m_TTFFontPool, name);
    }

    core::SmartReference<IResourcePostEffectShader> ResourcePool::GetFX(std::string_view name) noexcept
    {
        return findResource(m_FXPool, name);
    }

    core::SmartReference<IResourceModel> ResourcePool::GetModel(std::string_view name) noexcept
    {
        return findResource(m_ModelPool, name);
    }

    ResourcePool::ResourcePool(ResourceMgr* mgr, ResourcePoolId const id, std::string name)
        : m_pMgr(mgr), m_id(id), m_name(std::move(name))
    {
    }

    void ResourcePool::UpdateVideo(double delta_seconds)
    {
        for(auto& item : m_TexturePool) {
            if(auto* video = dynamic_cast<IResourceVideo*>(item.second.get())) {
                video->Update(delta_seconds);
            }
        }
    }
}
