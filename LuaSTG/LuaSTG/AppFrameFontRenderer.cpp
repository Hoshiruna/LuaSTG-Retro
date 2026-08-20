#include "AppFrame.h"
#include "utf8.hpp"

namespace luastg
{
    // luastg plus interface

    bool AppFrame::RenderText(IResourceFont* p, wchar_t* strBuf, core::RectF rect, core::Vector2F scale, FontAlignHorizontal halign, FontAlignVertical valign, bool bWordBreak) noexcept
    {
        using namespace core;
        using namespace core::Graphics;

        IGlyphManager* pGlyphManager = p->GetGlyphManager();

        // 准备渲染字体
        m_pTextRenderer->setGlyphManager(pGlyphManager);
        m_pTextRenderer->setScale(scale);

        // 设置混合和颜色
        updateGraph2DBlendMode(p->GetBlendMode());
        m_pTextRenderer->setColor(p->GetBlendColor());

        // 第一次遍历计算要渲染多少行
        const wchar_t* pText = strBuf;
        int iLineCount = 1;
        float fLineWidth = 0.f;
        while(*pText) {
            bool bNewLine = false;
            if(*pText == L'\n')
                bNewLine = true;
            else {
                GlyphInfo tGlyphInfo{};
                if(pGlyphManager->getGlyph(*pText, &tGlyphInfo, true)) {
                    float adv = tGlyphInfo.advance.x * scale.x;
                    if(bWordBreak && fLineWidth + adv > std::abs(rect.a.x - rect.b.x)) // 截断模式
                    {
                        if(pText == strBuf || *(pText - 1) == L'\n') {
                            ++pText; // 防止一个字符都不渲染导致死循环
                            if(*pText == L'\0')
                                break;
                        }
                        bNewLine = true;
                    } else
                        fLineWidth += adv;
                }
            }
            if(bNewLine) {
                ++iLineCount;
                fLineWidth = 0.f;
            }
            if(*pText != L'\0')
                ++pText;
        }

        // 计算起笔位置
        float fTotalLineHeight = pGlyphManager->getLineHeight() * iLineCount * scale.y;
        core::Vector2F vRenderPos;
        switch(valign) {
            case FontAlignVertical::Bottom:
                vRenderPos.y = rect.b.y + fTotalLineHeight;
                break;
            case FontAlignVertical::Middle:
                vRenderPos.y = rect.a.y - std::abs(rect.a.y - rect.b.y) / 2.f + fTotalLineHeight / 2.f;
                break;
            case FontAlignVertical::Top:
            default:
                vRenderPos.y = rect.a.y;
                break;
        }
        vRenderPos.x = rect.a.x;
        vRenderPos.y -= pGlyphManager->getAscender() * scale.y;

        // 逐行渲染文字
        wchar_t* pScanner = strBuf;
        wchar_t c = 0;
        bool bEOS = false;
        fLineWidth = 0.f;
        pText = pScanner;
        while(!bEOS) {
            // 寻找断句位置，换行、EOF、或者行溢出
            while(*pScanner != L'\0' && *pScanner != '\n') {
                GlyphInfo tGlyphInfo{};
                if(pGlyphManager->getGlyph(*pScanner, &tGlyphInfo, true)) {
                    float adv = tGlyphInfo.advance.x * scale.x;

                    // 检查当前字符渲染后会不会导致行溢出
                    if(bWordBreak && fLineWidth + adv > std::abs(rect.a.x - rect.b.x)) {
                        if(pScanner == pText) // 防止一个字符都不渲染导致死循环
                            ++pScanner;
                        break;
                    }
                    fLineWidth += adv;
                }
                ++pScanner;
            }

            // 在断句位置写入\0
            c = *pScanner;
            if(c == L'\0')
                bEOS = true;
            else
                *pScanner = L'\0';

            // 渲染从pText~pScanner的文字
            std::string u8_str(utf8::to_string(pText));
            Vector2F ignore_;
            switch(halign) {
                case FontAlignHorizontal::Right:
                    m_pTextRenderer->drawText(u8_str, Vector2F(vRenderPos.x + std::abs(rect.a.x - rect.b.x) - fLineWidth, vRenderPos.y), &ignore_);
                    break;
                case FontAlignHorizontal::Center:
                    m_pTextRenderer->drawText(u8_str, Vector2F(vRenderPos.x + std::abs(rect.a.x - rect.b.x) / 2.f - fLineWidth / 2.f, vRenderPos.y), &ignore_);
                    break;
                case FontAlignHorizontal::Left:
                default:
                    m_pTextRenderer->drawText(u8_str, vRenderPos, &ignore_);
                    break;
            }

            // 恢复断句处字符
            *pScanner = c;
            fLineWidth = 0.f;
            if(c == L'\n')
                pText = ++pScanner;
            else
                pText = pScanner;

            // 移动y轴
            vRenderPos.y -= pGlyphManager->getLineHeight() * scale.y;
        }

        return true;
    }

