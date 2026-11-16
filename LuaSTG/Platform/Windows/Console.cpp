#include "Platform/Console.hpp"
#include "Platform/CleanWindows.hpp"
#include "utf8.hpp"
#include <SDL3/SDL_timer.h>

namespace
{
    bool g_alloc_console{ false };
}

namespace Platform
{
    bool openConsole(std::string_view const title)
    {
        if(!AllocConsole()) {
            return false;
        }
        g_alloc_console = true;
        HWND const window = GetConsoleWindow();
        HMENU const menu = GetSystemMenu(window, FALSE);
        RemoveMenu(menu, SC_CLOSE, MF_BYCOMMAND);
        const auto wide_title = utf8::to_wstring(title);
        SetConsoleTitleW(wide_title.c_str());
        ShowWindow(window, SW_MAXIMIZE);
        SetConsoleOutputCP(CP_UTF8);
        return true;
    }
    void closeConsole(bool const preserve)
    {
        if(g_alloc_console) {
            if(preserve) {
                constexpr std::wstring_view exit_message(L"按 ESC 关闭引擎日志窗口 | Press ESC to close the engine log window\n");
                WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), exit_message.data(), static_cast<DWORD>(exit_message.length()), nullptr, nullptr);
                while((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0x8000) {
                    SDL_Delay(1);
                }
            }
            FreeConsole();
        }
    }
}
