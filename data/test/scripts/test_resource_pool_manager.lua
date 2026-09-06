local test = require("test")
local resources = require("resource_pool")

---@class test.Module.ResourcePoolManager : test.Base
local M = {}

function M:onCreate()
    local manager = lstg.ResourceManager
    local a = manager.createPool("resource-pool-test-a")
    local b = manager.createPool("resource-pool-test-b")

    assert(a:getName() == "resource-pool-test-a")
    assert(manager.getPool("resource-pool-test-a") == a)
    assert(manager.getPool("resource-pool-test-missing") == nil)
    assert(not pcall(manager.createPool, ""))
    assert(not pcall(manager.createPool, "resource-pool-test-a"))
    local found_a = false
    for _, pool in ipairs(manager.getPools()) do
        found_a = found_a or pool == a
    end
    assert(found_a)

    local a_texture = a:createRenderTarget("shared", 8, 8, false)
    local b_texture = b:createRenderTarget("shared", 16, 16, false)
    local named_texture = lstg.CreateRenderTarget("resource-pool-test-a", "named", 4, 4, false)
    assert(named_texture:getWidth() == 4 and named_texture:getHeight() == 4)
    assert(a:contains(1, "named"))
    assert(not pcall(lstg.CreateRenderTarget, "resource-pool-test-missing", "missing", 4, 4, false))

    manager.setLookupOrder({ a, b, resources.pool })
    local width, height = lstg.GetTextureSize("shared")
    assert(width == 8 and height == 8)

    manager.setLookupOrder({ b, a, resources.pool })
    width, height = lstg.GetTextureSize("shared")
    assert(width == 16 and height == 16)

    assert(not pcall(manager.setLookupOrder, { b, b }))
    local order = manager.getLookupOrder()
    assert(order[1] == b and order[2] == a and order[3] == resources.pool)

    a:clear()
    assert(not a:contains(1, "shared"))
    assert(a_texture:getWidth() == 8)

    manager.destroyPool(b)
    assert(not b:isValid())
    assert(b_texture:getWidth() == 16)

    local replacement = manager.createPool("resource-pool-test-b")
    assert(replacement ~= b)
    manager.destroyPool(replacement)
    manager.destroyPool(a)

    local target = manager.createPool("resource-pool-test-reload")
    local old_texture = target:createRenderTarget("shared", 8, 8, false)
    local old_sprite = target:createSprite("sprite", old_texture, 0, 0, 8, 8)
    manager.setLookupOrder({ target, resources.pool })
    local staging = target:beginReload()
    assert(not pcall(staging.beginReload, staging))
    local new_texture = staging:createRenderTarget("shared", 16, 16, false)
    staging:createSprite("sprite", new_texture, 0, 0, 16, 16)
    assert(target:getTexture("shared"):getWidth() == 8)
    assert(staging:commitReload())
    assert(not staging:isValid() and not staging:commitReload())
    assert(manager.getPool("resource-pool-test-reload") == target)
    assert(manager.getLookupOrder()[1] == target)
    assert(target:getTexture("shared"):getWidth() == 16 and old_texture:getWidth() == 8)
    assert(target:getSprite("sprite") ~= old_sprite)

    staging = target:beginReload()
    assert(not pcall(staging.loadTexture, staging, "missing", "res/missing-reload.png", false))
    manager.destroyPool(staging)
    assert(target:getTexture("shared"):getWidth() == 16)

    staging = target:beginReload()
    target:clear()
    assert(not staging:commitReload())
    manager.destroyPool(staging)
    target:createRenderTarget("shared", 16, 16, false)
    local concurrent = target:beginReload()
    staging = target:beginReload()
    staging:createRenderTarget("shared", 24, 24, false)
    self.old_generation_job = target:loadTextureAsync("old-generation", "res/block.png", false)
    assert(staging:commitReload())
    assert(not concurrent:commitReload())
    manager.destroyPool(concurrent)
    assert(target:getTexture("shared"):getWidth() == 24)

    staging = target:beginReload()
    local cancelled = staging:loadTextureAsync("cancelled", "res/block.png", false)
    cancelled:cancel()
    assert(not staging:commitReload())
    manager.destroyPool(staging)

    self.reload_target = target
    self.reload_staging = target:beginReload()
    self.reload_job = self.reload_staging:loadTextureAsync("async-replacement", "res/block.png", false)
    assert(not self.reload_staging:commitReload())
    self.failed_staging = target:beginReload()
    self.failed_reload_job = self.failed_staging:loadTextureAsync("failed", "res/missing-reload.png", false)

    local doomed = manager.createPool("resource-pool-test-doomed")
    local abandoned = doomed:beginReload()
    manager.destroyPool(doomed)
    assert(not abandoned:isValid())

    local async_pool = manager.createPool("resource-pool-test-async")
    self.async_job = async_pool:loadTextureAsync("async", "res/block.png", false)
    manager.destroyPool(async_pool)
    self.async_replacement = manager.createPool("resource-pool-test-async")

    self.clear_pool = manager.createPool("resource-pool-test-clear")
    self.clear_job = self.clear_pool:loadTextureAsync("cleared", "res/block.png", false)
    self.clear_pool:clear()
    manager.setLookupOrder({ resources.pool })
