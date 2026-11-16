#pragma once

#include <string_view>

namespace Platform
{
    bool openConsole(std::string_view title);
    void closeConsole(bool preserve);
}
