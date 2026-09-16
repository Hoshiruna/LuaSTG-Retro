#include "core/ShellIntegration.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <format>
#include <memory>
#include <string>
#include <string_view>

#ifdef _WIN32
#include "utf8.hpp"
#include <windows.h>
#include <shlobj_core.h>
#else
#include <cerrno>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char** environ;
#endif
#endif

namespace
{
    struct Messages
    {
        const char* file_title;
        const char* file_message;
        const char* directory_title;
        const char* directory_message;
        const char* url_title;
        const char* url_message;
        const char* accept;
        const char* cancel;
    };

    constexpr Messages english{
        "Open file",
        "About to open file:\n{}",
        "Open folder",
        "About to open folder:\n{}",
        "Open website",
        "About to open website:\n{}",
        "OK",
        "Cancel"
    };
    constexpr Messages simplified_chinese{
        "打开文件",
        "即将打开文件：\n{}",
        "打开文件夹",
        "即将打开文件夹：\n{}",
        "打开网站",
        "即将打开网站：\n{}",
        "确定",
        "取消"
    };
    constexpr Messages traditional_chinese{
        "開啟檔案",
        "即將開啟檔案：\n{}",
        "開啟資料夾",
        "即將開啟資料夾：\n{}",
        "開啟網站",
        "即將開啟網站：\n{}",
        "確定",
        "取消"
    };
    constexpr Messages japanese{
        "ファイルを開く",
        "ファイルを開く予定です：\n{}",
        "フォルダーを開く",
        "フォルダーを開く予定です：\n{}",
        "Web サイトを開く",
        "Web サイトを開く予定です：\n{}",
        "OK",
        "キャンセル"
    };
    constexpr Messages korean{
        "파일 열기",
        "파일을 열 예정입니다:\n{}",
        "폴더 열기",
        "폴더를 열 예정입니다:\n{}",
        "웹 사이트 열기",
        "웹 사이트를 열 예정입니다:\n{}",
        "확인",
        "취소"
    };

    const Messages& messages()
    {
        const std::unique_ptr<SDL_Locale*, decltype(&SDL_free)> locales(SDL_GetPreferredLocales(nullptr), SDL_free);
        if(locales) {
            for(auto** locale = locales.get(); *locale != nullptr; ++locale) {
                const std::string_view language((*locale)->language);
                const std::string_view country((*locale)->country != nullptr ? (*locale)->country : "");
                if(language == "en") {
                    return english;
                }
                if(language == "zh") {
                    return country == "TW" || country == "HK" || country == "MO" ? traditional_chinese : simplified_chinese;
                }
                if(language == "ja") {
                    return japanese;
                }
                if(language == "ko") {
                    return korean;
                }
            }
        }
        return english;
    }

    bool confirm(const Messages& text, const char* title, const char* pattern, const std::string_view path)
    {
        const auto message = std::vformat(pattern, std::make_format_args(path));
        const SDL_MessageBoxButtonData buttons[]{
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, text.cancel },
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, text.accept },
        };
        SDL_MessageBoxData dialog{};
        dialog.flags = SDL_MESSAGEBOX_WARNING;
        dialog.title = title;
        dialog.message = message.c_str();
        dialog.numbuttons = 2;
        dialog.buttons = buttons;
        int selected{};
        if(!SDL_ShowMessageBox(&dialog, &selected)) {
            core::Logger::error("[core::ShellIntegration] Confirmation failed: {}", SDL_GetError());
            return false;
        }
        return selected == 1;
    }

    bool openPath(const std::string& path, const bool file)
    {
#ifdef _WIN32
        const auto wide = utf8::to_wstring(path);
        if(file) {
            OPENASINFO info{};
            info.pcszFile = wide.c_str();
            info.oaifInFlags = OAIF_EXEC | OAIF_HIDE_REGISTRATION;
            const auto result = SHOpenWithDialog(nullptr, &info);
            if(FAILED(result)) {
                core::Logger::error("[core::ShellIntegration] SHOpenWithDialog failed: HRESULT 0x{:08x}", static_cast<uint32_t>(result));
                return false;
            }
            return true;
        }
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
        info.lpVerb = L"open";
        info.lpFile = wide.c_str();
        info.nShow = SW_SHOWNORMAL;
        if(!ShellExecuteExW(&info)) {
            core::Logger::error("[core::ShellIntegration] ShellExecuteExW failed: {}", GetLastError());
            return false;
        }
        return true;
#else
        (void)file;
#ifdef __APPLE__
        char program[] = "/usr/bin/open";
        char** const environment = *_NSGetEnviron();
#else
        char program[] = "xdg-open";
        char** const environment = environ;
#endif
        // Pass the absolute path as one argument; shell metacharacters stay literal.
        std::string argument(path);
        char* arguments[]{ program, argument.data(), nullptr };
        pid_t process{};
        const int error = posix_spawnp(&process, program, nullptr, nullptr, arguments, environment);
        if(error != 0) {
            core::Logger::error("[core::ShellIntegration] {} failed: {}", program, std::strerror(error));
            return false;
        }
        int status{};
        pid_t result{};
        do {
            result = waitpid(process, &status, 0);
        } while(result < 0 && errno == EINTR);
        if(result < 0) {
            core::Logger::error("[core::ShellIntegration] waitpid failed: {}", std::strerror(errno));
            return false;
        }
        if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            core::Logger::error("[core::ShellIntegration] {} could not open '{}' (status {})", program, path, status);
            return false;
        }
        return true;
#endif
    }

    bool openFileSystemPath(const std::string_view path, const bool file)
    {
        try {
            const std::u8string_view input(reinterpret_cast<const char8_t*>(path.data()), path.size());
            const auto absolute = std::filesystem::absolute(std::filesystem::path(input)).lexically_normal();
            if(file ? !std::filesystem::is_regular_file(absolute) : !std::filesystem::is_directory(absolute)) {
                core::Logger::error("[core::ShellIntegration] {} does not exist: {}", file ? "File" : "Directory", path);
                return false;
            }
            const auto encoded = absolute.u8string();
            const std::string name(reinterpret_cast<const char*>(encoded.data()), encoded.size());
            const auto& text = messages();
            if(!confirm(text, file ? text.file_title : text.directory_title, file ? text.file_message : text.directory_message, name)) {
                return false;
            }
            return openPath(name, file);
        } catch(const std::exception& error) {
            core::Logger::error("[core::ShellIntegration] Open path failed: {}", error.what());
            return false;
        }
    }
}

namespace core
{
    bool ShellIntegration::openFile(std::string_view const path)
    {
        return openFileSystemPath(path, true);
    }

    bool ShellIntegration::openDirectory(std::string_view const path)
    {
        return openFileSystemPath(path, false);
    }

    bool ShellIntegration::openUrl(std::string_view const url)
    {
        try {
            const auto& text = messages();
            if(!confirm(text, text.url_title, text.url_message, url)) {
                return false;
            }
            const std::string terminated(url);
            if(!SDL_OpenURL(terminated.c_str())) {
                Logger::error("[core::ShellIntegration] SDL_OpenURL failed: {}", SDL_GetError());
                return false;
            }
            return true;
        } catch(const std::exception& error) {
            Logger::error("[core::ShellIntegration] Open URL failed: {}", error.what());
            return false;
        }
    }
}
