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
    if self.async_replacement and self.async_replacement:isValid() then
        lstg.ResourceManager.destroyPool(self.async_replacement)
    end
    if self.clear_pool and self.clear_pool:isValid() then
        lstg.ResourceManager.destroyPool(self.clear_pool)
    end
end

function M:onUpdate()
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
end

test.registerTest("resource pool manager", M)