end

function M:onDestroy()
    if self.reload_target and self.reload_target:isValid() then
        lstg.ResourceManager.destroyPool(self.reload_target)
    end
    if self.async_replacement and self.async_replacement:isValid() then
        lstg.ResourceManager.destroyPool(self.async_replacement)
    end
    if self.clear_pool and self.clear_pool:isValid() then
        lstg.ResourceManager.destroyPool(self.clear_pool)
    end
end

function M:onUpdate()
    if self.old_generation_job and self.old_generation_job:isDone() then
        assert(self.old_generation_job:status() == "cancelled")
        assert(not self.reload_target:hasTexture("old-generation"))
        self.old_generation_job = nil
    end
    if self.failed_reload_job and self.failed_reload_job:isDone() then
        assert(self.failed_reload_job:status() == "failed")
        assert(not self.failed_staging:commitReload())
        assert(not self.reload_target:hasTexture("failed"))
        lstg.ResourceManager.destroyPool(self.failed_staging)
        self.failed_staging = nil
        self.failed_reload_job = nil
    end
    if self.reload_job and self.reload_job:isDone() and not self.failed_reload_job then
        assert(self.reload_job:status() == "done")
        assert(self.reload_staging:commitReload())
        assert(self.reload_target:hasTexture("async-replacement"))
        assert(not self.reload_target:hasTexture("shared"))
        self.render_staging = self.reload_target:beginReload()
        self.render_staging:createRenderTarget("prepared", 4, 4, false)
        self.reload_staging = nil
        self.reload_job = nil
    end
    if self.async_job and self.async_job:isDone() then
        assert(self.async_job:status() == "cancelled")
        assert(not self.async_replacement:contains(1, "async"))
        lstg.ResourceManager.destroyPool(self.async_replacement)
        self.async_replacement = nil
        self.async_job = nil
    end
    if self.clear_job and self.clear_job:isDone() then
        assert(self.clear_job:status() == "cancelled")
        assert(not self.clear_pool:contains(1, "cleared"))
        lstg.ResourceManager.destroyPool(self.clear_pool)
        self.clear_pool = nil
        self.clear_job = nil
    end
end

function M:onRender()
    if self.render_staging then
        lstg.ResourceManager.setLookupOrder({ self.render_staging, resources.pool })
        lstg.PushRenderTarget("prepared")
        assert(not self.render_staging:commitReload())
        lstg.RenderClear(lstg.Color(255, 255, 255, 255))
        lstg.PopRenderTarget()
        assert(self.render_staging:commitReload())
        assert(self.reload_target:hasTexture("prepared"))
        self.render_staging = nil
        lstg.ResourceManager.setLookupOrder({ resources.pool })
    end
end

test.registerTest("resource pool manager", M)
