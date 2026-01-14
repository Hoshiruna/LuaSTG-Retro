#include "luastg_config_generated.h"
#include "LuaBinding/LuaWrapper.hpp"

#ifdef LUASTG_DISCORD_RPC_ENABLE
#include "discord_rpc.h"
#endif

namespace
{
#ifdef LUASTG_DISCORD_RPC_ENABLE
	const char* get_string_field(lua_State* L, int idx, const char* key) noexcept
	{
		lua_getfield(L, idx, key);
		const char* value = nullptr;
		if (!lua_isnil(L, -1))
		{
			value = luaL_checkstring(L, -1);
		}
		lua_pop(L, 1);
		return value;
	}

	int get_int_field(lua_State* L, int idx, const char* key) noexcept
	{
		lua_getfield(L, idx, key);
		int value = 0;
		if (!lua_isnil(L, -1))
		{
			value = (int)luaL_checkinteger(L, -1);
		}
		lua_pop(L, 1);
		return value;
	}

	int64_t get_int64_field(lua_State* L, int idx, const char* key) noexcept
	{
		lua_getfield(L, idx, key);
		int64_t value = 0;
		if (!lua_isnil(L, -1))
		{
			value = (int64_t)luaL_checkinteger(L, -1);
		}
		lua_pop(L, 1);
		return value;
	}
#endif
}

void luastg::binding::DiscordRPC::Register(lua_State* L) noexcept
{
	struct Wrapper
	{
		static int IsEnabled(lua_State* L) noexcept
		{
#ifdef LUASTG_DISCORD_RPC_ENABLE
			lua_pushboolean(L, 1);
#else
			lua_pushboolean(L, 0);
#endif
			return 1;
		}

		static int UpdatePresence(lua_State* L) noexcept
		{
#ifdef LUASTG_DISCORD_RPC_ENABLE
			luaL_checktype(L, 1, LUA_TTABLE);
			DiscordRichPresence presence{};
			presence.state = get_string_field(L, 1, "state");
			presence.details = get_string_field(L, 1, "details");
			presence.startTimestamp = get_int64_field(L, 1, "startTimestamp");
			presence.endTimestamp = get_int64_field(L, 1, "endTimestamp");
			presence.largeImageKey = get_string_field(L, 1, "largeImageKey");
			presence.largeImageText = get_string_field(L, 1, "largeImageText");
			presence.smallImageKey = get_string_field(L, 1, "smallImageKey");
			presence.smallImageText = get_string_field(L, 1, "smallImageText");
			presence.partyId = get_string_field(L, 1, "partyId");
			presence.partySize = get_int_field(L, 1, "partySize");
			presence.partyMax = get_int_field(L, 1, "partyMax");
			presence.partyPrivacy = get_int_field(L, 1, "partyPrivacy");
			presence.matchSecret = get_string_field(L, 1, "matchSecret");
			presence.joinSecret = get_string_field(L, 1, "joinSecret");
			presence.spectateSecret = get_string_field(L, 1, "spectateSecret");
			presence.instance = (int8_t)get_int_field(L, 1, "instance");
			::Discord_UpdatePresence(&presence);
#else
			(void)L;
#endif
			return 0;
		}

		static int ClearPresence(lua_State* L) noexcept
		{
#ifdef LUASTG_DISCORD_RPC_ENABLE
			::Discord_ClearPresence();
#else
			(void)L;
#endif
			return 0;
		}
	};

	luaL_Reg lib[] = {
		{ "IsEnabled", &Wrapper::IsEnabled },
		{ "UpdatePresence", &Wrapper::UpdatePresence },
		{ "ClearPresence", &Wrapper::ClearPresence },
		{ NULL, NULL },
	};

	luaL_register(L, LUASTG_LUA_LIBNAME ".DiscordRPC", lib); // lib (also sets _G["lstg.DiscordRPC"])
	lua_getglobal(L, LUASTG_LUA_LIBNAME);                    
	lua_pushvalue(L, -2);                                    
	lua_setfield(L, -2, "DiscordPRC");                       // lib lstg (lstg.DiscordPRC = lib)
	lua_pop(L, 2);                                           
}
