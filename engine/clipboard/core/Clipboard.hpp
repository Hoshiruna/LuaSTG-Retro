#pragma once
#include <string_view>
#include <string>

namespace core
{
    class Clipboard
    {
    public:
        // Clipboard operations run on the SDL application thread.
        static bool hasText();
        static bool setText(std::string_view const& text);
        static bool getText(std::string& buffer);
    };
}
