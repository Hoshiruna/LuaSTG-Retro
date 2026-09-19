geometry_fixture = {}

function geometry_fixture:camera(width, height)
    lstg.SetViewport(0, width, 0, height)
    lstg.SetScissorRect(0, width, 0, height)
    lstg.SetOrtho(0, width, height, 0)
    lstg.SetFog()
    lstg.SetZBufferEnable(0)
end

function geometry_fixture:quad(texture, x, y, width, height, color)
    lstg.Renderer.setTexture(texture)
    lstg.Renderer.setVertexColorBlendState(3)
    lstg.Renderer.setBlendState(2)
    lstg.Renderer.drawQuad(
        { x, y, 0.5, 0, 0, color }, { x + width, y, 0.5, 1, 0, color },
        { x + width, y + height, 0.5, 1, 1, color }, { x, y + height, 0.5, 0, 1, color })
end

function geometry_fixture:fillMesh(case)
    for vertex = 0, case.count - 1 do
        case.corner = case.order[vertex + 1]
        case.x = (case.corner % 2) * 48 + 8
        case.y = math.floor(case.corner / 2) * 48 + 8
        if self.phase == 2 and case.dynamic then case.x = case.x + 4 end
        case.mesh:setVertex(vertex, case.x, case.y, 0.5,
            case.corner % 2, math.floor(case.corner / 2),
            self.phase == 2 and case.dynamic and 0xff80ff80 or 0xffffffff)
    end
    if case.indexed then
        for index, vertex in ipairs(case.strip and { 0, 1, 2, 3 } or { 0, 1, 2, 2, 1, 3 }) do
            case.mesh:setIndex(index - 1, vertex)
        end
    end
    if not case.mesh:commit() then error("Geometry fixture: mesh commit failed") end
end

