#include "LuaBinding/modern/DynamicFont.hpp"

#include "AppFrame.h"
#include "LuaBinding/LuaWrapper.hpp"
#include "lua/plus.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <tuple>

namespace luastg::binding
{
    std::string_view const DynamicFont::class_name{ "lstg.DynamicFont" };

    namespace
    {
        int absoluteIndex(lua_State* const vm, int const index) noexcept
        {
            return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(vm) + index + 1;
        }

        bool getNumberField(lua_State* const vm, int const table_index, char const* const name, float& value)
        {
            lua_getfield(vm, table_index, name);
            auto const found = !lua_isnil(vm, -1);
            if(found) {
                value = static_cast<float>(luaL_checknumber(vm, -1));
            }
            lua_pop(vm, 1);
            return found;
        }

        std::optional<std::string_view> getStringField(lua_State* const vm, int const table_index, char const* const name)
        {
            lua_getfield(vm, table_index, name);
            if(lua_isnil(vm, -1)) {
                lua_pop(vm, 1);
                return std::nullopt;
            }
            size_t length{};
            auto const* const value = luaL_checklstring(vm, -1, &length);
            std::string_view const result(value, length);
            lua_pop(vm, 1);
            return result;
        }

        DynamicFontWrapMode parseWrap(lua_State* const vm, std::string_view const value)
        {
            if(value == "none") {
                return DynamicFontWrapMode::None;
            }
            if(value == "word") {
                return DynamicFontWrapMode::Word;
            }
            if(value == "character") {
                return DynamicFontWrapMode::Character;
            }
            luaL_error(vm, "invalid dynamic font wrap mode '%.*s'", static_cast<int>(value.size()), value.data());
            return DynamicFontWrapMode::None;
        }

        DynamicFontHorizontalAlignment parseHorizontalAlignment(lua_State* const vm, std::string_view const value)
        {
            if(value == "left") {
                return DynamicFontHorizontalAlignment::Left;
            }
            if(value == "center") {
                return DynamicFontHorizontalAlignment::Center;
            }
            if(value == "right") {
                return DynamicFontHorizontalAlignment::Right;
            }
            luaL_error(vm, "invalid horizontal alignment '%.*s'", static_cast<int>(value.size()), value.data());
            return DynamicFontHorizontalAlignment::Left;
        }

        DynamicFontVerticalAlignment parseVerticalAlignment(lua_State* const vm, std::string_view const value)
        {
            if(value == "top") {
                return DynamicFontVerticalAlignment::Top;
            }
            if(value == "middle") {
                return DynamicFontVerticalAlignment::Middle;
            }
            if(value == "bottom") {
                return DynamicFontVerticalAlignment::Bottom;
            }
            if(value == "baseline") {
                return DynamicFontVerticalAlignment::Baseline;
            }
            luaL_error(vm, "invalid vertical alignment '%.*s'", static_cast<int>(value.size()), value.data());
            return DynamicFontVerticalAlignment::Top;
        }

        void validateLayoutOptions(lua_State* const vm, int const argument, DynamicFontLayoutOptions const& options, bool const rect)
        {
            luaL_argcheck(vm, std::isfinite(options.scale.x) && options.scale.x > 0.0f, argument, "scaleX must be finite and greater than zero");
            luaL_argcheck(vm, std::isfinite(options.scale.y) && options.scale.y > 0.0f, argument, "scaleY must be finite and greater than zero");
            luaL_argcheck(vm, std::isfinite(options.line_spacing) && options.line_spacing > 0.0f, argument, "lineSpacing must be finite and greater than zero");
            luaL_argcheck(vm, std::isfinite(options.max_width) && options.max_width >= 0.0f, argument, "maxWidth must be finite and non-negative");
            luaL_argcheck(vm, rect || options.wrap == DynamicFontWrapMode::None || options.max_width > 0.0f, argument, "maxWidth is required when wrapping is enabled");
        }

