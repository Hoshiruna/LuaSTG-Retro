#include "GameResource/ResourceManager.h"
#include "GameResource/AsyncResourceLoader.hpp"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>

namespace luastg
{
	ResourceMgr::ResourceMgr()
		: m_AsyncLoader(std::make_unique<AsyncResourceLoader>())
	{
	}

	ResourceMgr::~ResourceMgr() = default;

	// 资源池管理

	void ResourceMgr::ClearAllResource() noexcept {
		if (m_AsyncLoader) {
			m_AsyncLoader->stop();
		}
		m_lookupOrder.clear();
		m_resourcePoolNames.clear();
		m_resourcePools.clear();
		m_GlobalImageScaleFactor = 1.0f;
	}

	size_t ResourceMgr::GetResourcePoolGeneration(ResourcePoolId const id) const noexcept {
		auto const* pool = GetResourcePool(id);
		return pool ? pool->GetGeneration() : 0;
	}

	void ResourceMgr::UpdateAsyncResourceLoading(size_t const max_count) {
		if (m_AsyncLoader) {
			m_AsyncLoader->update(*this, max_count);
		}
	}

	void ResourceMgr::CancelAsyncResourceLoading() noexcept {
		if (m_AsyncLoader) {
			m_AsyncLoader->cancelAll();
		}
	}

	void ResourceMgr::CancelAsyncResourceLoading(ResourcePoolId const pool_id) noexcept {
		if (m_AsyncLoader) {
			m_AsyncLoader->cancel(pool_id);
		}
	}

	std::shared_ptr<AsyncResourceJob> ResourceMgr::SubmitAsyncFileRead(std::string_view const path) {
		return m_AsyncLoader->submitFileRead(path);
	}

	std::shared_ptr<AsyncResourceJob> ResourceMgr::SubmitAsyncResource(AsyncResourceRequest request) {
		request.pool_generation = GetResourcePoolGeneration(request.pool_id);
		if (!GetResourcePool(request.pool_id)) {
			return m_AsyncLoader->submitFailedResource(std::move(request), "can't load resource at this time.");
		}
		return m_AsyncLoader->submitResource(std::move(request));
	}

	ResourcePoolId ResourceMgr::CreateResourcePool(std::string_view const name) noexcept {
		if (name.empty() || m_nextPoolId == InvalidResourcePoolId) {
			spdlog::warn("[luastg] rejected resource pool creation: name is empty or the pool ID space is exhausted");
			return InvalidResourcePoolId;
		}
		try {
			std::string const pool_name(name);
			if (m_resourcePoolNames.find(pool_name) != m_resourcePoolNames.end()) {
				spdlog::warn("[luastg] rejected resource pool creation: pool '{}' already exists", pool_name);
				return InvalidResourcePoolId;
			}
			ResourcePoolId const id = m_nextPoolId++;
			auto pool = std::make_unique<ResourcePool>(this, id, pool_name);
			m_resourcePools.emplace(id, std::move(pool));
			try {
				m_resourcePoolNames.emplace(pool_name, id);
			}
			catch (...) {
				m_resourcePools.erase(id);
				throw;
			}
			spdlog::info("[luastg] created resource pool '{}' (ID {})", pool_name, id);
			return id;
		}
		catch (std::exception const& e) {
			spdlog::error("[luastg] failed to create resource pool '{}': {}", name, e.what());
			return InvalidResourcePoolId;
		}
	}

	bool ResourceMgr::DestroyResourcePool(ResourcePoolId const id) noexcept {
		auto const it = m_resourcePools.find(id);
		if (it == m_resourcePools.end()) {
			spdlog::warn("[luastg] cannot destroy resource pool: ID {} is not live", id);
			return false;
		}
		CancelAsyncResourceLoading(id);
		m_lookupOrder.erase(std::remove(m_lookupOrder.begin(), m_lookupOrder.end(), id), m_lookupOrder.end());
		auto const pool_name = it->second->GetNameString();
		m_resourcePoolNames.erase(pool_name);
		m_resourcePools.erase(it);
		spdlog::info("[luastg] destroyed resource pool '{}' (ID {})", pool_name, id);
		return true;
	}

	ResourcePool* ResourceMgr::GetResourcePool(ResourcePoolId const id) noexcept {
		auto const it = m_resourcePools.find(id);
		return it != m_resourcePools.end() ? it->second.get() : nullptr;
	}

	ResourcePool const* ResourceMgr::GetResourcePool(ResourcePoolId const id) const noexcept {
		auto const it = m_resourcePools.find(id);
		return it != m_resourcePools.end() ? it->second.get() : nullptr;
	}

	ResourcePool* ResourceMgr::GetResourcePool(std::string_view const name) {
		auto const it = m_resourcePoolNames.find(std::string(name));
		return it != m_resourcePoolNames.end() ? GetResourcePool(it->second) : nullptr;
	}

	ResourcePool const* ResourceMgr::GetResourcePool(std::string_view const name) const {
		auto const it = m_resourcePoolNames.find(std::string(name));
		return it != m_resourcePoolNames.end() ? GetResourcePool(it->second) : nullptr;
	}

