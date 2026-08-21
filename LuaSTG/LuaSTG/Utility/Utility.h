#pragma once

#include <cstdint>

namespace luastg
{
    class TimerScope
    {
    private:
        uint64_t _start;
        float& _out;

    public:
        float operator()() const;
        explicit TimerScope(float& Out);
        ~TimerScope();
    };

    class CoInitializeScope
    {
    private:
        bool _result = false;

    public:
        inline bool operator()() const { return _result; }
        CoInitializeScope();
        ~CoInitializeScope();
    };
}
