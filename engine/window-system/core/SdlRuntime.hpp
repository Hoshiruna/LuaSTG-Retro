#pragma once

namespace core
{
    class SdlRuntime
    {
    public:
        SdlRuntime() = default;
        SdlRuntime(const SdlRuntime&) = delete;
        SdlRuntime(SdlRuntime&&) = delete;
        ~SdlRuntime();

        SdlRuntime& operator=(const SdlRuntime&) = delete;
        SdlRuntime& operator=(SdlRuntime&&) = delete;

        bool initialize();
        bool isInitialized() const noexcept { return m_initialized; }

    private:
        bool m_initialized{};
    };
}
