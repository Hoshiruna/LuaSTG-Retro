#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace core::Graphics::SDLGPU
{
    enum class RecoveryState
    {
        Healthy,
        RecoveryPending,
        Recovering,
        Fatal
    };
    enum class RecoveryStage
    {
        Storage,
        Geometry,
        Shader,
        Renderer,
        Presentation
    };

    class RecoveryPolicy
    {
    public:
        explicit RecoveryPolicy(std::atomic_bool& attempted) : m_attempted(attempted) {}
        RecoveryState state() const noexcept { return m_state; }
        void request() noexcept
        {
            if(m_state == RecoveryState::Healthy)
                m_state = RecoveryState::RecoveryPending;
        }
        bool begin() noexcept
        {
            if(m_state != RecoveryState::RecoveryPending)
                return false;
            if(m_attempted.exchange(true)) {
                m_state = RecoveryState::Fatal;
                return false;
            }
            m_state = RecoveryState::Recovering;
            return true;
        }
        bool complete() noexcept
        {
            if(m_state != RecoveryState::Recovering)
                return false;
            m_state = RecoveryState::Healthy;
            return true;
        }
        void fatal() noexcept { m_state = RecoveryState::Fatal; }

    private:
        std::atomic_bool& m_attempted;
        RecoveryState m_state{ RecoveryState::Healthy };
    };

    class RecoveryRegistry
    {
    public:
        struct Entry
        {
            size_t id;
            RecoveryStage stage;
            std::string name;
            std::function<void()> release, rebuild;
        };
        size_t add(RecoveryStage stage, std::string name, std::function<void()> release, std::function<void()> rebuild)
        {
            const auto id = ++m_next;
            m_entries.push_back({ id, stage, std::move(name), std::move(release), std::move(rebuild) });
            return id;
        }
        void remove(size_t id) noexcept
        {
            std::erase_if(m_entries, [id](const auto& entry) { return entry.id == id; });
        }
        void release()
        {
            auto entries = ordered();
            for(auto it = entries.rbegin(); it != entries.rend(); ++it)
                if(contains(it->id))
                    it->release();
        }
        void rebuild()
        {
            for(const auto& entry : ordered()) {
                if(!contains(entry.id))
                    continue;
                try {
                    entry.rebuild();
                } catch(const std::exception& error) {
                    throw std::runtime_error("Rebuild " + entry.name + ": " + error.what());
                }
            }
        }

    private:
        bool contains(size_t id) const
        {
            return std::any_of(m_entries.begin(), m_entries.end(), [id](const auto& entry) { return entry.id == id; });
        }
        std::vector<Entry> ordered() const
        {
            auto entries = m_entries;
            std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.stage < b.stage; });
            return entries;
        }
        std::vector<Entry> m_entries;
        size_t m_next{};
    };

    // Declare this member after the GPU resources and their owning device reference.
    class RecoveryRegistration
    {
    public:
        RecoveryRegistration() = default;
        RecoveryRegistration(const RecoveryRegistration&) = delete;
        RecoveryRegistration& operator=(const RecoveryRegistration&) = delete;
        ~RecoveryRegistration()
        {
            if(m_registry)
                m_registry->remove(m_id);
        }
        void attach(RecoveryRegistry& registry, RecoveryStage stage, std::string name, std::function<void()> release, std::function<void()> rebuild)
        {
            m_id = registry.add(stage, std::move(name), std::move(release), std::move(rebuild));
            m_registry = &registry;
        }

    private:
        RecoveryRegistry* m_registry{};
        size_t m_id{};
    };
}
