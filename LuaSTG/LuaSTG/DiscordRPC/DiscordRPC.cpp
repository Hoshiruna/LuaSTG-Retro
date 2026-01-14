#include "luastg_config_generated.h"
#include "DiscordRPC/DiscordRPC.hpp"

#ifdef LUASTG_DISCORD_RPC_ENABLE
#include "discord_rpc.h"
#endif

namespace luastg::DiscordRPC
{
#ifdef LUASTG_DISCORD_RPC_ENABLE
	namespace
	{
#if defined(LUASTG_DISCORD_RPC_APP_ID)
		constexpr char kDiscordAppId[] = LUASTG_DISCORD_RPC_APP_ID;
#else
		constexpr char kDiscordAppId[] = "";
#endif
		bool g_initialized = false;
	}
#endif

	bool Init()
	{
#ifdef LUASTG_DISCORD_RPC_ENABLE
		if (kDiscordAppId[0] == '\0') {
			return true;
		}

		DiscordEventHandlers handlers{};
		::Discord_Initialize(kDiscordAppId, &handlers, 1, nullptr);
		g_initialized = true;
#endif
		return true;
	}

	void RunCallbacks()
	{
#ifdef LUASTG_DISCORD_RPC_ENABLE
		if (g_initialized) {
			::Discord_RunCallbacks();
		}
#endif
	}

	void Shutdown()
	{
#ifdef LUASTG_DISCORD_RPC_ENABLE
		if (g_initialized) {
			::Discord_Shutdown();
			g_initialized = false;
		}
#endif
	}
}
