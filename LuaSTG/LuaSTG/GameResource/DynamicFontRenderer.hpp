#pragma once

#include "Core/Graphics/Font.hpp"
#include "GameResource/ResourceBase.hpp"

#include <cstddef>
#include <string_view>

namespace luastg
{
    enum class DynamicFontHorizontalAlignment
    {
        Left,
        Center,
        Right,
    };

    enum class DynamicFontVerticalAlignment
    {
        Top,
        Middle,
        Bottom,
        Baseline,
    };

    enum class DynamicFontWrapMode
    {
        None,
        Word,
        Character,
    };

    struct DynamicFontLayoutOptions
    {
        core::Vector2F scale{ 1.0f, 1.0f };
        float line_spacing{ 1.0f };
        float max_width{};
        DynamicFontWrapMode wrap{ DynamicFontWrapMode::None };
    };

    struct DynamicFontDrawOptions
    {
        DynamicFontLayoutOptions layout;
        DynamicFontHorizontalAlignment horizontal_alignment{ DynamicFontHorizontalAlignment::Left };
        DynamicFontVerticalAlignment vertical_alignment{ DynamicFontVerticalAlignment::Top };
        float rotation{};
        float z{ 0.5f };
        BlendMode blend{ BlendMode::MulAlpha };
        core::Color4B color{ 0xFFFFFFFFu };
        float outline_width{};
        core::Color4B outline_color{ 0xFF000000u };
        bool shadow_enabled{};
        core::Vector2F shadow_offset{};
        core::Color4B shadow_color{ 0xFF000000u };
    };

    struct DynamicFontTextMetrics
    {
        float ink_left{};
        float ink_right{};
        float ink_bottom{};
        float ink_top{};
        float layout_width{};
        float layout_height{};
        float advance_x{};
        float advance_y{};
        size_t line_count{ 1 };
    };

    class DynamicFontRenderer
    {
    public:
        explicit DynamicFontRenderer(core::Graphics::ITextRenderer* renderer) noexcept;

        bool cache(core::Graphics::IGlyphManager* glyph_manager, std::string_view text) const noexcept;
        bool measure(
            core::Graphics::IGlyphManager* glyph_manager,
            std::string_view text,
            DynamicFontLayoutOptions const& options,
            DynamicFontTextMetrics& metrics) const noexcept;
        bool draw(
            core::Graphics::IGlyphManager* glyph_manager,
            std::string_view text,
            core::Vector2F anchor,
            DynamicFontDrawOptions const& options,
            core::Vector2F& end_position) const noexcept;
        bool drawInRect(
            core::Graphics::IGlyphManager* glyph_manager,
            std::string_view text,
            core::RectF const& rect,
            DynamicFontDrawOptions const& options,
            core::Vector2F& end_position) const noexcept;

    private:
        core::Graphics::ITextRenderer* m_renderer{};
    };
}
