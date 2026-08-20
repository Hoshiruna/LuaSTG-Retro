#pragma once

#include "GameResource/ResourceFont.hpp"
#include "lua.hpp"

#include <string_view>

namespace luastg::binding
{
    struct DynamicFont
    {
        static std::string_view const class_name;

        IResourceFont* data{};

        static bool is(lua_State* vm, int index);
        static DynamicFont* as(lua_State* vm, int index);
        static void push(lua_State* vm, IResourceFont* resource);
        static void registerClass(lua_State* vm);
    };
}
