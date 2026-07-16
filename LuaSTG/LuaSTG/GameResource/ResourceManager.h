#pragma once
#include "core/SmartReference.hpp"
#include "GameResource/ResourceTexture.hpp"
#include "GameResource/ResourceVideo.hpp"
#include "GameResource/ResourceSprite.hpp"
#include "GameResource/ResourceAnimation.hpp"
#include "GameResource/ResourceMusic.hpp"
#include "GameResource/ResourceSoundEffect.hpp"
#include "GameResource/ResourceParticle.hpp"
#include "GameResource/ResourceFont.hpp"
#include "GameResource/ResourcePostEffectShader.hpp"
#include "GameResource/ResourceModel.hpp"
#include "lua.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace core
{
    struct IData;
    struct IAudioDecoder;
    struct IVideoDecoder;
}

namespace luastg
{
    class AsyncResourceLoader;
    class AsyncResourceJob;
    struct AsyncResourceRequest;
    class ResourceMgr;
    
    using ResourcePoolId = uint64_t;
    inline constexpr ResourcePoolId InvalidResourcePoolId = 0;
    
    // 资源池
    class ResourcePool
    {
        friend class ResourceMgr;
    public:
        struct dictionary_hash_t
        {
            using is_transparent = void;
            size_t operator()(std::string_view value) const noexcept { return std::hash<std::string_view>{}(value); }
            size_t operator()(std::string const& value) const noexcept { return operator()(std::string_view(value)); }
            size_t operator()(char const* value) const noexcept { return operator()(std::string_view(value)); }
        };
        template<typename T>
        using dictionary_t = std::unordered_map<std::string, T, dictionary_hash_t, std::equal_to<>>;
    private:
        ResourceMgr* m_pMgr;
        ResourcePoolId m_id;
        std::string m_name;
        dictionary_t<core::SmartReference<IResourceTexture>> m_TexturePool;
        dictionary_t<core::SmartReference<IResourceSprite>> m_SpritePool;
        dictionary_t<core::SmartReference<IResourceAnimation>> m_AnimationPool;
        dictionary_t<core::SmartReference<IResourceMusic>> m_MusicPool;
        dictionary_t<core::SmartReference<IResourceSoundEffect>> m_SoundSpritePool;
        dictionary_t<core::SmartReference<IResourceParticle>> m_ParticlePool;
        dictionary_t<core::SmartReference<IResourceFont>> m_SpriteFontPool;
        dictionary_t<core::SmartReference<IResourceFont>> m_TTFFontPool;
        dictionary_t<core::SmartReference<IResourcePostEffectShader>> m_FXPool;
        dictionary_t<core::SmartReference<IResourceModel>> m_ModelPool;
    private:
        const char* getResourcePoolName() const noexcept { return m_name.c_str(); }
    public:
        void Clear() noexcept;
        void RemoveResource(ResourceType t, const char* name) noexcept;
        bool CheckResourceExists(ResourceType t, std::string_view name) const noexcept;
        int ExportResourceList(lua_State* L, ResourceType t) const  noexcept;
        
        // 纹理
        bool LoadTexture(const char* name, const char* path, bool mipmaps = true) noexcept;
        bool LoadTexture(const char* name, core::IData* data, const char* path, bool mipmaps = true) noexcept;
        bool LoadVideo(const char* name, const char* path, bool loop = false) noexcept;
        bool LoadVideo(const char* name, core::IVideoDecoder* decoder, bool loop = false) noexcept;
        bool CreateTexture(const char* name, int width, int height) noexcept;
        // 渲染目标
        bool CreateRenderTarget(const char* name, int width = 0, int height = 0, bool depth_buffer = false) noexcept;
        // 图片精灵
        bool CreateSprite(const char* name, IResourceTexture* texture,
                          double x, double y, double w, double h,
                          double a, double b, bool rect = false) noexcept;
        // 动画精灵
        bool CreateAnimation(const char* name, IResourceTexture* texture,
                             double x, double y, double w, double h, int n, int m, int intv,
                             double a, double b, bool rect = false) noexcept;
        bool CreateAnimation(const char* name,
            std::vector<core::SmartReference<IResourceSprite>> const& sprite_list,
            int intv,
            double a, double b, bool rect = false) noexcept;
        // 音乐
        bool LoadMusic(const char* name, const char* path, double start, double end, bool once_decode) noexcept;
        bool LoadMusic(const char* name, core::IAudioDecoder* decoder, const char* path, double start, double end, bool once_decode) noexcept;
        // 音效
        bool LoadSoundEffect(const char* name, const char* path) noexcept;
        bool LoadSoundEffect(const char* name, core::IAudioDecoder* decoder, const char* path) noexcept;
        // 粒子特效(HGE)
        bool LoadParticle(const char* name, const hgeParticleSystemInfo& info, IResourceSprite* sprite,
                          double a, double b, bool rect = false, bool _nolog = false) noexcept;
        bool LoadParticle(const char* name, const char* path, IResourceSprite* sprite,
                          double a, double b, bool rect = false) noexcept;
        // 装载纹理字体(HGE)
        bool LoadSpriteFont(const char* name, const char* path, bool mipmaps = true) noexcept;
        bool LoadSpriteFont(const char* name, core::IData* font_data, const char* path, core::IData* texture_data, const char* texture_path, bool mipmaps = true) noexcept;
        // 装载纹理字体(fancy2d)
        bool LoadSpriteFont(const char* name, const char* path, const char* tex_path, bool mipmaps = true) noexcept;
        bool LoadSpriteFont(const char* name, core::IData* font_data, const char* path, const char* tex_path, core::IData* texture_data, bool mipmaps = true) noexcept;
        // 加载矢量字体
        bool LoadTTFFont(const char* name, const char* path, float width, float height) noexcept;
        bool LoadTTFFont(const char* name, core::IData* data, float width, float height) noexcept;
        bool LoadTrueTypeFont(const char* name, core::Graphics::TrueTypeFontInfo* fonts, size_t count) noexcept;
        // 特效
        bool LoadFX(const char* name, const char* path) noexcept;
        bool LoadFXFromSource(const char* name, std::string_view source, const char* path) noexcept;
        // 模型
        bool LoadModel(const char* name, const char* path) noexcept;
        
