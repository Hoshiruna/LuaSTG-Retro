#include "ApplicationRestart.hpp"
#include "core/Logger.hpp"
#include "core/CommandLineArguments.hpp"
#include <atomic>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include "utf8.hpp"
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <crt_externs.h>
#else
extern char** environ;
#endif
#endif

namespace
{
    std::atomic_bool s_restart;

#ifdef _WIN32
    std::wstring executablePath()
    {
        std::vector<wchar_t> buffer(256);
        for(;;) {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if(length == 0) {
                throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "GetModuleFileNameW");
            }
            if(length < buffer.size()) {
                return { buffer.data(), length };
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    void appendArgument(std::wstring& command, const std::wstring_view argument)
    {
        if(!command.empty()) {
            command.push_back(L' ');
        }
        command.push_back(L'"');
        size_t backslashes{};
        for(const wchar_t character : argument) {
            if(character == L'\\') {
                ++backslashes;
                continue;
            }
            // Windows doubles backslashes before quotes and the closing quote.
            command.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
            command.push_back(character);
            backslashes = 0;
        }
        command.append(backslashes * 2, L'\\');
        command.push_back(L'"');
    }

    void restartProcess(std::vector<std::string> const& arguments)
    {
        const auto path = executablePath();
        std::wstring command;
        appendArgument(command, path);
        for(size_t i = 1; i < arguments.size(); ++i) {
            appendArgument(command, utf8::to_wstring(arguments[i]));
        }
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if(!CreateProcessW(path.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "CreateProcessW");
        }
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
#else
    std::string executablePath()
    {
#ifdef __APPLE__
        uint32_t size{};
        (void)_NSGetExecutablePath(nullptr, &size);
        std::vector<char> buffer(size);
        if(_NSGetExecutablePath(buffer.data(), &size) != 0) {
            throw std::runtime_error("_NSGetExecutablePath failed");
        }
        return buffer.data();
#else
        std::vector<char> buffer(256);
        for(;;) {
            const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
            if(length < 0) {
                throw std::system_error(errno, std::generic_category(), "readlink /proc/self/exe");
            }
            if(static_cast<size_t>(length) < buffer.size()) {
                return { buffer.data(), static_cast<size_t>(length) };
            }
            buffer.resize(buffer.size() * 2);
        }
#endif
    }

    void restartProcess(std::vector<std::string> arguments)
    {
        const auto path = executablePath();
        if(arguments.empty()) {
            arguments.push_back(path);
        } else {
            arguments.front() = path;
        }
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for(auto& argument : arguments) {
            argv.push_back(argument.data());
        }
        argv.push_back(nullptr);
#ifdef __APPLE__
        char** const environment = *_NSGetEnviron();
#else
        char** const environment = environ;
#endif
        pid_t child{};
        const int error = posix_spawn(&child, path.c_str(), nullptr, nullptr, argv.data(), environment);
        if(error != 0) {
            throw std::system_error(error, std::generic_category(), "posix_spawn");
        }
    }
#endif
}

namespace luastg
{
    void ApplicationRestart::disable()
    {
        s_restart.store(false);
    }

    void ApplicationRestart::enableWithCommandLineArguments(std::vector<std::string> const& args)
    {
        core::CommandLineArguments::assign(args);
        s_restart.store(true);
    }

    bool ApplicationRestart::hasRestart()
    {
        return s_restart.load();
    }

    void ApplicationRestart::start()
    {
        if(!s_restart.exchange(false)) {
            return;
        }
        try {
            restartProcess(core::CommandLineArguments::copy());
        } catch(const std::exception& error) {
            core::Logger::error("[luastg::ApplicationRestart] Restart failed: {}", error.what());
        }
    }
}
