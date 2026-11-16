#pragma once

#include <string>
#include <string_view>

namespace core::Graphics::Common
{
    bool findSystemFont(std::string_view name, std::string& u8_path);
}
