#pragma once

#include <memory>

namespace core::Graphics::Direct3D11
{
    class Device;

    class FrameQuery final
    {
    public:
        explicit FrameQuery(Device* device);
        FrameQuery(FrameQuery const&) = delete;
        FrameQuery(FrameQuery&&) noexcept;
        ~FrameQuery();

        FrameQuery& operator=(FrameQuery const&) = delete;
        FrameQuery& operator=(FrameQuery&&) noexcept;

        void begin();
        void end();
        double getTime();

    private:
        class Implementation;
        std::unique_ptr<Implementation> m_implementation;
    };
}
