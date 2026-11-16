#include "Platform/Application.hpp"
#include "Platform/CleanWindows.hpp"
#include "Platform/ProcessorInfo.hpp"
#include "Platform/WindowsVersion.hpp"
#include "spdlog/spdlog.h"

#include <cstdio>
#include <iterator>
#include <string>

namespace
{
    static std::string bytesCountToString(DWORDLONG const size)
    {
        char buffer[64]{};
        int count{};
        if(size < 1024llu) {
            count = std::snprintf(buffer, std::size(buffer), "%u B", static_cast<unsigned int>(size));
        } else if(size < 1024llu * 1024llu) {
            count = std::snprintf(buffer, std::size(buffer), "%.2f KiB", static_cast<double>(size) / 1024.0);
        } else if(size < 1024llu * 1024llu * 1024llu) {
            count = std::snprintf(buffer, std::size(buffer), "%.2f MiB", static_cast<double>(size) / 1048576.0);
        } else {
            count = std::snprintf(buffer, std::size(buffer), "%.2f GiB", static_cast<double>(size) / 1073741824.0);
        }
        return std::string(buffer, static_cast<size_t>(count));
    }

    static void logSystemMemoryStatus()
    {
        MEMORYSTATUSEX info{ sizeof(MEMORYSTATUSEX) };
        if(!GlobalMemoryStatusEx(&info)) {
            spdlog::error("[core] Failed to retrieve system memory usage");
            return;
        }

        spdlog::info(
            "[core] System memory usage:\n"
            "    Physical memory usage: {}%\n"
            "    Total physical memory: {}\n"
            "    Available physical memory: {}\n"
            "    Current process commit memory limit: {}\n"
            "    Current process available commit memory: {}\n"
            "    Current process user-mode address space limit: {}\n"
            "    Current process available user-mode address space: {}",
            info.dwMemoryLoad,
            bytesCountToString(info.ullTotalPhys),
            bytesCountToString(info.ullAvailPhys),
            bytesCountToString(info.ullTotalPageFile),
            bytesCountToString(info.ullAvailPageFile),
            bytesCountToString(info.ullTotalVirtual),
            bytesCountToString(info.ullAvailVirtual));
    }

}

namespace Platform
{
    void prepareApplicationThread()
    {
        SetThreadAffinityMask(GetCurrentThread(), 1);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }

    void logSystemInfo()
    {
        spdlog::info("[core] System: {}", WindowsVersion::GetName());
        spdlog::info("[core] Kernel: {}", WindowsVersion::GetKernelVersionString());
        spdlog::info("[core] CPU: {}", ProcessorInfo::name());
        logSystemMemoryStatus();
    }
}
