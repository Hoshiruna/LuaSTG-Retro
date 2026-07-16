#pragma once
#include "GameResource/ResourceManager.h"
#include "lua.hpp"

int luaopen_LuaSTG_Sub(lua_State* L);

namespace luastg::binding {
	ResourcePoolId checkResourcePoolId(lua_State* L, int index);
	ResourcePool* checkResourcePool(lua_State* L, int index);
	void pushResourcePool(lua_State* L, ResourcePoolId id);
	void registerResourcePoolMethods(lua_State* L, luaL_Reg const* methods);
	IResourceTexture* checkResourceTexture(lua_State* L, int index);
	IResourceSprite* checkResourceSprite(lua_State* L, int index);
	void pushResourceTexture(lua_State* L, IResourceTexture* resource);
	void pushResourceSprite(lua_State* L, IResourceSprite* resource);
	void pushResourceAnimation(lua_State* L, IResourceAnimation* resource);
}
