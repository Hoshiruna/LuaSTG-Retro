local M = {}

M.pool = lstg.ResourceManager.createPool("test")
lstg.ResourceManager.setLookupOrder({ M.pool })

function M.loadTexture(...)
    return M.pool:loadTexture(...)
end

function M.loadVideo(...)
    return M.pool:loadVideo(...)
end

function M.createSprite(name, texture_name, ...)
    return M.pool:createSprite(name, M.pool:getTexture(texture_name), ...)
end

function M.loadParticle(name, definition, sprite_name, ...)
    return M.pool:loadParticle(name, definition, M.pool:getSprite(sprite_name), ...)
end

function M.loadSound(...)
    return M.pool:loadSound(...)
end

function M.loadMusic(...)
    return M.pool:loadMusic(...)
end

function M.loadSpriteFont(...)
    return M.pool:loadSpriteFont(...)
end

function M.loadTTF(...)
    return M.pool:loadTTF(...)
end

function M.loadFX(...)
    return M.pool:loadFX(...)
end

function M.loadModel(...)
    return M.pool:loadModel(...)
end

function M.createRenderTarget(...)
    return M.pool:createRenderTarget(...)
end

function M.removeResource(_, resource_type, name)
    return M.pool:removeByName(resource_type, name)
end

return M