        DynamicFontLayoutOptions parseLayoutOptions(lua_State* const vm, int const index, bool const rect = false)
        {
            DynamicFontLayoutOptions options;
            if(lua_isnoneornil(vm, index)) {
                return options;
            }
            luaL_checktype(vm, index, LUA_TTABLE);
            auto const table_index = absoluteIndex(vm, index);
            getNumberField(vm, table_index, "scaleX", options.scale.x);
            getNumberField(vm, table_index, "scaleY", options.scale.y);
            getNumberField(vm, table_index, "lineSpacing", options.line_spacing);
            getNumberField(vm, table_index, "maxWidth", options.max_width);
            if(auto const wrap = getStringField(vm, table_index, "wrap")) {
                options.wrap = parseWrap(vm, *wrap);
            }
            validateLayoutOptions(vm, index, options, rect);
            return options;
        }

        DynamicFontDrawOptions parseDrawOptions(lua_State* const vm, int const index, bool const rect)
        {
            DynamicFontDrawOptions options;
            if(lua_isnoneornil(vm, index)) {
                return options;
            }
            luaL_checktype(vm, index, LUA_TTABLE);
            auto const table_index = absoluteIndex(vm, index);
            options.layout = parseLayoutOptions(vm, table_index, rect);
            if(auto const alignment = getStringField(vm, table_index, "horizontalAlign")) {
                options.horizontal_alignment = parseHorizontalAlignment(vm, *alignment);
            }
            if(auto const alignment = getStringField(vm, table_index, "verticalAlign")) {
                options.vertical_alignment = parseVerticalAlignment(vm, *alignment);
            }
            getNumberField(vm, table_index, "rotation", options.rotation);
            getNumberField(vm, table_index, "z", options.z);
            getNumberField(vm, table_index, "outlineWidth", options.outline_width);
            auto shadow_x_set = getNumberField(vm, table_index, "shadowOffsetX", options.shadow_offset.x);
            auto shadow_y_set = getNumberField(vm, table_index, "shadowOffsetY", options.shadow_offset.y);
            options.shadow_enabled = shadow_x_set || shadow_y_set;

            lua_getfield(vm, table_index, "color");
            if(!lua_isnil(vm, -1)) {
                options.color = *Color::Cast(vm, -1);
            }
            lua_pop(vm, 1);
            lua_getfield(vm, table_index, "outlineColor");
            if(!lua_isnil(vm, -1)) {
                options.outline_color = *Color::Cast(vm, -1);
            }
            lua_pop(vm, 1);
            lua_getfield(vm, table_index, "shadowColor");
            if(!lua_isnil(vm, -1)) {
                options.shadow_color = *Color::Cast(vm, -1);
                options.shadow_enabled = true;
            }
            lua_pop(vm, 1);
            lua_getfield(vm, table_index, "blend");
            if(!lua_isnil(vm, -1)) {
                options.blend = TranslateBlendMode(vm, -1);
            }
            lua_pop(vm, 1);

            luaL_argcheck(vm, std::isfinite(options.rotation), index, "rotation must be finite");
            luaL_argcheck(vm, std::isfinite(options.z), index, "z must be finite");
            luaL_argcheck(vm, std::isfinite(options.outline_width) && options.outline_width >= 0.0f, index, "outlineWidth must be finite and non-negative");
            luaL_argcheck(vm, std::isfinite(options.shadow_offset.x) && std::isfinite(options.shadow_offset.y), index, "shadow offsets must be finite");
            if(rect && options.layout.wrap != DynamicFontWrapMode::None) {
                options.layout.max_width = 1.0f;
            }
            return options;
        }

        void setNumberField(lua_State* const vm, char const* const name, lua_Number const value)
        {
            lua_pushnumber(vm, value);
            lua_setfield(vm, -2, name);
        }

        void setBooleanField(lua_State* const vm, char const* const name, bool const value)
        {
            lua_pushboolean(vm, value);
            lua_setfield(vm, -2, name);
        }

        bool isUnicodeScalar(uint32_t const value) noexcept
        {
            return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
        }

