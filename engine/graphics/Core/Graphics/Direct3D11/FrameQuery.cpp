#include "Core/Graphics/Direct3D11/FrameQuery.hpp"

#include "Core/Graphics/Direct3D11/Device.hpp"
#include "core/SmartReference.hpp"
#include "win32/base.hpp"

#include <d3d11.h>
#include <spdlog/spdlog.h>

#include <cassert>
#include <cstdint>
#include <utility>

namespace core::Graphics::Direct3D11
{
    class FrameQuery::Implementation final : public IDeviceEventListener
    {
    public:
        explicit Implementation(Device* const device)
            : m_device(device)
        {
            m_device->addEventListener(this);
            if(!createResources()) {
                spdlog::warn("[graphics] D3D11 frame timing queries are unavailable");
            }
        }

        ~Implementation()
        {
            m_device->removeEventListener(this);
            destroyResources();
        }

        void onDeviceCreate() override
        {
            if(!createResources()) {
                spdlog::warn("[graphics] Failed to recreate D3D11 frame timing queries");
            }
        }

        void onDeviceDestroy() override
        {
            destroyResources();
        }

        void begin()
        {
            if(!m_available) {
                return;
            }
            if(m_flying && !fetchData()) {
                return;
            }

            assert(!m_flying);
            assert(!m_active);
            m_active = true;
            m_context->Begin(m_frequency_query.get());
            m_context->End(m_start_query.get());
            m_context->Begin(m_statistics_query.get());
        }

        void end()
        {
            if(!m_available || !m_active) {
                return;
            }

            assert(m_active);
            m_context->End(m_end_query.get());
            m_context->End(m_statistics_query.get());
            m_context->End(m_frequency_query.get());
            m_active = false;
            m_flying = true;
        }

        double getTime() const noexcept
        {
            if(m_frequency.Frequency == 0) {
                return 0.0;
            }
            return static_cast<double>(m_end_time - m_start_time) / static_cast<double>(m_frequency.Frequency);
        }

        bool isAvailable() const noexcept
        {
            return m_available && m_frequency.Frequency != 0 && !m_frequency.Disjoint;
        }

    private:
        bool createResources()
        {
            destroyResources();

            auto* const device = m_device->GetD3D11Device();
            if(device == nullptr) {
                return false;
            }
            device->GetImmediateContext(m_context.put());

            D3D11_QUERY_DESC query_description{};
            query_description.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            if(FAILED(device->CreateQuery(&query_description, m_frequency_query.put()))) {
                destroyResources();
                return false;
            }
            query_description.Query = D3D11_QUERY_TIMESTAMP;
            if(FAILED(device->CreateQuery(&query_description, m_start_query.put())) || FAILED(device->CreateQuery(&query_description, m_end_query.put()))) {
                destroyResources();
                return false;
            }
            query_description.Query = D3D11_QUERY_PIPELINE_STATISTICS;
            if(FAILED(device->CreateQuery(&query_description, m_statistics_query.put()))) {
                destroyResources();
                return false;
            }

            m_available = true;
            return true;
        }

        void destroyResources()
        {
            m_statistics_query.reset();
            m_end_query.reset();
            m_start_query.reset();
            m_frequency_query.reset();
            m_context.reset();
            m_frequency = {};
            m_start_time = 0;
            m_end_time = 0;
            m_statistics = {};
            m_available = false;
            m_flying = false;
            m_active = false;
        }

        bool fetchData()
        {
            assert(m_available);
            assert(m_flying);
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT frequency{};
            uint64_t start{}, end{};
            D3D11_QUERY_DATA_PIPELINE_STATISTICS statistics{};
            auto read = [&](ID3D11Query* query, void* data, UINT size) {
                const HRESULT result = m_context->GetData(query, data, size, D3D11_ASYNC_GETDATA_DONOTFLUSH);
                if(FAILED(result)) {
                    spdlog::warn("[graphics] Read D3D11 frame query failed: {}", static_cast<uint32_t>(result));
                    destroyResources();
                }
                return result == S_OK;
            };
            if(!read(m_frequency_query.get(), &frequency, sizeof(frequency)) ||
                !read(m_start_query.get(), &start, sizeof(start)) ||
                !read(m_end_query.get(), &end, sizeof(end)) ||
                !read(m_statistics_query.get(), &statistics, sizeof(statistics))) {
                return false;
            }
            m_frequency = frequency;
            m_start_time = start;
            m_end_time = end;
            m_statistics = statistics;
            m_flying = false;
            return true;
        }

        SmartReference<Device> m_device;
        win32::com_ptr<ID3D11DeviceContext> m_context;
        win32::com_ptr<ID3D11Query> m_frequency_query;
        win32::com_ptr<ID3D11Query> m_start_query;
        win32::com_ptr<ID3D11Query> m_end_query;
        win32::com_ptr<ID3D11Query> m_statistics_query;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT m_frequency{};
        uint64_t m_start_time{};
        uint64_t m_end_time{};
        D3D11_QUERY_DATA_PIPELINE_STATISTICS m_statistics{};
        bool m_available{};
        bool m_flying{};
        bool m_active{};
    };

    FrameQuery::FrameQuery(Device* const device)
        : m_implementation(std::make_unique<Implementation>(device))
    {
    }

    FrameQuery::FrameQuery(FrameQuery&&) noexcept = default;
    FrameQuery::~FrameQuery() = default;
    FrameQuery& FrameQuery::operator=(FrameQuery&&) noexcept = default;

    void FrameQuery::begin()
    {
        m_implementation->begin();
    }

    void FrameQuery::end()
    {
        m_implementation->end();
    }

    double FrameQuery::getTime()
    {
        return m_implementation->getTime();
    }

    bool FrameQuery::isAvailable() const noexcept
    {
        return m_implementation->isAvailable();
    }
}
