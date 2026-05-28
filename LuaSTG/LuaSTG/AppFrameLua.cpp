#include "AppFrame.h"
#include "GameResource/ResourcePassword.hpp"
#include "LuaBinding/LuaAppFrame.hpp"
#include "LuaBinding/LuaCustomLoader.hpp"
#include "LuaBinding/LuaWrapper.hpp"
extern "C" {
#include "lua_cjson.h"
#include "lfs.h"
	extern int luaopen_string_pack(lua_State* L);
}
#ifdef LUASTG_LINK_LUASOCKET
extern "C" {
	extern int luaopen_mime_core(lua_State* L);
	extern int luaopen_socket_core(lua_State* L);
}
#endif
//#include "lua_xlsx_csv.h"
#include "lua_steam.h"
#include "LuaBinding/external/lua_xinput.hpp"
#include "LuaBinding/external/lua_random.hpp"
#include "LuaBinding/external/lua_dwrite.hpp"

#include "core/Logger.hpp"
#include "core/CommandLineArguments.hpp"
#include "core/FileSystem.hpp"
#include "utf8.hpp"
#include "lua/plus.hpp"
#include "luastg/EmbeddedFileSystem.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

using std::string_view_literals::operator ""sv;

namespace {
	void registerCommandLineArguments(lua_State* const vm) {
		[[maybe_unused]] lua::stack_balancer_t const sb(vm);
		lua::stack_t const ctx(vm);

		auto const args{ core::CommandLineArguments::copy() };
		auto const args_table = ctx.create_array(args.size());
		for (size_t i = 0; i < args.size(); i += 1) {
			core::Logger::info("[luajit] [{}] {}", i, args[i]);
			ctx.set_array_value(args_table, static_cast<int32_t>(i + 1), args[i]);
		}

		auto const m = ctx.push_module("lstg"sv);
		ctx.set_map_value(m, "args"sv, args_table);
	}
}