        bool decodeSingleCodepoint(std::string_view const text, uint32_t& codepoint) noexcept
        {
            if(text.empty()) {
                return false;
            }
            auto const* const bytes = reinterpret_cast<uint8_t const*>(text.data());
            size_t length{};
            uint32_t minimum{};
            if(bytes[0] <= 0x7F) {
                length = 1;
                codepoint = bytes[0];
            } else if(bytes[0] >= 0xC2 && bytes[0] <= 0xDF) {
                length = 2;
                minimum = 0x80;
                codepoint = bytes[0] & 0x1F;
            } else if(bytes[0] >= 0xE0 && bytes[0] <= 0xEF) {
                length = 3;
                minimum = 0x800;
                codepoint = bytes[0] & 0x0F;
            } else if(bytes[0] >= 0xF0 && bytes[0] <= 0xF4) {
                length = 4;
                minimum = 0x10000;
                codepoint = bytes[0] & 0x07;
            } else {
                return false;
            }
            if(text.size() != length) {
                return false;
            }
            for(size_t i = 1; i < length; ++i) {
                if((bytes[i] & 0xC0) != 0x80) {
                    return false;
                }
                codepoint = (codepoint << 6) | (bytes[i] & 0x3F);
            }
            return codepoint >= minimum && isUnicodeScalar(codepoint);
        }

        uint32_t checkCodepoint(lua_State* const vm, int const index)
        {
            if(lua_type(vm, index) == LUA_TNUMBER) {
                auto const value = lua_tonumber(vm, index);
                luaL_argcheck(vm,
                    std::isfinite(value) && value >= 0 && value <= 0x10FFFF && std::floor(value) == value,
                    index,
                    "codepoint must be a Unicode scalar integer");
                auto const codepoint = static_cast<uint32_t>(value);
                luaL_argcheck(vm, isUnicodeScalar(codepoint), index, "codepoint must be a Unicode scalar integer");
                return codepoint;
            }
            size_t length{};
            auto const* const value = luaL_checklstring(vm, index, &length);
            uint32_t codepoint{};
            luaL_argcheck(vm,
                decodeSingleCodepoint(std::string_view(value, length), codepoint),
                index,
                "expected exactly one valid UTF-8 code point");
            return codepoint;
        }

        core::Graphics::IRenderer::SamplerState checkSamplerState(lua_State* const vm, int const index)
        {
            size_t length{};
            auto const* const value = luaL_checklstring(vm, index, &length);
            std::string_view const name(value, length);
            if(name == "point+wrap") {
                return core::Graphics::IRenderer::SamplerState::PointWrap;
            }
            if(name == "point+clamp") {
                return core::Graphics::IRenderer::SamplerState::PointClamp;
            }
            if(name == "linear+wrap") {
                return core::Graphics::IRenderer::SamplerState::LinearWrap;
            }
            if(name == "linear+clamp") {
                return core::Graphics::IRenderer::SamplerState::LinearClamp;
            }
            luaL_argerror(vm, index, "sampler must be 'point+wrap', 'point+clamp', 'linear+wrap', or 'linear+clamp'");
            return core::Graphics::IRenderer::SamplerState::LinearClamp;
        }

        int getResourceType(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            lua_pushinteger(vm, static_cast<lua_Integer>(self->data->GetType()));
            return 1;
        }

        int getResourceName(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            auto const name = self->data->GetResName();
            lua_pushlstring(vm, name.data(), name.size());
            return 1;
        }

        int cache(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            size_t length{};
            auto const* const text = luaL_checklstring(vm, 2, &length);
            lua_pushboolean(vm, LAPP.DynamicFontCache(self->data, std::string_view(text, length)));
            return 1;
        }

        int getMetrics(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            auto const glyph_manager = self->data->GetGlyphManager();
            lua_createtable(vm, 0, 3);
            setNumberField(vm, "lineHeight", glyph_manager->getLineHeight());
            setNumberField(vm, "ascender", glyph_manager->getAscender());
            setNumberField(vm, "descender", glyph_manager->getDescender());
            return 1;
        }

        int setSamplerState(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            auto const state = checkSamplerState(vm, 2);
            self->data->GetGlyphManager()->setSamplerState(
                LAPP.GetRenderer2D()->getKnownSamplerState(state));
            lua_pushvalue(vm, 1);
            return 1;
        }

