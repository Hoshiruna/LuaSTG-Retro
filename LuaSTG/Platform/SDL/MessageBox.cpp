#include "Platform/MessageBox.hpp"
#include <SDL3/SDL_messagebox.h>
#include <cstdio>
#include <string>

namespace Platform
{
    bool MessageBox::Warning(std::string_view title, std::string_view message)
    {
        return WarningFromWindow(title, message, nullptr);
    }

    void MessageBox::Error(std::string_view title, std::string_view message)
    {
        ErrorFromWindow(title, message, nullptr);
    }

    bool MessageBox::WarningFromWindow(std::string_view title, std::string_view message, void* window)
    {
        const std::string title_text(title);
        const std::string message_text(message);
        const SDL_MessageBoxButtonData buttons[]{
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "OK" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Cancel" },
        };
        SDL_MessageBoxData data{};
        data.flags = SDL_MESSAGEBOX_WARNING;
        data.window = static_cast<SDL_Window*>(window);
        data.title = title_text.c_str();
        data.message = message_text.c_str();
        data.numbuttons = 2;
        data.buttons = buttons;
        int selected{};
        return SDL_ShowMessageBox(&data, &selected) && selected == 1;
    }

    void MessageBox::ErrorFromWindow(std::string_view title, std::string_view message, void* window)
    {
        const std::string title_text(title);
        const std::string message_text(message);
        if(!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title_text.c_str(), message_text.c_str(), static_cast<SDL_Window*>(window))) {
            std::fprintf(stderr, "%s\n%s\n", title_text.c_str(), message_text.c_str());
        }
    }
}