    core::Vector2F AppFrame::CalcuTextSize(IResourceFont* p, const wchar_t* strBuf, core::Vector2F scale) noexcept
    {
        using namespace core;
        using namespace core::Graphics;

        IGlyphManager* pGlyphManager = p->GetGlyphManager();

        int iLineCount = 1;
        float fLineWidth = 0.f;
        float fMaxLineWidth = 0.f;
        while(*strBuf) {
            if(*strBuf == L'\n') {
                ++iLineCount;
                fMaxLineWidth = std::max(fMaxLineWidth, fLineWidth);
                fLineWidth = 0.f;
            } else {
                GlyphInfo tGlyphInfo{};
                if(pGlyphManager->getGlyph(*strBuf, &tGlyphInfo, true))
                    fLineWidth += tGlyphInfo.advance.x * scale.x;
            }
            ++strBuf;
        }
        fMaxLineWidth = std::max(fMaxLineWidth, fLineWidth);

        return core::Vector2F(fMaxLineWidth, iLineCount * pGlyphManager->getLineHeight() * scale.y);
    }

    bool AppFrame::RenderText(const char* name, const char* str, float x, float y, float scale, FontAlignHorizontal halign, FontAlignVertical valign) noexcept
    {
        core::SmartReference<IResourceFont> p = m_ResourceMgr.FindSpriteFont(name);
        if(!p) {
            spdlog::error("[luastg] RenderText: 找不到字体资源'{}'", name);
            return false;
        }

        // 编码转换
        std::wstring s_TempStringBuf;
        try {
            s_TempStringBuf = utf8::to_wstring(str);
        } catch(const std::bad_alloc&) {
            spdlog::error("[luastg] RenderText: 内存不足");
            return false;
        }

        // 计算渲染位置
        core::Vector2F tSize = CalcuTextSize(p.get(), s_TempStringBuf.c_str(), core::Vector2F(scale, scale));
        switch(halign) {
            case FontAlignHorizontal::Right:
                x -= tSize.x;
                break;
            case FontAlignHorizontal::Center:
                x -= tSize.x / 2.f;
                break;
            case FontAlignHorizontal::Left:
            default:
                break;
        }
        switch(valign) {
            case FontAlignVertical::Bottom:
                y += tSize.y;
                break;
            case FontAlignVertical::Middle:
                y += tSize.y / 2.f;
                break;
            case FontAlignVertical::Top:
            default:
                break;
        }

        return RenderText(
            p.get(),
            const_cast<wchar_t*>(s_TempStringBuf.data()),
            core::RectF(x, y, x + tSize.x, y - tSize.y),
            core::Vector2F(scale, scale),
            halign,
            valign,
            false);
    }

    bool AppFrame::DynamicFontCache(IResourceFont* const font, std::string_view const text) noexcept
    {
        return font && DynamicFontRenderer(m_pTextRenderer.get()).cache(font->GetGlyphManager(), text);
    }

    bool AppFrame::DynamicFontMeasure(
        IResourceFont* const font,
        std::string_view const text,
        DynamicFontLayoutOptions const& options,
        DynamicFontTextMetrics& metrics) noexcept
    {
        return font && DynamicFontRenderer(m_pTextRenderer.get()).measure(font->GetGlyphManager(), text, options, metrics);
    }

    bool AppFrame::DynamicFontDraw(
        IResourceFont* const font,
        std::string_view const text,
        core::Vector2F const anchor,
        DynamicFontDrawOptions const& options,
        core::Vector2F& end_position) noexcept
    {
        if(!font) {
            return false;
        }
        updateGraph2DBlendMode(options.blend);
        return DynamicFontRenderer(m_pTextRenderer.get()).draw(font->GetGlyphManager(), text, anchor, options, end_position);
    }

    bool AppFrame::DynamicFontDrawInRect(
        IResourceFont* const font,
        std::string_view const text,
        core::RectF const& rect,
        DynamicFontDrawOptions const& options,
        core::Vector2F& end_position) noexcept
    {
        if(!font) {
            return false;
        }
        updateGraph2DBlendMode(options.blend);
        return DynamicFontRenderer(m_pTextRenderer.get()).drawInRect(font->GetGlyphManager(), text, rect, options, end_position);
    }

};