        int getSamplerState(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            auto* const current = self->data->GetGlyphManager()->getSamplerState();
            auto* const renderer = LAPP.GetRenderer2D();
            struct NamedSampler
            {
                core::Graphics::IRenderer::SamplerState state;
                char const* name;
            };
            NamedSampler const samplers[] = {
                { core::Graphics::IRenderer::SamplerState::PointWrap, "point+wrap" },
                { core::Graphics::IRenderer::SamplerState::PointClamp, "point+clamp" },
                { core::Graphics::IRenderer::SamplerState::LinearWrap, "linear+wrap" },
                { core::Graphics::IRenderer::SamplerState::LinearClamp, "linear+clamp" },
            };
            for(auto const& sampler : samplers) {
                if(current == renderer->getKnownSamplerState(sampler.state)) {
                    lua_pushstring(vm, sampler.name);
                    return 1;
                }
            }
            return luaL_error(vm, "dynamic font has an unknown sampler state");
        }

        int getGlyph(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            auto const codepoint = checkCodepoint(vm, 2);
            core::Graphics::GlyphBitmap glyph;
            if(!self->data->GetGlyphManager()->getGlyphBitmap(codepoint, &glyph)) {
                lua_pushnil(vm);
                lua_pushliteral(vm, "failed to rasterize glyph");
                return 2;
            }
            lua_createtable(vm, 0, 14);
            setNumberField(vm, "codepoint", glyph.codepoint);
            setNumberField(vm, "sourceIndex", static_cast<lua_Number>(glyph.source_index + 1));
            setBooleanField(vm, "missing", glyph.missing);
            setNumberField(vm, "width", glyph.width);
            setNumberField(vm, "height", glyph.height);
            setNumberField(vm, "stride", glyph.stride);
            setNumberField(vm, "bearingX", glyph.info.position.x);
            setNumberField(vm, "bearingY", glyph.info.position.y);
            setNumberField(vm, "advanceX", glyph.info.advance.x);
            setNumberField(vm, "advanceY", glyph.info.advance.y);
            lua_pushliteral(vm, "a8");
            lua_setfield(vm, -2, "format");
            auto const* const pixels = glyph.pixels.empty() ? "" : reinterpret_cast<char const*>(glyph.pixels.data());
            lua_pushlstring(vm, pixels, glyph.pixels.size());
            lua_setfield(vm, -2, "pixels");
            return 1;
        }

        int measure(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            size_t length{};
            auto const* const text = luaL_checklstring(vm, 2, &length);
            auto const options = parseLayoutOptions(vm, 3);
            DynamicFontTextMetrics metrics;
            if(!LAPP.DynamicFontMeasure(self->data, std::string_view(text, length), options, metrics)) {
                return luaL_error(vm, "failed to measure dynamic font text; the text must be valid UTF-8");
            }
            lua_createtable(vm, 0, 9);
            setNumberField(vm, "inkLeft", metrics.ink_left);
            setNumberField(vm, "inkRight", metrics.ink_right);
            setNumberField(vm, "inkBottom", metrics.ink_bottom);
            setNumberField(vm, "inkTop", metrics.ink_top);
            setNumberField(vm, "layoutWidth", metrics.layout_width);
            setNumberField(vm, "layoutHeight", metrics.layout_height);
            setNumberField(vm, "advanceX", metrics.advance_x);
            setNumberField(vm, "advanceY", metrics.advance_y);
            lua_pushinteger(vm, static_cast<lua_Integer>(metrics.line_count));
            lua_setfield(vm, -2, "lineCount");
            return 1;
        }

        int draw(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            size_t length{};
            auto const* const text = luaL_checklstring(vm, 2, &length);
            auto const anchor = core::Vector2F(
                static_cast<float>(luaL_checknumber(vm, 3)),
                static_cast<float>(luaL_checknumber(vm, 4)));
            luaL_argcheck(vm, std::isfinite(anchor.x), 3, "x must be finite");
            luaL_argcheck(vm, std::isfinite(anchor.y), 4, "y must be finite");
            auto const options = parseDrawOptions(vm, 5, false);
            core::Vector2F end_position;
            if(!LAPP.DynamicFontDraw(self->data, std::string_view(text, length), anchor, options, end_position)) {
                return luaL_error(vm, "failed to draw dynamic font text; the text must be valid UTF-8 and drawing must occur inside a scene");
            }
            lua_pushnumber(vm, end_position.x);
            lua_pushnumber(vm, end_position.y);
            return 2;
        }

