#include "GameResource/DynamicFontRenderer.hpp"
#include "core/SmartReference.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace luastg
{
    namespace
    {
        struct DecodedCharacter
        {
            char32_t codepoint{};
            size_t offset{};
            size_t length{};
            float advance{};
            bool whitespace{};
        };

        struct TextLine
        {
            std::string text;
            float width{};
        };

        struct TextLayout
        {
            std::vector<TextLine> lines;
            DynamicFontTextMetrics metrics;
        };

        class TextRendererStateGuard
        {
        public:
            explicit TextRendererStateGuard(core::Graphics::ITextRenderer* const renderer)
                : m_renderer(renderer)
                , m_glyph_manager(renderer->getGlyphManager())
                , m_scale(renderer->getScale())
                , m_color(renderer->getColor())
                , m_z(renderer->getZ())
            {
            }

            ~TextRendererStateGuard()
            {
                m_renderer->setGlyphManager(m_glyph_manager.get());
                m_renderer->setScale(m_scale);
                m_renderer->setColor(m_color);
                m_renderer->setZ(m_z);
            }

            TextRendererStateGuard(TextRendererStateGuard const&) = delete;
            TextRendererStateGuard& operator=(TextRendererStateGuard const&) = delete;

        private:
            core::Graphics::ITextRenderer* m_renderer;
            core::SmartReference<core::Graphics::IGlyphManager> m_glyph_manager;
            core::Vector2F m_scale;
            core::Color4B m_color;
            float m_z;
        };

        bool isContinuation(uint8_t const value) noexcept
        {
            return (value & 0xC0u) == 0x80u;
        }

        bool decodeCodepoint(std::string_view const text, size_t& offset, char32_t& codepoint, size_t& length) noexcept
        {
            auto const remaining = text.size() - offset;
            auto const* const bytes = reinterpret_cast<uint8_t const*>(text.data() + offset);
            auto const first = bytes[0];
            if(first <= 0x7Fu) {
                codepoint = first;
                length = 1;
            } else if(first >= 0xC2u && first <= 0xDFu && remaining >= 2 && isContinuation(bytes[1])) {
                codepoint = static_cast<char32_t>(((first & 0x1Fu) << 6) | (bytes[1] & 0x3Fu));
                length = 2;
            } else if(first >= 0xE0u && first <= 0xEFu && remaining >= 3 && isContinuation(bytes[1]) && isContinuation(bytes[2])
                && !(first == 0xE0u && bytes[1] < 0xA0u) && !(first == 0xEDu && bytes[1] >= 0xA0u)) {
                codepoint = static_cast<char32_t>(((first & 0x0Fu) << 12) | ((bytes[1] & 0x3Fu) << 6) | (bytes[2] & 0x3Fu));
                length = 3;
            } else if(first >= 0xF0u && first <= 0xF4u && remaining >= 4 && isContinuation(bytes[1]) && isContinuation(bytes[2]) && isContinuation(bytes[3])
                && !(first == 0xF0u && bytes[1] < 0x90u) && !(first == 0xF4u && bytes[1] > 0x8Fu)) {
                codepoint = static_cast<char32_t>(((first & 0x07u) << 18) | ((bytes[1] & 0x3Fu) << 12) | ((bytes[2] & 0x3Fu) << 6) | (bytes[3] & 0x3Fu));
                length = 4;
            } else {
                return false;
            }
            offset += length;
            return true;
        }

        bool isWhitespace(char32_t const codepoint) noexcept
        {
            switch(codepoint) {
                case U'\t':
                case U' ':
                case U'\u00A0':
                case U'\u1680':
                case U'\u2000':
                case U'\u2001':
                case U'\u2002':
                case U'\u2003':
                case U'\u2004':
                case U'\u2005':
                case U'\u2006':
                case U'\u2007':
                case U'\u2008':
                case U'\u2009':
                case U'\u200A':
                case U'\u202F':
                case U'\u205F':
                case U'\u3000':
                    return true;
                default:
                    return false;
            }
        }

        bool decodeText(
            core::Graphics::IGlyphManager* const glyph_manager,
            std::string_view const text,
            float const scale_x,
            std::vector<DecodedCharacter>& characters)
        {
            characters.clear();
            characters.reserve(text.size());
            size_t offset = 0;
            while(offset < text.size()) {
                auto const begin = offset;
                char32_t codepoint{};
                size_t length{};
                if(!decodeCodepoint(text, offset, codepoint, length)) {
                    return false;
                }
                float advance{};
                if(codepoint != U'\n' && codepoint != U'\r') {
                    core::Graphics::GlyphInfo glyph{};
                    if(glyph_manager->getGlyph(codepoint, &glyph, true)) {
                        advance = glyph.advance.x * scale_x;
                    }
                }
                characters.push_back({ codepoint, begin, length, advance, isWhitespace(codepoint) });
            }
            return true;
        }

        void appendCharacters(TextLine& line, std::string_view const source, std::vector<DecodedCharacter> const& characters, size_t const begin, size_t const end)
        {
            if(begin >= end) {
                return;
            }
            auto const byte_begin = characters[begin].offset;
            auto const byte_end = characters[end - 1].offset + characters[end - 1].length;
            line.text.append(source.substr(byte_begin, byte_end - byte_begin));
            for(size_t i = begin; i < end; ++i) {
                line.width += characters[i].advance;
            }
        }

        void pushLine(std::vector<TextLine>& lines, TextLine& line)
        {
            lines.emplace_back(std::move(line));
            line = {};
        }

        void appendCharacterWrapped(
            std::vector<TextLine>& lines,
            TextLine& line,
            std::string_view const source,
            std::vector<DecodedCharacter> const& characters,
            size_t begin,
            size_t const end,
            float const max_width)
        {
            while(begin < end) {
                auto const width = characters[begin].advance;
                if(!line.text.empty() && line.width + width > max_width) {
                    pushLine(lines, line);
                }
                appendCharacters(line, source, characters, begin, begin + 1);
                ++begin;
            }
        }

        void layoutParagraph(
            std::vector<TextLine>& lines,
            TextLine& line,
            std::string_view const source,
            std::vector<DecodedCharacter> const& characters,
            size_t const begin,
            size_t const end,
            DynamicFontLayoutOptions const& options)
        {
            if(options.wrap == DynamicFontWrapMode::None) {
                appendCharacters(line, source, characters, begin, end);
                return;
            }
            if(options.wrap == DynamicFontWrapMode::Character) {
                appendCharacterWrapped(lines, line, source, characters, begin, end, options.max_width);
                return;
            }

            size_t token_begin = begin;
            while(token_begin < end) {
                auto const whitespace = characters[token_begin].whitespace;
                size_t token_end = token_begin + 1;
                float token_width = characters[token_begin].advance;
                while(token_end < end && characters[token_end].whitespace == whitespace) {
                    token_width += characters[token_end].advance;
                    ++token_end;
                }

                if(line.text.empty() || line.width + token_width <= options.max_width) {
                    if(token_width <= options.max_width || whitespace) {
                        appendCharacters(line, source, characters, token_begin, token_end);
                    } else {
                        appendCharacterWrapped(lines, line, source, characters, token_begin, token_end, options.max_width);
                    }
                } else {
                    pushLine(lines, line);
                    if(!whitespace) {
                        if(token_width <= options.max_width) {
                            appendCharacters(line, source, characters, token_begin, token_end);
                        } else {
                            appendCharacterWrapped(lines, line, source, characters, token_begin, token_end, options.max_width);
                        }
                    }
                }
                token_begin = token_end;
            }
        }

        bool createLayout(
            core::Graphics::IGlyphManager* const glyph_manager,
            std::string_view const text,
            DynamicFontLayoutOptions const& options,
            TextLayout& output)
        {
            if(!glyph_manager
                || !std::isfinite(options.scale.x)
                || !std::isfinite(options.scale.y)
                || !std::isfinite(options.line_spacing)
                || options.line_spacing <= 0.0f
                || !std::isfinite(options.max_width)
                || (options.wrap != DynamicFontWrapMode::None && options.max_width < 0.0f)) {
                return false;
            }

            std::vector<DecodedCharacter> characters;
            if(!decodeText(glyph_manager, text, options.scale.x, characters)) {
                return false;
            }

            output = {};
            TextLine current_line;
            size_t paragraph_begin = 0;
            for(size_t i = 0; i <= characters.size(); ++i) {
                auto const is_end = i == characters.size();
                auto const is_newline = !is_end && (characters[i].codepoint == U'\n' || characters[i].codepoint == U'\r');
                if(!is_end && !is_newline) {
                    continue;
                }
                layoutParagraph(output.lines, current_line, text, characters, paragraph_begin, i, options);
                if(is_newline || is_end) {
                    pushLine(output.lines, current_line);
                }
                if(is_newline && characters[i].codepoint == U'\r' && i + 1 < characters.size() && characters[i + 1].codepoint == U'\n') {
                    ++i;
                }
                paragraph_begin = i + 1;
            }
            if(output.lines.empty()) {
                output.lines.emplace_back();
            }

            auto& metrics = output.metrics;
            metrics.line_count = output.lines.size();
            float const line_height = glyph_manager->getLineHeight() * options.scale.y;
            float const line_advance = line_height * options.line_spacing;
            metrics.layout_height = line_height + line_advance * static_cast<float>(output.lines.size() - 1);
            metrics.advance_y = -line_advance * static_cast<float>(output.lines.size() - 1);
            metrics.advance_x = output.lines.back().width;
            for(auto const& line : output.lines) {
                metrics.layout_width = std::max(metrics.layout_width, line.width);
            }

            bool has_ink = false;
            metrics.ink_left = std::numeric_limits<float>::max();
            metrics.ink_right = std::numeric_limits<float>::lowest();
            metrics.ink_bottom = std::numeric_limits<float>::max();
            metrics.ink_top = std::numeric_limits<float>::lowest();
            for(size_t line_index = 0; line_index < output.lines.size(); ++line_index) {
                auto const& line = output.lines[line_index];
                float pen_x{};
                float const baseline_y = -line_advance * static_cast<float>(line_index);
                size_t offset = 0;
                while(offset < line.text.size()) {
                    char32_t codepoint{};
                    size_t length{};
                    if(!decodeCodepoint(line.text, offset, codepoint, length)) {
                        return false;
                    }
                    core::Graphics::GlyphInfo glyph{};
                    if(glyph_manager->getGlyph(codepoint, &glyph, true)) {
                        auto const left = pen_x + glyph.position.x * options.scale.x;
                        auto const top = baseline_y + glyph.position.y * options.scale.y;
                        auto const right = left + glyph.size.x * options.scale.x;
                        auto const bottom = top - glyph.size.y * options.scale.y;
                        metrics.ink_left = std::min(metrics.ink_left, left);
                        metrics.ink_right = std::max(metrics.ink_right, right);
                        metrics.ink_bottom = std::min(metrics.ink_bottom, bottom);
                        metrics.ink_top = std::max(metrics.ink_top, top);
                        pen_x += glyph.advance.x * options.scale.x;
                        has_ink = true;
                    }
                }
            }
            if(!has_ink) {
                metrics.ink_left = 0.0f;
                metrics.ink_right = 0.0f;
                metrics.ink_bottom = 0.0f;
                metrics.ink_top = 0.0f;
            }
            return true;
        }

        core::Vector2F anchorOffset(
            DynamicFontHorizontalAlignment const horizontal,
            DynamicFontVerticalAlignment const vertical,
            float const line_width,
            DynamicFontTextMetrics const& metrics,
            float const ascender) noexcept
        {
            core::Vector2F result;
            switch(horizontal) {
                case DynamicFontHorizontalAlignment::Center: result.x = -line_width * 0.5f; break;
                case DynamicFontHorizontalAlignment::Right: result.x = -line_width; break;
                case DynamicFontHorizontalAlignment::Left: break;
            }
            switch(vertical) {
                case DynamicFontVerticalAlignment::Top: result.y = -ascender; break;
                case DynamicFontVerticalAlignment::Middle: result.y = metrics.layout_height * 0.5f - ascender; break;
                case DynamicFontVerticalAlignment::Bottom: result.y = metrics.layout_height - ascender; break;
                case DynamicFontVerticalAlignment::Baseline: break;
            }
            return result;
        }
    }

    DynamicFontRenderer::DynamicFontRenderer(core::Graphics::ITextRenderer* const renderer) noexcept
        : m_renderer(renderer)
    {
    }

    bool DynamicFontRenderer::cache(core::Graphics::IGlyphManager* const glyph_manager, std::string_view const text) const noexcept
    {
        try {
            std::vector<DecodedCharacter> characters;
            if(!glyph_manager || !decodeText(glyph_manager, text, 1.0f, characters)) {
                return false;
            }
            return glyph_manager->cacheString(text) && glyph_manager->flush();
        } catch(...) {
            return false;
        }
    }

    bool DynamicFontRenderer::measure(
        core::Graphics::IGlyphManager* const glyph_manager,
        std::string_view const text,
        DynamicFontLayoutOptions const& options,
        DynamicFontTextMetrics& metrics) const noexcept
    {
        try {
            TextLayout layout;
            if(!createLayout(glyph_manager, text, options, layout)) {
                return false;
            }
            metrics = layout.metrics;
            return true;
        } catch(...) {
            return false;
        }
    }

    bool DynamicFontRenderer::draw(
        core::Graphics::IGlyphManager* const glyph_manager,
        std::string_view const text,
        core::Vector2F const anchor,
        DynamicFontDrawOptions const& options,
        core::Vector2F& end_position) const noexcept
    {
        try {
            if(!m_renderer) {
                return false;
            }
            TextLayout layout;
            if(!createLayout(glyph_manager, text, options.layout, layout)) {
                return false;
            }

            TextRendererStateGuard const state_guard(m_renderer);
            m_renderer->setGlyphManager(glyph_manager);
            m_renderer->setScale(options.layout.scale);
            m_renderer->setZ(options.z);

            auto const cosine = std::cos(options.rotation);
            auto const sine = std::sin(options.rotation);
            core::Vector3F const right(cosine, sine, 0.0f);
            core::Vector3F const down(sine, -cosine, 0.0f);
            core::Vector3F const up(-sine, cosine, 0.0f);
            float const ascender = glyph_manager->getAscender() * options.layout.scale.y;
            float const line_advance = glyph_manager->getLineHeight() * options.layout.scale.y * options.layout.line_spacing;

            auto draw_pass = [&](core::Color4B const color, core::Vector2F const local_offset, core::Vector2F* const final_position) {
                m_renderer->setColor(color);
                core::Vector3F last_end(anchor.x, anchor.y, options.z);
                for(size_t i = 0; i < layout.lines.size(); ++i) {
                    auto const& line = layout.lines[i];
                    auto local = anchorOffset(options.horizontal_alignment, options.vertical_alignment, line.width, layout.metrics, ascender);
                    local.y -= line_advance * static_cast<float>(i);
                    local += local_offset;
                    auto const start = core::Vector3F(anchor.x, anchor.y, options.z) + right * local.x + up * local.y;
                    if(!m_renderer->drawTextInSpace(line.text, start, right, down, &last_end)) {
                        return false;
                    }
                }
                if(final_position) {
                    *final_position = core::Vector2F(last_end.x, last_end.y);
                }
                return true;
            };

            if(options.shadow_enabled && !draw_pass(options.shadow_color, options.shadow_offset, nullptr)) {
                return false;
            }
            if(options.outline_width > 0.0f) {
                auto const width = options.outline_width;
                auto const diagonal = width * 0.7071067811865475f;
                std::array const offsets{
                    core::Vector2F(-width, 0.0f), core::Vector2F(width, 0.0f), core::Vector2F(0.0f, -width), core::Vector2F(0.0f, width),
                    core::Vector2F(-diagonal, -diagonal), core::Vector2F(-diagonal, diagonal),
                    core::Vector2F(diagonal, -diagonal), core::Vector2F(diagonal, diagonal),
                };
                for(auto const offset : offsets) {
                    if(!draw_pass(options.outline_color, offset, nullptr)) {
                        return false;
                    }
                }
            }
            return draw_pass(options.color, {}, &end_position);
        } catch(...) {
            return false;
        }
    }

    bool DynamicFontRenderer::drawInRect(
        core::Graphics::IGlyphManager* const glyph_manager,
        std::string_view const text,
        core::RectF const& rect,
        DynamicFontDrawOptions const& options,
        core::Vector2F& end_position) const noexcept
    {
        if(rect.a.x > rect.b.x || rect.b.y > rect.a.y) {
            return false;
        }
        auto adjusted = options;
        if(adjusted.layout.wrap != DynamicFontWrapMode::None) {
            adjusted.layout.max_width = rect.b.x - rect.a.x;
        }
        core::Vector2F anchor;
        switch(adjusted.horizontal_alignment) {
            case DynamicFontHorizontalAlignment::Left: anchor.x = rect.a.x; break;
            case DynamicFontHorizontalAlignment::Center: anchor.x = (rect.a.x + rect.b.x) * 0.5f; break;
            case DynamicFontHorizontalAlignment::Right: anchor.x = rect.b.x; break;
        }
        switch(adjusted.vertical_alignment) {
            case DynamicFontVerticalAlignment::Top:
            case DynamicFontVerticalAlignment::Baseline: anchor.y = rect.a.y; break;
            case DynamicFontVerticalAlignment::Middle: anchor.y = (rect.a.y + rect.b.y) * 0.5f; break;
            case DynamicFontVerticalAlignment::Bottom: anchor.y = rect.b.y; break;
        }
        return draw(glyph_manager, text, anchor, adjusted, end_position);
    }
}
