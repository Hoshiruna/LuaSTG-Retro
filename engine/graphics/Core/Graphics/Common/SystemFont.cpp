#include "Core/Graphics/Common/SystemFont.hpp"
#include "core/Logger.hpp"

namespace core::Graphics::Common
{
    bool findSystemFont(std::string_view const name, std::string& u8_path)
    {
        u8_path.clear();
        Logger::error("[core] System font lookup is unavailable on this platform for '{}'. Load a font file by path.", name);
        return false;
    }
}
