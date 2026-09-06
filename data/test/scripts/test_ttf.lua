local test = require("test")
local resources = require("resource_pool")

---@class test.Module.DynamicFont : test.Base
local M = {}

local FONT_PATH = "res/model/syst_heavy.otf"

local function descriptor(pixel_height, options)
    local result = {
        pixelWidth = 0,
        pixelHeight = pixel_height,
        sources = {
            { path = FONT_PATH, faceIndex = 0 },
        },
    }
    for key, value in pairs(options or {}) do
        result[key] = value
    end
    return result
end

local function glyph_alphas(glyph)
    return { string.byte(glyph.pixels, 1, #glyph.pixels) }
end

function M:onCreate()
    local pool = resources.pool

    assert(type(rawget(lstg, "DynamicFont")) == "table")

    assert(not pcall(pool.loadDynamicFont, pool, "dynamic-font:malformed", {
        pixelHeight = 26,
        sources = {},
    }))
    assert(not pcall(pool.loadDynamicFont, pool, "dynamic-font:invalid-face", {
        pixelHeight = 26,
        sources = { { path = FONT_PATH, faceIndex = -1 } },
    }))
    for field, value in pairs({
        rasterMode = "lcd",
        hinting = "sometimes",
        hintingTarget = "strong",
        sampler = "anisotropic+wrap",
        alphaThreshold = 1.5,
    }) do
        local invalid = descriptor(26, { [field] = value })
        assert(not pcall(pool.loadDynamicFont, pool, "dynamic-font:invalid-" .. field, invalid))
    end
    assert(not pcall(pool.loadDynamicFont, pool, "dynamic-font:negative-threshold",
        descriptor(26, { alphaThreshold = -1 })))
    assert(not pcall(pool.loadDynamicFont, pool, "dynamic-font:large-threshold",
        descriptor(26, { alphaThreshold = 256 })))

    local missing, missing_error = lstg.LoadDynamicFont(pool, "dynamic-font:missing", {
        pixelHeight = 26,
        sources = { { path = "res/does-not-exist.ttf" } },
    })
    assert(missing == nil and type(missing_error) == "string")
    assert(not pool:hasDynamicFont("dynamic-font:missing"))

    local atomic, atomic_error = pool:loadDynamicFont("dynamic-font:atomic", {
        pixelHeight = 26,
        sources = {
            { path = FONT_PATH },
            { path = "res/does-not-exist.ttf" },
        },
    })
    assert(atomic == nil and type(atomic_error) == "string")
    assert(not pool:hasDynamicFont("dynamic-font:atomic"))

    local font, load_error = lstg.LoadDynamicFont("test", "dynamic-font:visual", descriptor(32))
    self.font = assert(font, load_error)
    assert(self.font:getResourceType() == 8)
    assert(self.font:getResourceName() == "dynamic-font:visual")
    assert(pool:hasDynamicFont("dynamic-font:visual"))
    assert(pool:getDynamicFont("dynamic-font:visual") == self.font)

    local duplicate, duplicate_error = pool:loadDynamicFont("dynamic-font:visual", descriptor(32))
    assert(duplicate == nil and type(duplicate_error) == "string")

    local font_metrics = self.font:getMetrics()
    assert(font_metrics.lineHeight > 0)
    assert(font_metrics.ascender > 0)
    assert(font_metrics.descender <= 0)
    assert(self.font:getSamplerState() == "linear+clamp")
    for _, sampler in ipairs({
        "point+wrap",
        "point+clamp",
        "linear+wrap",
        "linear+clamp",
    }) do
        assert(self.font:setSamplerState(sampler) == self.font)
        assert(self.font:getSamplerState() == sampler)
    end
    assert(not pcall(self.font.setSamplerState, self.font, "nearest"))

    local glyph_by_string = assert(self.font:getGlyph("A"))
    local glyph_by_integer = assert(self.font:getGlyph(0x41))
    assert(glyph_by_string.codepoint == 0x41)
    assert(glyph_by_string.sourceIndex == 1 and not glyph_by_string.missing)
    assert(glyph_by_string.format == "a8")
    assert(glyph_by_string.stride == glyph_by_string.width)
    assert(#glyph_by_string.pixels == glyph_by_string.stride * glyph_by_string.height)
    assert(glyph_by_string.pixels == glyph_by_integer.pixels)
    local pixel_snapshot = glyph_by_string.pixels
    self.font:cache("atlas changes do not alter snapshots")
    assert(pixel_snapshot == glyph_by_string.pixels)

    local space = assert(self.font:getGlyph(" "))
    assert(space.width == 0 and space.height == 0 and space.pixels == "")
    local supplementary_glyph = assert(self.font:getGlyph("\240\159\152\128"))
    assert(supplementary_glyph.codepoint == 0x1F600)
    assert(not pcall(self.font.getGlyph, self.font, ""))
    assert(not pcall(self.font.getGlyph, self.font, "AB"))
    assert(not pcall(self.font.getGlyph, self.font, "\255"))
    assert(not pcall(self.font.getGlyph, self.font, -1))
    assert(not pcall(self.font.getGlyph, self.font, 0xD800))
    assert(not pcall(self.font.getGlyph, self.font, 0x110000))

    self.fallback_font = assert(pool:loadDynamicFont("dynamic-font:fallback", {
        pixelHeight = 32,
        sources = {
            { path = FONT_PATH },
            { path = FONT_PATH },
        },
    }))
    local missing_glyph = assert(self.fallback_font:getGlyph(0x10FFFF))
    assert(missing_glyph.sourceIndex == 2 and missing_glyph.missing)

    self.grayscale_font = assert(pool:loadDynamicFont("dynamic-font:grayscale",
        descriptor(40, { rasterMode = "grayscale" })))
    self.monochrome_font = assert(pool:loadDynamicFont("dynamic-font:monochrome",
        descriptor(40, { rasterMode = "monochrome", sampler = "point+clamp" })))
    self.threshold_font = assert(pool:loadDynamicFont("dynamic-font:threshold",
        descriptor(40, { alphaThreshold = 128 })))
    self.light_hint_font = assert(pool:loadDynamicFont("dynamic-font:light-hint",
        descriptor(40, { hintingTarget = "light" })))
    local grayscale_glyph = assert(self.grayscale_font:getGlyph("A"))
    local monochrome_glyph = assert(self.monochrome_font:getGlyph("A"))
    local threshold_glyph = assert(self.threshold_font:getGlyph("A"))
    local grayscale_alphas = glyph_alphas(grayscale_glyph)
    local monochrome_alphas = glyph_alphas(monochrome_glyph)
    local threshold_alphas = glyph_alphas(threshold_glyph)
    assert(#grayscale_alphas == #threshold_alphas)
    assert(self.monochrome_font:getSamplerState() == "point+clamp")
    local has_intermediate_alpha = false
    for _, alpha in ipairs(grayscale_alphas) do
        has_intermediate_alpha = has_intermediate_alpha or (alpha > 0 and alpha < 255)
    end
    assert(has_intermediate_alpha)
    for _, alpha in ipairs(monochrome_alphas) do
        assert(alpha == 0 or alpha == 255)
    end
    for index, alpha in ipairs(grayscale_alphas) do
        assert(threshold_alphas[index] == (alpha < 128 and 0 or alpha))
    end

    for _, hinting in ipairs({ "native", "auto", "none" }) do
        for _, target in ipairs({ "normal", "light", "monochrome" }) do
            local name = "dynamic-font:hinting-" .. hinting .. "-" .. target
            local hint_font, hint_error = pool:loadDynamicFont(name,
                descriptor(20, { hinting = hinting, hintingTarget = target }))
            assert(hint_font, hint_error)
            assert(hint_font:getGlyph("A"))
            pool:remove(hint_font)
        end
    end

    local empty = self.font:measure("")
    assert(empty.lineCount == 1 and empty.layoutWidth == 0)
    assert(math.abs(empty.layoutHeight - font_metrics.lineHeight) < 0.001)

    local multiple_lines = self.font:measure("first\n第二行\n")
    assert(multiple_lines.lineCount == 3)
    local supplementary = self.font:measure("supplementary: \240\159\152\128")
    assert(supplementary.lineCount == 1)

    local sample = self.font:measure("你好朋友")
    assert(sample.layoutWidth > 0 and sample.advanceX > 0)
    local character_wrap = self.font:measure("你好朋友", {
        wrap = "character",
        maxWidth = sample.layoutWidth * 0.55,
    })
    assert(character_wrap.lineCount >= 2)

    local hello = self.font:measure("Hello")
    local word_wrap = self.font:measure("Hello dynamic font", {
        wrap = "word",
        maxWidth = hello.layoutWidth + 1,
    })
    assert(word_wrap.lineCount >= 2)
    local oversized_word = self.font:measure("oversized", {
        wrap = "word",
        maxWidth = self.font:measure("ov").layoutWidth,
    })
    assert(oversized_word.lineCount > 1)

    local scaled = self.font:measure("scale", { scaleX = 2, scaleY = 0.5 })
    local unscaled = self.font:measure("scale")
    assert(math.abs(scaled.layoutWidth - unscaled.layoutWidth * 2) < 0.01)
    assert(math.abs(scaled.layoutHeight - unscaled.layoutHeight * 0.5) < 0.01)
    local spaced = self.font:measure("a\nb", { lineSpacing = 1.5 })
    assert(spaced.layoutHeight > self.font:measure("a\nb").layoutHeight)

    assert(self.font:cache("你好朋友"))
    assert(not self.font:cache("\255"))
    assert(not pcall(self.font.measure, self.font, "\255"))

    pool:remove(self.font)
    assert(not pool:hasDynamicFont("dynamic-font:visual"))
    assert(self.font:getMetrics().lineHeight == font_metrics.lineHeight)

    self.async_job = pool:loadDynamicFontAsync("dynamic-font:async", descriptor(24, {
        rasterMode = "monochrome",
        hinting = "auto",
        hintingTarget = "monochrome",
        alphaThreshold = 128,
        sampler = "point+wrap",
    }))
    self.failed_async_job = pool:loadDynamicFontAsync("dynamic-font:async-missing", {
        pixelHeight = 24,
        sources = { { path = "res/does-not-exist.ttf" } },
    })
end

function M:onDestroy()
    if self.async_job and not self.async_job:isDone() then
        self.async_job:cancel()
    end
    if self.failed_async_job and not self.failed_async_job:isDone() then
        self.failed_async_job:cancel()
    end
    local async_font = resources.pool:getDynamicFont("dynamic-font:async")
    if async_font then
        resources.pool:remove(async_font)
    end
    if self.fallback_font then
        resources.pool:remove(self.fallback_font)
    end
    if self.grayscale_font then
        resources.pool:remove(self.grayscale_font)
    end
    if self.monochrome_font then
        resources.pool:remove(self.monochrome_font)
    end
    if self.threshold_font then
        resources.pool:remove(self.threshold_font)
    end
    if self.light_hint_font then
        resources.pool:remove(self.light_hint_font)
    end
end

function M:onUpdate()
    if self.async_job and self.async_job:isDone() then
        local ok, async_error = self.async_job:read()
        assert(ok, async_error)
        local async_font = assert(resources.pool:getDynamicFont("dynamic-font:async"))
        assert(async_font:getMetrics().lineHeight > 0)
        assert(async_font:getSamplerState() == "point+wrap")
        for _, alpha in ipairs(glyph_alphas(assert(async_font:getGlyph("A")))) do
            assert(alpha == 0 or alpha == 255)
        end
        resources.pool:remove(async_font)
        self.async_job = nil
    end
    if self.failed_async_job and self.failed_async_job:isDone() then
        local result, async_error = self.failed_async_job:read()
        assert(result == nil and type(async_error) == "string")
        assert(self.failed_async_job:status() == "failed")
        assert(not resources.pool:hasDynamicFont("dynamic-font:async-missing"))
        self.failed_async_job = nil
    end
end

function M:onRender()
    window:applyCameraV()
    local font = self.font
    local center_x = window.width / 2

    local samples = {
        { self.grayscale_font, "grayscale / linear / normal target", lstg.Color(255, 235, 245, 255) },
        { self.monochrome_font, "monochrome / point / mono target", lstg.Color(255, 255, 235, 190) },
        { self.light_hint_font, "grayscale / light target", lstg.Color(255, 230, 255, 205) },
        { self.threshold_font, "threshold 128", lstg.Color(255, 220, 255, 220) },
    }
    for index, sample_font in ipairs(samples) do
        sample_font[1]:draw(sample_font[2], 40, window.height - 25 - (index - 1) * 35, {
            color = sample_font[3],
            verticalAlign = "top",
        })
    end

    local final_x, final_y = font:draw("lstg.DynamicFont", center_x, window.height - 70, {
        color = lstg.Color(255, 255, 255, 255),
        blend = "",
        z = 0.5,
        horizontalAlign = "center",
        verticalAlign = "top",
        shadowOffsetX = 4,
        shadowOffsetY = -4,
        shadowColor = lstg.Color(160, 0, 0, 0),
        outlineWidth = 2,
        outlineColor = lstg.Color(255, 32, 32, 32),
    })
    assert(type(final_x) == "number" and type(final_y) == "number")

    font:drawInRect(
        "Word wrapping preserves explicit\nnewlines and wraps oversized words character by character.",
        80, center_x - 30, 180, window.height - 180,
        {
            color = lstg.Color(255, 225, 240, 255),
            horizontalAlign = "left",
            verticalAlign = "top",
            lineSpacing = 1.2,
            wrap = "word",
        })

    font:drawInRect(
        "字符换行：你好朋友。矩形只负责布局，不会自动裁剪。",
        center_x + 30, window.width - 80, 180, window.height - 180,
        {
            color = lstg.Color(255, 255, 235, 190),
            horizontalAlign = "center",
            verticalAlign = "middle",
            scaleX = 1.15,
            scaleY = 0.85,
            rotation = math.rad(-4),
            wrap = "character",
            outlineWidth = 1,
            outlineColor = lstg.Color(255, 32, 32, 32),
        })

    local pen_text = "final pen"
    local pen_advance = font:measure(pen_text).advanceX
    local pen_x, pen_y = font:draw(pen_text, 80, 100, {
        color = lstg.Color(255, 220, 255, 220),
        verticalAlign = "baseline",
    })
    assert(math.abs(pen_x - (80 + pen_advance)) < 0.01)
    assert(math.abs(pen_y - 100) < 0.01)
end

test.registerTest("test.Module.DynamicFont", M)