namespace luastg
{
	static int StackTraceback(lua_State *L) noexcept
    {
        // ??? errmsg
        int ret = 0;

        lua_getfield(L, LUA_GLOBALSINDEX, "debug");            // ??? errmsg table(debug)
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);                                     // ??? errmsg
            return 1;
        }
        lua_getfield(L, -1, "traceback");                      // ??? errmsg table(debug) function(debug.traceback)
        if (!lua_isfunction(L, -1) && !lua_iscfunction(L, -1))
        {
            lua_pop(L, 2);                                     // ??? errmsg
            return 1;
        }

        lua_pushvalue(L, -3);        // ??? errmsg table(debug) function(debug.traceback) errmsg
        lua_pushinteger(L, 2);       // ??? errmsg table(debug) function(debug.traceback) errmsg 2
        ret = lua_pcall(L, 2, 1, 0); // ??? errmsg table(debug) tracebackmsg   <==> [lua] debug.traceback(errmsg, 2)
        if (0 != ret)
        {
            char const* errmsg = lua_tostring(L, -1);
            if (errmsg == nullptr) {
                errmsg = "(error object is a nil value)";
            }
            spdlog::error("[luajit] Error occurred during StackTraceback: {}", errmsg);// ??? errmsg t errmsg
            lua_pop(L, 2);                                                // ??? errmsg
            return 1;
        }

        return 1;
    }

	bool AppFrame::SafeCallScript(const char* source, size_t len, const char* desc) noexcept
	{
		lua_pushcfunction(L, &StackTraceback);          // ??? f
		int tStacktraceIndex = lua_gettop(L);
		if (0 != luaL_loadbuffer(L, source, len, desc)) // ??? f f/s
		{
			try
			{
				spdlog::error("[luajit] Failed to compile '{}': {}", desc, lua_tostring(L, -1));
				MessageBoxW(
					m_pAppModel ? (HWND)m_pAppModel->getWindow()->getNativeHandle() : NULL,
					utf8::to_wstring(
						fmt::format("Failed to compile '{}': {}", desc, lua_tostring(L, -1))
					).c_str(),
					L"" LUASTG_INFO,
					MB_ICONERROR | MB_OK
				);
			}
			catch (const std::bad_alloc&)
			{
				spdlog::error("[luastg] Error occurred while writing log");
			}
			lua_pop(L, 2);
			return false;
		}
		if (0 != lua_pcall(L, 0, 0, tStacktraceIndex)) // ??? f _/e
		{
			try
			{
				char const* errmsg = lua_tostring(L, -1);
				if (errmsg == nullptr) {
					errmsg = "(error object is a nil value)";
				}
				spdlog::error("[luajit] Error while running '{}': {}", desc, errmsg);
				MessageBoxW(
					m_pAppModel ? (HWND)m_pAppModel->getWindow()->getNativeHandle() : NULL,
					utf8::to_wstring(
						fmt::format("Error while running '{}': \n{}", desc, errmsg)
					).c_str(),
					L"" LUASTG_INFO,
					MB_ICONERROR | MB_OK
				);
			}
			catch (const std::bad_alloc&)
			{
				spdlog::error("[luastg] Failed to write log message");
			}
			lua_pop(L, 2);
			return false;
		}
		lua_pop(L, 1);
		return true;
	}

	bool AppFrame::UnsafeCallGlobalFunction(const char* name, int retc) noexcept
	{
		lua_getglobal(L, name); // ... f
		if (lua_isfunction(L, -1) || lua_iscfunction(L, -1))
		{
			lua_call(L, 0, retc);
			return true;
		}
		return false;
	}

	bool AppFrame::SafeCallGlobalFunction(const char* name, int retc) noexcept
	{
		lua_pushcfunction(L, &StackTraceback); // ... f
		int tStacktraceIndex = lua_gettop(L);
		lua_getglobal(L, name);                // ... f f
		if (0 != lua_pcall(L, 0, retc, tStacktraceIndex))
		{
			try
			{
				char const* errmsg = lua_tostring(L, -1);
				if (errmsg == nullptr) {
					errmsg = "(error object is a nil value)";
				}
				spdlog::error("[luajit] Error while calling global function '{}':{}", name, errmsg);
				MessageBoxW(
					m_pAppModel ? (HWND)m_pAppModel->getWindow()->getNativeHandle() : NULL,
					utf8::to_wstring(
						fmt::format("Error while calling global function '{}':\n{}", name, errmsg)
					).c_str(),
					L"" LUASTG_INFO,
					MB_ICONERROR | MB_OK
				);
			}
			catch (const std::bad_alloc&)
			{
				spdlog::error("[luastg] Failed to write log message");
			}
			lua_pop(L, 2);
			return false;
		}
		lua_remove(L, tStacktraceIndex);
		return true;
	}

	// TODO: 废弃
	bool AppFrame::SafeCallGlobalFunctionB(const char* name, int argc, int retc) noexcept
	{
		const int base_stack = lua_gettop(L) - argc;
		//																// ? ...
		lua_pushcfunction(L, &StackTraceback);							// ? ... trace
		lua_getglobal(L, name);											// ? ... trace func
		if (lua_type(L, lua_gettop(L)) != LUA_TFUNCTION)
		{
			//															// ? ... trace nil
		#ifdef _DEBUG
			try
			{
				spdlog::error("[luajit] Error while calling global function '{}': function does not exist", name);
				/*
				MessageBoxW(
					m_pAppModel ? (HWND)m_pAppModel->getWindow()->getNativeHandle() : NULL,
					tErrorInfo.c_str(),
					L"" LUASTG_INFO,
					MB_ICONERROR | MB_OK);
				//*/
			}
			catch (const std::bad_alloc&)
			{
				spdlog::error("[luastg] Failed to write log message");
			}
		#endif
			lua_pop(L, argc + 2); 										// ?
			return false;
		}
		if (argc > 0)
		{
			lua_insert(L, base_stack + 1);								// ? func ... trace
			lua_insert(L, base_stack + 1);								// ? trace func ...
		}
		if (0 != lua_pcall(L, argc, retc, base_stack + 1))
		{
			//															// ? trace errmsg
			try
			{
				spdlog::error("[luajit] Error while calling global function '{}': {}", name, lua_tostring(L, -1));
				MessageBoxW(
					m_pAppModel ? (HWND)m_pAppModel->getWindow()->getNativeHandle() : NULL,
					utf8::to_wstring(
						fmt::format("Error while calling global function '{}':\n{}", name, lua_tostring(L, -1))
					).c_str(),
					L"" LUASTG_INFO,
					MB_ICONERROR | MB_OK);
			}
			catch (const std::bad_alloc&)
			{
				spdlog::error("[luastg] Failed to write log message");
			}
			lua_pop(L, 2);												// ?
			return false;
		}
		else
		{
			if (retc > 0)
			{
				//														// ? trace ...
				lua_remove(L, base_stack + 1);							// ? ...
			}
			else
			{
				//														// ? trace
				lua_pop(L, 1);											// ?
			}
			return true;
		}
	}

	void AppFrame::LoadScript(lua_State* SL, const char* path, const char* packname)
	{
	#define L (fuck) // Do not use the global lua_State here; use the one passed in instead.
		if (ResourceMgr::GetResourceLoadingLog())
		{
			if (packname)
				spdlog::info("[luastg] Loading script '{}' from resource package '{}'", packname, path);
			else
				spdlog::info("[luastg] Loading script '{}'", path);
		}
		bool loaded = false;
		core::SmartReference<core::IData> src;
		if (packname)
		{
			core::SmartReference<core::IFileSystemArchive> archive;
			if (core::FileSystemManager::getFileSystemArchiveByPath(packname, archive.put())) {
				loaded = archive->readFile(path, src.put());
			}
		}
		else
		{
			loaded = core::FileSystemManager::readFile(path, src.put());
		}
		if (!loaded)
		{
			spdlog::error("[luastg] can't load file '{}'", path);
			luaL_error(SL, "can't load file '%s'", path);
			return;
		}
		if (0 != luaL_loadbuffer(SL, (char const*)src->data(), src->size(), luaL_checkstring(SL, 1)))
		{
			const char* tDetail = lua_tostring(SL, -1);
			spdlog::error("[luajit] Failed to compile '{}': {}", path, tDetail);
			luaL_error(SL, "failed to compile '%s': %s", path, tDetail);
			return;
		}
		lua_call(SL, 0, LUA_MULTRET); // This is normally only called from Lua code, and the outer layer already has a pcall.这个一般只会在lua代码调用，外层已经有pcall了
	#undef L
	}

	bool AppFrame::OnOpenLuaEngine()
	{
		// Mounting file system
		core::FileSystemManager::addFileSystem("luastg", IEmbeddedFileSystem::getInstance());

		// Loading Lua virtual machine
		spdlog::info("[luajit] {}", LUAJIT_VERSION);
		L = luaL_newstate();
		if (!L)
		{
			spdlog::error("[luajit] Failed to create LuaJIT engine");
			return false;
		}
		if (0 == luaJIT_setmode(L, 0, LUAJIT_MODE_ENGINE | LUAJIT_MODE_ON))
		{
			spdlog::error("[luajit] Failed to enable JIT mode");
		}
		lua_gc(L, LUA_GCSTOP, 0);  // Disabling GC during initialization
		{
			spdlog::info("[luajit] Registering standard libraries and built-in packages");
			luaL_openlibs(L);  // Built-in libraries (lua build in lib)
			lua_register_custom_loader(L); // Enhanced package library (require)
			luaopen_cjson(L);
			luaopen_lfs(L);
			//lua_xlsx_open(L);
			//lua_csv_open(L);
			lua_steam_open(L);
			luaopen_xinput(L);
			luaopen_dwrite(L);
			luaopen_random(L);
			luaopen_string_pack(L);
		#ifdef LUASTG_LINK_LUASOCKET
			{
				lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED"); // ... _LOADED
				{
					luaopen_socket_core(L);        // ... _LOADED socket
					lua_setfield(L, -2, "socket.core"); // ... _LOADED
					luaopen_mime_core(L);          // ... _LOADED mime
					lua_setfield(L, -2, "mime.core");   // ... _LOADED
				}
				lua_pop(L, 1); // ...
			}
		#endif
			lua_settop(L, 0);

			binding::RegistBuiltInClassWrapper(L); // LuaSTG API
			registerCommandLineArguments(L); // command line args

			constexpr std::string_view boost_script(R"(-- Luastg Retro boost script
package.cpath = ""
package.path = "?.lua;?/init.lua;"
require("luastg.main")
)");
			if (!SafeCallScript(boost_script.data(), boost_script.size(), "luastg/boost.lua")) {
				return false;
			}
		}
		lua_gc(L, LUA_GCRESTART, -1);  // Restart GC

		return true;
	}
}