        int drawInRect(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            size_t length{};
            auto const* const text = luaL_checklstring(vm, 2, &length);
            auto const left = static_cast<float>(luaL_checknumber(vm, 3));
            auto const right = static_cast<float>(luaL_checknumber(vm, 4));
            auto const bottom = static_cast<float>(luaL_checknumber(vm, 5));
            auto const top = static_cast<float>(luaL_checknumber(vm, 6));
            luaL_argcheck(vm, std::isfinite(left), 3, "left must be finite");
            luaL_argcheck(vm, std::isfinite(right), 4, "right must be finite");
            luaL_argcheck(vm, std::isfinite(bottom), 5, "bottom must be finite");
            luaL_argcheck(vm, std::isfinite(top), 6, "top must be finite");
            luaL_argcheck(vm, left <= right, 4, "right must be greater than or equal to left");
            luaL_argcheck(vm, bottom <= top, 6, "top must be greater than or equal to bottom");
            auto const options = parseDrawOptions(vm, 7, true);
            luaL_argcheck(
                vm,
                options.layout.wrap == DynamicFontWrapMode::None || left < right,
                4,
                "wrapping requires a rectangle with positive width");
            core::Vector2F end_position;
            if(!LAPP.DynamicFontDrawInRect(
                   self->data,
                   std::string_view(text, length),
                   core::RectF(left, top, right, bottom),
                   options,
                   end_position)) {
                return luaL_error(vm, "failed to draw dynamic font text; the text must be valid UTF-8 and drawing must occur inside a scene");
            }
            lua_pushnumber(vm, end_position.x);
            lua_pushnumber(vm, end_position.y);
            return 2;
        }

        int collect(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            if(self->data) {
                self->data->release();
                self->data = nullptr;
            }
            return 0;
        }

        int tostring(lua_State* const vm)
        {
            std::ignore = DynamicFont::as(vm, 1);
            lua_pushlstring(vm, DynamicFont::class_name.data(), DynamicFont::class_name.size());
            return 1;
        }

        int equal(lua_State* const vm)
        {
            auto const self = DynamicFont::as(vm, 1);
            lua_pushboolean(vm, DynamicFont::is(vm, 2) && self->data == DynamicFont::as(vm, 2)->data);
            return 1;
        }
    }

    bool DynamicFont::is(lua_State* const vm, int const index)
    {
        return luaL_testudata(vm, index, class_name.data()) != nullptr;
    }

    DynamicFont* DynamicFont::as(lua_State* const vm, int const index)
    {
        return static_cast<DynamicFont*>(luaL_checkudata(vm, index, class_name.data()));
    }

    void DynamicFont::push(lua_State* const vm, IResourceFont* const resource)
    {
        auto const self = static_cast<DynamicFont*>(lua_newuserdata(vm, sizeof(DynamicFont)));
        self->data = resource;
        resource->retain();
        luaL_getmetatable(vm, class_name.data());
        lua_setmetatable(vm, -2);
    }

    void DynamicFont::registerClass(lua_State* const vm)
    {
        luaL_Reg const methods[] = {
            { "getResourceType", &getResourceType },
            { "getResourceName", &getResourceName },
            { "cache", &cache },
            { "getMetrics", &getMetrics },
            { "setSamplerState", &setSamplerState },
            { "getSamplerState", &getSamplerState },
            { "getGlyph", &getGlyph },
            { "measure", &measure },
            { "draw", &draw },
            { "drawInRect", &drawInRect },
            { nullptr, nullptr },
        };
        luaL_register(vm, class_name.data(), methods);
        luaL_newmetatable(vm, class_name.data());
        lua_pushcfunction(vm, &collect);
        lua_setfield(vm, -2, "__gc");
        lua_pushcfunction(vm, &tostring);
        lua_setfield(vm, -2, "__tostring");
        lua_pushcfunction(vm, &equal);
        lua_setfield(vm, -2, "__eq");
        lua_pushvalue(vm, -2);
        lua_setfield(vm, -2, "__index");
        lua_pop(vm, 2);
    }
}
