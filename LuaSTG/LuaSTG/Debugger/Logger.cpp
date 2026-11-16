#include "luastg_config_generated.h"
#include "Debugger/Logger.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "Platform/Console.hpp"
#ifdef _WIN32
#include "spdlog/sinks/msvc_sink.h"
#include "Platform/HResultChecker.hpp"
#include "win32/base.hpp"
#endif
#include "core/Configuration.hpp"
#include <chrono>
#include <filesystem>
#include <format>
#ifdef __APPLE__
#include <ctime>
#include <stdexcept>
#endif

namespace
{
    std::filesystem::path rolling_file_root;
    std::string generateRollingFileName()
    {
#ifdef __APPLE__
        const auto now = std::time(nullptr);
        std::tm local_time{};
        char filename[64]{};
        if(!localtime_r(&now, &local_time) || std::strftime(filename, sizeof(filename), "engine-%Y%m%d-%H%M%S.log", &local_time) == 0) {
            throw std::runtime_error("Cannot format the rolling log filename");
        }
        return filename;
#else
        auto const now = std::chrono::zoned_time{
            std::chrono::current_zone(),
            std::chrono::utc_clock::to_sys(std::chrono::utc_clock::now()),
        };
        auto const local_date_time = now.get_local_time();
        auto const local_date_time_point = std::chrono::floor<std::chrono::days>(local_date_time);
        auto const local_date = std::chrono::year_month_day{ local_date_time_point };
        auto const local_time = std::chrono::hh_mm_ss{ local_date_time - local_date_time_point };
        return std::format(
            "engine-{:04}{:02}{:02}-{:02}{:02}{:02}.log",
            static_cast<int>(local_date.year()),
            static_cast<unsigned int>(local_date.month()),
            static_cast<unsigned int>(local_date.day()),
            local_time.hours().count(),
            local_time.minutes().count(),
            local_time.seconds().count());
#endif
    }
    spdlog::level::level_enum mapLevel(core::ConfigurationLoader::Logging::Level const level)
    {
        switch(level) {
            default: // NOLINT(clang-diagnostic-covered-switch-default)
                return (spdlog::level::trace);
            case core::ConfigurationLoader::Logging::Level::debug:
                return (spdlog::level::debug);
            case core::ConfigurationLoader::Logging::Level::info:
                return (spdlog::level::info);
            case core::ConfigurationLoader::Logging::Level::warn:
                return (spdlog::level::warn);
            case core::ConfigurationLoader::Logging::Level::error:
                return (spdlog::level::err);
            case core::ConfigurationLoader::Logging::Level::fatal:
                return (spdlog::level::critical);
        }
    }
#ifdef _WIN32
    void writeMessage(std::string_view const message)
    {
        spdlog::error("[luastg] {}", message);
    }
#endif
}

namespace luastg
{
    void Logger::create()
    {
        auto const& config = core::ConfigurationLoader::getInstance().getLogging();

        std::vector<spdlog::sink_ptr> sinks;

#if defined(_WIN32) && !defined(NDEBUG)
        if(auto const& logging_debugger = config.getDebugger(); logging_debugger.isEnable()) {
            auto sink = std::make_shared<spdlog::sinks::windebug_sink_mt>();
            sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
            sink->set_level(mapLevel(logging_debugger.getThreshold()));
            sinks.emplace_back(sink);
        }
#endif

#ifdef LUASTG_LOGGING_CONSOLE_WINDOW_ALLOWED
        if(auto const& logging_console = config.getConsole(); logging_console.isEnable() && Platform::openConsole(LUASTG_INFO)) {
            auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            sink->set_pattern("%^[%Y-%m-%d %H:%M:%S] [%L]%$ %v");
            sink->set_level(mapLevel(logging_console.getThreshold()));
            sinks.emplace_back(sink);
        }
#endif

        if(auto const& logging_file = config.getFile(); logging_file.isEnable()) {
            std::filesystem::path file_path;
            if(logging_file.hasPath()) {
                core::ConfigurationLoader::resolveFilePathWithPredefinedVariables(logging_file.getPath(), file_path, true);
            } else {
                file_path = std::filesystem::u8path(LUASTG_LOGGING_DEFAULT_FILE_PATH);
            }
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path.native(), true);
            sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
            sink->set_level(mapLevel(logging_file.getThreshold()));
            sinks.emplace_back(sink);
        }

        if(auto const& logging_rolling_file = config.getRollingFile(); logging_rolling_file.isEnable()) {
            std::filesystem::path path;
            if(logging_rolling_file.hasPath()) {
                std::filesystem::path directory;
                core::ConfigurationLoader::resolvePathWithPredefinedVariables(logging_rolling_file.getPath(), directory, true);
                path = directory / generateRollingFileName();
                rolling_file_root = directory;
            } else {
                std::filesystem::path directory("logs/"); // TODO: MAGIC VALUE
                path = directory / generateRollingFileName();
                rolling_file_root = directory;
            }
            auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.native(), true);
            sink->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
            sink->set_level(mapLevel(logging_rolling_file.getThreshold()));
            sinks.emplace_back(sink);
        }

        auto const logger = std::make_shared<spdlog::logger>("luastg", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        //logger->set_pattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
        //logger->set_pattern("%^[%Y-%m-%d %H:%M:%S] [%L]%$ %v");
        //logger->set_pattern("%^[%Y-%m-%d %H:%M:%S] [%L] [%n]%$ %v");
        logger->flush_on(spdlog::level::info);

        spdlog::set_default_logger(logger);

#ifdef _WIN32
        Platform::HResultChecker::SetPrintCallback(&writeMessage);
        win32::set_logger_writer(writeMessage);
#endif
    }
    void Logger::destroy()
    {
        auto const& config = core::ConfigurationLoader::getInstance().getLogging();

        if(auto const logger = spdlog::get("luastg")) {
            logger->flush();
        }

#ifdef _WIN32
        Platform::HResultChecker::SetPrintCallback();
#endif

        spdlog::drop_all();
        spdlog::shutdown();

#ifdef LUASTG_LOGGING_CONSOLE_WINDOW_ALLOWED
        Platform::closeConsole(config.getConsole().isPreserve());
#endif

        if(auto const& logging_rolling_file = config.getRollingFile(); logging_rolling_file.isEnable()) {
            std::error_code ec;
            std::vector<std::filesystem::path> logs;
            for(auto& it : std::filesystem::directory_iterator(rolling_file_root, ec)) {
                if(it.is_regular_file(ec) && it.path().extension() == u8".log") {
                    logs.push_back(it.path());
                }
            }
            if(logs.size() > logging_rolling_file.getMaxHistory()) {
                std::sort(logs.begin(), logs.end(), [](std::filesystem::path const& a, std::filesystem::path const& b) -> bool {
                    return a.filename().generic_u8string() < b.filename().generic_u8string();
                });
                size_t const remove_count = logs.size() - logging_rolling_file.getMaxHistory();
                for(size_t idx = 0; idx < remove_count; idx += 1) {
                    std::filesystem::remove(logs[idx], ec);
                }
            }
        }
    }
};
