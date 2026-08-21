#include "Core/FrameRateController.hpp"

#include <SDL3/SDL_timer.h>

#include <algorithm>
#include <limits>

namespace core
{
    FrameRateController::FrameRateController(uint32_t const target_fps)
    {
        setTargetFPS(target_fps);
    }

    double FrameRateController::update()
    {
        auto current_tick = SDL_GetTicksNS();
        auto const frame_duration = frameDurationNanoseconds();
        if(m_last_tick == 0) {
            m_last_tick = current_tick;
            m_next_tick = current_tick + frame_duration;
            double const elapsed_seconds = static_cast<double>(frame_duration) / 1'000'000'000.0;
            updateStatistics(elapsed_seconds);
            return elapsed_seconds;
        }

        if(current_tick < m_next_tick) {
            SDL_DelayPrecise(m_next_tick - current_tick);
            current_tick = SDL_GetTicksNS();
        }

        auto const maximum_lag = frame_duration * 10;
        if(current_tick > m_next_tick && current_tick - m_next_tick >= maximum_lag) {
            m_next_tick = current_tick + frame_duration;
        } else {
            m_next_tick += frame_duration;
        }

        auto const elapsed_nanoseconds = current_tick >= m_last_tick ? current_tick - m_last_tick : frame_duration;
        m_last_tick = current_tick;

        double const elapsed_seconds = std::max(
            static_cast<double>(elapsed_nanoseconds) / 1'000'000'000.0,
            std::numeric_limits<double>::epsilon());
        updateStatistics(elapsed_seconds);
        return elapsed_seconds;
    }

    uint32_t FrameRateController::getTargetFPS()
    {
        return m_target_fps;
    }

    void FrameRateController::setTargetFPS(uint32_t const target_fps)
    {
        m_target_fps = std::max(target_fps, 1u);
        if(m_last_tick != 0) {
            m_next_tick = SDL_GetTicksNS() + frameDurationNanoseconds();
        }
    }

    double FrameRateController::getFPS()
    {
        return m_current_fps;
    }

    uint64_t FrameRateController::getTotalFrame()
    {
        return m_total_frames;
    }

    double FrameRateController::getTotalTime()
    {
        return m_total_time;
    }

    double FrameRateController::getAvgFPS()
    {
        return m_average_fps;
    }

    double FrameRateController::getMinFPS()
    {
        return m_minimum_fps;
    }

    double FrameRateController::getMaxFPS()
    {
        return m_maximum_fps;
    }

    uint64_t FrameRateController::frameDurationNanoseconds() const
    {
        return std::max(1'000'000'000ull / m_target_fps, 1ull);
    }

    void FrameRateController::updateStatistics(double const elapsed_seconds)
    {
        m_total_frames += 1;
        m_total_time += elapsed_seconds;
        m_current_fps = 1.0 / elapsed_seconds;

        m_frame_times[m_frame_time_index] = elapsed_seconds;
        m_frame_time_index = (m_frame_time_index + 1) % m_frame_times.size();
        m_frame_time_count = std::min(m_frame_time_count + 1, m_frame_times.size());

        double total_history_time{};
        m_minimum_fps = std::numeric_limits<double>::max();
        m_maximum_fps = std::numeric_limits<double>::lowest();
        for(size_t index = 0; index < m_frame_time_count; index += 1) {
            auto const frame_time = m_frame_times[index];
            auto const frame_rate = 1.0 / frame_time;
            total_history_time += frame_time;
            m_minimum_fps = std::min(m_minimum_fps, frame_rate);
            m_maximum_fps = std::max(m_maximum_fps, frame_rate);
        }
        m_average_fps = static_cast<double>(m_frame_time_count) / total_history_time;
    }
}