        core::SmartReference<IResourceTexture> GetTexture(std::string_view name) noexcept;
        core::SmartReference<IResourceVideo> GetVideo(std::string_view name) noexcept;
        core::SmartReference<IResourceSprite> GetSprite(std::string_view name) noexcept;
        core::SmartReference<IResourceAnimation> GetAnimation(std::string_view name) noexcept;
        core::SmartReference<IResourceMusic> GetMusic(std::string_view name) noexcept;
        core::SmartReference<IResourceSoundEffect> GetSound(std::string_view name) noexcept;
        core::SmartReference<IResourceParticle> GetParticle(std::string_view name) noexcept;
        core::SmartReference<IResourceFont> GetSpriteFont(std::string_view name) noexcept;
        core::SmartReference<IResourceFont> GetTTFFont(std::string_view name) noexcept;
        core::SmartReference<IResourcePostEffectShader> GetFX(std::string_view name) noexcept;
        core::SmartReference<IResourceModel> GetModel(std::string_view name) noexcept;
    public:
        ResourcePool(ResourceMgr* mgr, ResourcePoolId id, std::string name);
        void UpdateVideo(double delta_seconds);
        ResourcePoolId GetId() const noexcept { return m_id; }
        std::string_view GetName() const noexcept { return m_name; }
        std::string const& GetNameString() const noexcept { return m_name; }
        size_t GetGeneration() const noexcept { return m_generation; }
        ResourcePool& operator=(const ResourcePool&) = delete;
        ResourcePool(const ResourcePool&) = delete;
    private:
        size_t m_generation{ 0 };
    };
    
    // 资源管理器
    class ResourceMgr
    {
    private:
        ResourcePoolId m_nextPoolId = 1;
        std::unordered_map<ResourcePoolId, std::unique_ptr<ResourcePool>> m_resourcePools;
        std::unordered_map<std::string, ResourcePoolId> m_resourcePoolNames;
        std::vector<ResourcePoolId> m_lookupOrder;
    public:
        ResourcePoolId CreateResourcePool(std::string_view name) noexcept;
        bool DestroyResourcePool(ResourcePoolId id) noexcept;
        ResourcePool* GetResourcePool(ResourcePoolId id) noexcept;
        ResourcePool const* GetResourcePool(ResourcePoolId id) const noexcept;
        ResourcePool* GetResourcePool(std::string_view name);
        ResourcePool const* GetResourcePool(std::string_view name) const;
        std::vector<ResourcePool*> GetResourcePools();
        bool SetLookupOrder(std::vector<ResourcePoolId> const& order) noexcept;
        std::vector<ResourcePoolId> const& GetLookupOrder() const noexcept { return m_lookupOrder; }
        void ClearAllResource() noexcept;
        size_t GetResourcePoolGeneration(ResourcePoolId id) const noexcept;
        void UpdateAsyncResourceLoading(size_t max_count = 8);
        void CancelAsyncResourceLoading() noexcept;
        void CancelAsyncResourceLoading(ResourcePoolId pool_id) noexcept;
        std::shared_ptr<AsyncResourceJob> SubmitAsyncFileRead(std::string_view path);
        std::shared_ptr<AsyncResourceJob> SubmitAsyncResource(AsyncResourceRequest request);

        core::SmartReference<IResourceTexture> FindTexture(const char* name) noexcept;
        core::SmartReference<IResourceVideo> FindVideo(const char* name) noexcept;
        core::SmartReference<IResourceSprite> FindSprite(const char* name) noexcept;
        core::SmartReference<IResourceAnimation> FindAnimation(const char* name) noexcept;
        core::SmartReference<IResourceMusic> FindMusic(const char* name) noexcept;
        core::SmartReference<IResourceSoundEffect> FindSound(const char* name) noexcept;
        core::SmartReference<IResourceParticle> FindParticle(const char* name) noexcept;
        core::SmartReference<IResourceFont> FindSpriteFont(const char* name) noexcept;
        core::SmartReference<IResourceFont> FindTTFFont(const char* name) noexcept;
        core::SmartReference<IResourcePostEffectShader> FindFX(const char* name) noexcept;
        core::SmartReference<IResourceModel> FindModel(const char* name) noexcept;
        
        bool GetTextureSize(const char* name, core::Vector2U& out) noexcept;
        void CacheTTFFontString(const char* name, const char* text, size_t len) noexcept;
        void UpdateSound();
        void UpdateVideo(double delta_seconds);
    private:
        static bool g_ResourceLoadingLog;
        float m_GlobalImageScaleFactor = 1.0f;
        std::unique_ptr<AsyncResourceLoader> m_AsyncLoader;
    public:
        static void SetResourceLoadingLog(bool b);
        static bool GetResourceLoadingLog();
        float GetGlobalImageScaleFactor() const noexcept { return m_GlobalImageScaleFactor; }
        void SetGlobalImageScaleFactor(float s) noexcept { m_GlobalImageScaleFactor = s; }
        void ShowResourceManagerDebugWindow(bool* p_open = nullptr);
    public:
        ResourceMgr();
        ~ResourceMgr();
    };
}
