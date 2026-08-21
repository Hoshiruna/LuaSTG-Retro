#include "LuaBinding/LuaWrapper.hpp"
#include <SDL3/SDL_timer.h>

class fcyStopWatch
{
private:
    uint64_t m_last{};
    uint64_t m_pause_start{};
    uint64_t m_paused_time{};
    bool m_paused{};

public:
    void Pause();
    void Resume();
    void Reset();
    double GetElapsed();
    fcyStopWatch();
};

fcyStopWatch::fcyStopWatch()
{
    Reset();
}

void
fcyStopWatch::Pause()
{
    if(!m_paused) {
        m_pause_start = SDL_GetTicksNS();
        m_paused = true;
    }
}

void
fcyStopWatch::Resume()
{
    if(!m_paused) {
        return;
    }
    m_paused_time += SDL_GetTicksNS() - m_pause_start;
    m_paused = false;
}

void
fcyStopWatch::Reset()
{
    m_last = SDL_GetTicksNS();
    m_pause_start = 0;
    m_paused_time = 0;
    m_paused = false;
}

double
fcyStopWatch::GetElapsed()
{
    auto const current = m_paused ? m_pause_start : SDL_GetTicksNS();
    return static_cast<double>(current - m_last - m_paused_time) / 1'000'000'000.0;
}

namespace luastg::binding
{
    void StopWatch::Register(lua_State* L) noexcept
    {
        struct Function
        {
#define GETUDATA(p, i) fcyStopWatch*(p) = static_cast<fcyStopWatch*>(luaL_checkudata(L, (i), LUASTG_LUA_TYPENAME_STOPWATCH));
            static int Reset(lua_State* L)
            {
                GETUDATA(p, 1);
                p->Reset();
                return 1;
            }
            static int Pause(lua_State* L)
            {
                GETUDATA(p, 1);
                p->Pause();
                return 1;
            }
            static int Resume(lua_State* L)
            {
                GETUDATA(p, 1);
                p->Resume();
                return 1;
            }
            static int GetElapsed(lua_State* L)
            {
                GETUDATA(p, 1);
                lua_pushnumber(L, (lua_Number)p->GetElapsed());
                return 1;
            }
            static int Meta_ToString(lua_State* L) noexcept
            {
                ::lua_pushfstring(L, LUASTG_LUA_TYPENAME_STOPWATCH);
                return 1;
            }
#undef GETUDATA
        };

        luaL_Reg tMethods[] = {
            { "Reset", &Function::Reset },
            { "Pause", &Function::Pause },
            { "Resume", &Function::Resume },
            { "GetElapsed", &Function::GetElapsed },
            { NULL, NULL }
        };
        luaL_Reg tMetaTable[] = {
            { "__tostring", &Function::Meta_ToString },
            { NULL, NULL }
        };

        RegisterClassIntoTable(L, ".StopWatch", tMethods, LUASTG_LUA_TYPENAME_STOPWATCH, tMetaTable);
    }

    void StopWatch::CreateAndPush(lua_State* L)
    {
        fcyStopWatch* p = static_cast<fcyStopWatch*>(lua_newuserdata(L, sizeof(fcyStopWatch))); // udata
        new(p) fcyStopWatch();
        luaL_getmetatable(L, LUASTG_LUA_TYPENAME_STOPWATCH); // udata mt
        lua_setmetatable(L, -2); // udata
    }
}