function geometry_fixture:onCreate()
    self.old_order = lstg.ResourceManager.getLookupOrder()
    self.pool = lstg.ResourceManager.createPool("geometry-fixture")
    lstg.ResourceManager.setLookupOrder({ self.pool })
    self.old_width, self.old_height = window.width, window.height
    window.width, window.height = 640, 480
    require("lstg.SwapChain").getMain():setSize(640, 480)
    require("lstg.Window").getMain():setSize(960, 720)
    self.pool:loadTexture("geometry:white", "res/geometry/white.png")
    self.pool:loadTexture("geometry:channels", "res/geometry/channels.png")
    self.pool:loadFX("geometry:copy", "res/geometry/passthrough.hlsl")
    lstg.SetTextureSamplerState("geometry:white", "point+clamp")
    lstg.SetTextureSamplerState("geometry:channels", "point+clamp")
    for _, name in ipairs({ "depth", "plain", "copy" }) do
        self.pool:createRenderTarget("geometry:" .. name, 64, 64, name == "depth")
        lstg.SetTextureSamplerState("geometry:" .. name, "point+clamp")
    end
    self.texture = require("lstg.Texture2D").createFromFile("res/geometry/channels.png", 1)
    self.meshes, self.phase, self.frames = {}, 1, 0
    for variant = 0, 15 do
        self.case = {
            indexed = variant % 4 < 2, strip = variant % 2 == 1,
            dynamic = variant < 8,
            order = { 0, 1, 2, 3 }, count = 4,
        }
        if not self.case.indexed and not self.case.strip then
            self.case.order, self.case.count = { 0, 1, 2, 2, 1, 3 }, 6
        end
        self.case.mesh = require("lstg.Mesh").create({
            vertex_count = self.case.count,
            index_count = self.case.indexed and (self.case.strip and 4 or 6) or 0,
            vertex_position_no_z = variant % 4 < 2,
            vertex_color_compression = variant % 8 < 4,
            vertex_index_compression = variant < 8,
            primitive_topology = self.case.strip and 5 or 4,
        })
        self:fillMesh(self.case)
        if not self.case.dynamic then self.case.mesh:setReadOnly() end
        self.case.renderer = require("lstg.MeshRenderer").create(self.case.mesh, self.texture)
        self.case.renderer:setLegacyBlendState(variant % 3 == 0 and "mul+add" or "mul+alpha")
        self.meshes[#self.meshes + 1] = self.case
    end
    self.case = nil
    self.legacy = lstg.MeshData(4, 6)
    self.legacy:setVertex(0, 8, 8, 0.5, 0, 0, 0xffffffff)
    self.legacy:setVertex(1, 56, 8, 0.5, 1, 0, 0xffffffff)
    self.legacy:setVertex(2, 56, 56, 0.5, 1, 1, 0xffffffff)
    self.legacy:setVertex(3, 8, 56, 0.5, 0, 1, 0xffffffff)
    for index, vertex in ipairs({ 0, 1, 2, 0, 2, 3 }) do self.legacy:setIndex(index - 1, vertex) end
    self.archive = lstg.LoadPack("res/geometry.zip")
    if not self.archive then error("Prepare geometry.zip with prepare_geometry.py") end
    self.models = {}
    for _, name in ipairs({ "textured", "solid", "vertex", "mask", "blend", "strip", "nonindexed", "index32", "index16", "culled", "lines", "points" }) do
        self.models[#self.models + 1] = { name = name, path = "res/geometry/" .. name .. ".gltf" }
    end
    self.models[13] = { name = "glb", path = "res/geometry/textured.glb" }
    self.models[14] = { name = "archive", path = "geometry-archive/textured.gltf" }
    self.models[15] = { name = "async", path = "res/geometry/textured.gltf", async = true }
    self.models[16] = { name = "archive-async", path = "geometry-archive/textured.glb", async = true }
    self.models[17] = { name = "line-strip", path = "res/geometry/line-strip.gltf" }
    for _, case in ipairs(self.models) do
        case.resource = "geometry:model:" .. case.name
        if case.async then case.job = self.pool:loadModelAsync(case.resource, case.path)
        else self.pool:loadModel(case.resource, case.path); case.ready = true end
    end
end

function geometry_fixture:onUpdate()
    self.ready = true
    for _, case in ipairs(self.models) do
        if case.job and case.job:isDone() then
            case.ok, case.message = case.job:read()
            if not case.ok then error("Model " .. case.name .. ": " .. tostring(case.message)) end
            case.ready = self.pool:contains(10, case.resource)
            case.job = nil
        end
        if not case.ready then self.ready = false end
    end
    if self.frames == 3 and self.phase == 1 then
        self.phase = 2
        for _, case in ipairs(self.meshes) do if case.dynamic then self:fillMesh(case) end end
    end
    if require("imgui").ImGui.Begin("Mesh and model comparison") then
        require("imgui").ImGui.Text("Rows 1-2: mesh layouts. Rows 3-4: model materials and loading.")
        require("imgui").ImGui.Text(self.frames >= 6 and "Captures requested: geometry-before.png, geometry-after.png, geometry-target.png, geometry-legacy.png" or "Waiting for models and two mesh states.")
        for _, case in ipairs(self.models) do require("imgui").ImGui.Text(case.name .. (case.ready and ": loaded" or ": pending")) end
    end
    require("imgui").ImGui.End()
end

function geometry_fixture:beginTile(depth)
    lstg.PushRenderTarget(depth and "geometry:depth" or "geometry:plain")
    self:camera(64, 64)
    lstg.RenderClear(lstg.Color(255, 16, 24, 32))
    lstg.ClearZBuffer(1)
    lstg.SetZBufferEnable(depth and 1 or 0)
end

function geometry_fixture:endTile(index, depth)
    lstg.PopRenderTarget()
    self:camera(640, 480)
    self:quad(depth and "geometry:depth" or "geometry:plain",
        index > 32 and 240 or 16 + ((index - 1) % 8) * 76,
        index > 32 and 380 or 16 + math.floor((index - 1) / 8) * 88, 64, 64, 0xffffffff)
end

function geometry_fixture:onRender()
    if not self.ready then return end
    self:camera(640, 480)
    lstg.RenderClear(lstg.Color(255, 32, 48, 64))
    for index, case in ipairs(self.meshes) do
        self:beginTile(index % 2 == 0)
        self:quad("geometry:white", 0, 0, 4, 4, 0xffff00ff)
        if index >= 10 and index <= 12 then
            lstg.SetPerspective(0, 0, -1, 0, 0, 0, 0, 1, 0, math.pi / 2, 1, 0.1, 100)
            lstg.SetOrtho(0, 64, 64, 0)
            if index == 10 then lstg.SetFog(10, 100, lstg.Color(255, 32, 64, 96))
            elseif index == 11 then lstg.SetFog(-1, 0.01, lstg.Color(255, 32, 64, 96))
            else lstg.SetFog(-2, 0.01, lstg.Color(255, 32, 64, 96)) end
        end
        if index % 4 == 0 then lstg.SetScissorRect(12, 52, 12, 52) end
        if index == 6 then case.renderer:setPosition(2, 2, 0); case.renderer:setScale(0.9, 0.9, 1) end
        case.renderer:draw()
        lstg.SetFog()
        lstg.SetScissorRect(0, 64, 0, 64)
        self:quad("geometry:white", 60, 60, 4, 4, 0xff00ffff)
        self:endTile(index, index % 2 == 0)
    end
    for index, case in ipairs(self.models) do
        self:beginTile(true)
        lstg.SetOrtho(-1, 1, 1, -1, -1, 1)
        lstg.RenderModel(case.resource, 0, 0, 0, 0, 0, 0, 1, 1, 1)
        self:camera(64, 64)
        self:quad("geometry:white", 60, 60, 4, 4, 0xff00ffff)
        self:endTile(index + 16, true)
    end
    self:beginTile(false)
    lstg.RenderMesh("geometry:channels", "mul+alpha", self.legacy)
    lstg.PopRenderTarget()
    if self.frames == 5 then lstg.SaveTexture("geometry:plain", "geometry-legacy.png") end
    lstg.PushRenderTarget("geometry:copy")
    self:camera(64, 64)
    lstg.PostEffect("geometry:plain", "geometry:copy", "one")
    lstg.PopRenderTarget()
    self:camera(640, 480)
    self:quad("geometry:copy", 16, 380, 64, 64, 0xffffffff)
    self:quad("geometry:white", 100, 400, 24, 24, 0xffff0000)
    self:quad("geometry:white", 132, 400, 24, 24, 0xff00ff00)
    self:quad("geometry:white", 164, 400, 24, 24, 0xff0000ff)
    if self.frames < 6 then
        self.frames = self.frames + 1
        if self.frames == 3 then lstg.Snapshot("geometry-before.png") end
        if self.frames == 6 then
            lstg.Snapshot("geometry-after.png")
            lstg.SaveTexture("geometry:copy", "geometry-target.png")
        end
    end
end

function geometry_fixture:onDestroy()
    for _, case in ipairs(self.models or {}) do if case.job then case.job:cancel() end end
    self.models, self.meshes, self.texture, self.legacy = nil, nil, nil, nil
    if self.old_order then lstg.ResourceManager.setLookupOrder(self.old_order) end
    if self.pool and self.pool:isValid() then lstg.ResourceManager.destroyPool(self.pool) end
    if self.archive then lstg.UnloadPack("res/geometry.zip") end
    if self.old_width then window:setSize(self.old_width, self.old_height) end
end

require("test").registerTest("test.graphics.GeometryParity", geometry_fixture, "Graphics: deterministic meshes and models")
