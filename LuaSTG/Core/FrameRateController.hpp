#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace core
{
    struct IFrameRateController
    {
        virtual ~IFrameRateController() = default;
        virtual double update() = 0;
        virtual uint32_t getTargetFPS() = 0;
        virtual void setTargetFPS(uint32_t target_fps) = 0;
        virtual double getFPS() = 0;
        virtual uint64_t getTotalFrame() = 0;
        virtual double getTotalTime() = 0;
        virtual double getAvgFPS() = 0;
        virtual double getMinFPS() = 0;
        virtual double getMaxFPS() = 0;
    };

    class FrameRateController final : public IFrameRateController
    {
    public:
        explicit FrameRateController(uint32_t target_fps = 60);

        double update() override;
        uint32_t getTargetFPS() override;
        void setTargetFPS(uint32_t target_fps) override;
        double getFPS() override;
        uint64_t getTotalFrame() override;
        double getTotalTime() override;
        double getAvgFPS() override;
        double getMinFPS() override;
        double getMaxFPS() override;

    private:
        uint64_t frameDurationNanoseconds() const;
        void updateStatistics(double elapsed_seconds);

        uint32_t m_target_fps{ 60 };
        uint64_t m_last_tick{};
        uint64_t m_next_tick{};
        uint64_t m_total_frames{};
        double m_total_time{};
        double m_current_fps{};
        double m_average_fps{};
        double m_minimum_fps{};
        double m_maximum_fps{};
        std::array<double, 60> m_frame_times{};
        size_t m_frame_time_count{};
        size_t m_frame_time_index{};
    };
}
