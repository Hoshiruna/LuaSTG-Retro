#include "core/Configuration.hpp"
#include <array>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

namespace
{
    std::filesystem::path homeDirectory()
    {
        if(const char* home = std::getenv("HOME"); home && *home) {
            return home;
        }
        if(const auto* account = getpwuid(getuid()); account && account->pw_dir) {
            return account->pw_dir;
        }
        return {};
    }

    std::filesystem::path dataDirectory()
    {
        if(const char* data = std::getenv("XDG_DATA_HOME"); data && *data) {
            const std::filesystem::path path(data);
            if(path.is_absolute()) {
                return path;
            }
        }
        const auto home = homeDirectory();
        return home.empty() ? std::filesystem::path{} : home / ".local/share";
    }
}

namespace core
{
    bool ConfigurationLoader::replaceAllPredefinedVariables(std::string_view const& input, std::string& output)
    {
        output.assign(input);
        constexpr std::array<std::string_view, 7> marks{
            "${user:roaming_app_data}", "${user:local_app_data}", "${user:local_app_data_low}",
            "${user:saved_games}", "${AppData}", "${LocalAppData}", "${Temp}",
        };
        for(const auto mark : marks) {
            if(output.find(mark) == std::string::npos) {
                continue;
            }
            std::error_code error;
            auto directory = mark == "${Temp}" ? std::filesystem::temp_directory_path(error) : dataDirectory();
            if(error || directory.empty()) {
                return false;
            }
            if(mark == "${user:saved_games}") {
                directory /= "games";
            }
            const auto replacement = directory.generic_string();
            size_t position{};
            while((position = output.find(mark, position)) != std::string::npos) {
                output.replace(position, mark.size(), replacement);
                position += replacement.size();
            }
        }
        return true;
    }

    bool ConfigurationLoader::resolvePathWithPredefinedVariables(std::string_view const& input, std::filesystem::path& output, bool const create_directories)
    {
        std::string path;
        if(!replaceAllPredefinedVariables(input, path)) {
            return false;
        }
        output = std::filesystem::u8path(path).lexically_normal();
        if(create_directories && !output.empty()) {
            std::error_code error;
            std::filesystem::create_directories(output, error);
            return !error;
        }
        return true;
    }

    bool ConfigurationLoader::resolveFilePathWithPredefinedVariables(std::string_view const& input, std::filesystem::path& output, bool const create_parent_directories)
    {
        if(!resolvePathWithPredefinedVariables(input, output)) {
            return false;
        }
        if(create_parent_directories && output.has_parent_path()) {
            std::error_code error;
            std::filesystem::create_directories(output.parent_path(), error);
            return !error;
        }
        return true;
    }
}
