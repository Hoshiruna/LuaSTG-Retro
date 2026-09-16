#include "core/Clipboard.hpp"
#include <SDL3/SDL_clipboard.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_stdinc.h>
#include <memory>

namespace core
{
    bool Clipboard::hasText()
    {
        return SDL_HasClipboardText();
    }

    bool Clipboard::setText(std::string_view const& text)
    {
        // An empty string leaves the clipboard alone.
        if(text.empty()) {
            return true;
        }
        const std::string terminated(text);
        return SDL_SetClipboardText(terminated.c_str());
    }

    bool Clipboard::getText(std::string& buffer)
    {
        buffer.clear();
        if(!hasText()) {
            return false;
        }
        SDL_ClearError();
        const std::unique_ptr<char, decltype(&SDL_free)> text(SDL_GetClipboardText(), SDL_free);
        if(!text || (text.get()[0] == '\0' && SDL_GetError()[0] != '\0')) {
            return false;
        }
        buffer.assign(text.get());
        return true;
    }
}
