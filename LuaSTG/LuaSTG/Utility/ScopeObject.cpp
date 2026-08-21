#include "Utility/Utility.h"
#include <SDL3/SDL_timer.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <combaseapi.h>

namespace luastg
{
    class Scope
    {
    private:
        std::function<void()> m_WhatToDo;

    public:
        explicit Scope(std::function<void()> exitJob)
            : m_WhatToDo(std::move(exitJob)) {}
        ~Scope() { m_WhatToDo(); }
    };

    float TimerScope::operator()() const
    {
        return static_cast<float>(static_cast<double>(SDL_GetTicksNS() - _start) / 1'000'000'000.0);
    }
    TimerScope::TimerScope(float& inout)
        : _start(SDL_GetTicksNS()), _out(inout)
    {
        _out = 0.0f;
    }
    TimerScope::~TimerScope()
    {
        _out = operator()();
    }

    CoInitializeScope::CoInitializeScope()
    {
        _result = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    }
    CoInitializeScope::~CoInitializeScope()
    {
        if(_result) {
            ::CoUninitialize();
        }
    }
}