	std::vector<ResourcePool*> ResourceMgr::GetResourcePools() {
		std::vector<ResourcePool*> result;
		result.reserve(m_resourcePools.size());
		for (auto& item : m_resourcePools) {
			result.push_back(item.second.get());
		}
		std::sort(result.begin(), result.end(), [](ResourcePool const* left, ResourcePool const* right) {
			return left->GetId() < right->GetId();
		});
		return result;
	}

	bool ResourceMgr::SetLookupOrder(std::vector<ResourcePoolId> const& order) noexcept {
		try {
			std::unordered_set<ResourcePoolId> seen;
			for (ResourcePoolId const id : order) {
				if (!GetResourcePool(id)) {
					spdlog::warn("[luastg] rejected resource pool lookup order: ID {} is not live", id);
					return false;
				}
				if (!seen.emplace(id).second) {
					spdlog::warn("[luastg] rejected resource pool lookup order: pool '{}' (ID {}) appears more than once", GetResourcePool(id)->GetName(), id);
					return false;
				}
			}
			auto replacement = order;
			m_lookupOrder.swap(replacement);
			std::string description;
			for (ResourcePoolId const id : m_lookupOrder) {
				if (!description.empty()) {
					description.append(", ");
				}
				auto const* pool = GetResourcePool(id);
				description.push_back('\'');
				description.append(pool->GetName());
				description.append("' (ID ");
				description.append(std::to_string(id));
				description.push_back(')');
			}
			spdlog::info("[luastg] resource pool lookup order updated: [{}]", description);
			return true;
		}
		catch (...) {
			return false;
		}
	}

	// 自动查找资源池资源

	template<typename T>
	core::SmartReference<T> findResource(
		std::unordered_map<ResourcePoolId, std::unique_ptr<ResourcePool>>& pools,
		std::vector<ResourcePoolId> const& order,
		std::string_view const name,
		core::SmartReference<T> (ResourcePool::*getter)(std::string_view)) noexcept
	{
		for (ResourcePoolId const id : order) {
			auto const pool = pools.find(id);
			if (pool == pools.end()) {
				continue;
			}
			if (auto resource = (pool->second.get()->*getter)(name)) {
				return resource;
			}
		}
		return {};
	}

	core::SmartReference<IResourceTexture> ResourceMgr::FindTexture(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetTexture);
	}

	core::SmartReference<IResourceVideo> ResourceMgr::FindVideo(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetVideo);
	}

	core::SmartReference<IResourceSprite> ResourceMgr::FindSprite(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetSprite);
	}

	core::SmartReference<IResourceAnimation> ResourceMgr::FindAnimation(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetAnimation);
	}

	core::SmartReference<IResourceMusic> ResourceMgr::FindMusic(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetMusic);
	}

	core::SmartReference<IResourceSoundEffect> ResourceMgr::FindSound(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetSound);
	}

	core::SmartReference<IResourceParticle> ResourceMgr::FindParticle(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetParticle);
	}

	core::SmartReference<IResourceFont> ResourceMgr::FindSpriteFont(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetSpriteFont);
	}

	core::SmartReference<IResourceFont> ResourceMgr::FindTTFFont(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetTTFFont);
	}

	core::SmartReference<IResourcePostEffectShader> ResourceMgr::FindFX(const char* name) noexcept {
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetFX);
	}

	core::SmartReference<IResourceModel> ResourceMgr::FindModel(const char* name) noexcept
	{
		return findResource(m_resourcePools, m_lookupOrder, name, &ResourcePool::GetModel);
	}

	// 其他资源操作

	bool ResourceMgr::GetTextureSize(const char* name, core::Vector2U& out) noexcept {
		core::SmartReference<IResourceTexture> tRet = FindTexture(name);
		if (!tRet)
			return false;
		out = tRet->GetTexture()->getSize();
		return true;
	}

	void ResourceMgr::CacheTTFFontString(const char* name, const char* text, size_t len) noexcept {
		core::SmartReference<IResourceFont> f = FindTTFFont(name);
		if (f)
			f->GetGlyphManager()->cacheString(core::StringView(text, len));
		else
			spdlog::error("[luastg] CacheTTFFontString: 缓存字形时未找到指定字体'{}'", name);
	}

	void ResourceMgr::UpdateSound()
	{
		for (auto& pool : m_resourcePools) {
			for (auto& sound : pool.second->m_SoundSpritePool) {
				sound.second->FlushCommand();
			}
		}
	}

	void ResourceMgr::UpdateVideo(double const delta_seconds)
	{
		for (auto& pool : m_resourcePools) {
			pool.second->UpdateVideo(delta_seconds);
		}
	}

	// 其他

	#ifdef LDEVVERSION
	bool ResourceMgr::g_ResourceLoadingLog = true;
	#else
	bool ResourceMgr::g_ResourceLoadingLog = false;
	#endif

	void ResourceMgr::SetResourceLoadingLog(bool b) { g_ResourceLoadingLog = b; }

	bool ResourceMgr::GetResourceLoadingLog() { return g_ResourceLoadingLog; }
}
