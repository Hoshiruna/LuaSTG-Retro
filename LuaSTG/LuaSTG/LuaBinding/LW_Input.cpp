#include "LuaBinding/LuaWrapper.hpp"
#include "AppFrame.h"
#include "Debugger/ImGuiExtension.h"
#include "core/InputSystem.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{
    constexpr char gamepad_type_name[] = "lstg.Input.Gamepad";
    constexpr char joystick_type_name[] = "lstg.Input.Joystick";

    struct DeviceHandle
    {
        uint32_t id{};
        bool gamepad{};
    };

    core::InputSystem& input() noexcept
    {
        imgui::syncInputCapture();
        return core::InputSystem::getInstance();
    }

    DeviceHandle* checkDevice(lua_State* const L, const int index, const char* const type_name)
    {
        return static_cast<DeviceHandle*>(luaL_checkudata(L, index, type_name));
    }

    void pushDevice(lua_State* const L, const uint32_t id, const char* const type_name, const bool gamepad)
    {
        auto* const handle = static_cast<DeviceHandle*>(lua_newuserdata(L, sizeof(DeviceHandle)));
        handle->id = id;
        handle->gamepad = gamepad;
        luaL_getmetatable(L, type_name);
        lua_setmetatable(L, -2);
    }

    template<typename Enum>
    Enum checkEnum(lua_State* const L, const int argument, const Enum count)
    {
        const auto value = luaL_checkinteger(L, argument);
        luaL_argcheck(L, value >= 0 && static_cast<uint64_t>(value) < static_cast<uint64_t>(static_cast<std::underlying_type_t<Enum>>(count)), argument, "invalid enum value");
        return static_cast<Enum>(value);
    }

    template<typename Enum, size_t Size>
    void registerEnum(lua_State* const L, const char* const name, const std::array<std::pair<std::string_view, Enum>, Size>& values)
    {
        constexpr luaL_Reg empty[] = { { nullptr, nullptr } };
        luaL_register(L, name, empty);
        for(const auto& [key, value] : values) {
            lua_pushlstring(L, key.data(), key.size());
            lua_pushinteger(L, static_cast<lua_Integer>(value));
            lua_settable(L, -3);
        }
        lua_pop(L, 1);
    }

    int keyboardIsDown(lua_State* const L)
    {
        lua_pushboolean(L, input().isKeyDown(checkEnum(L, 1, core::Key::count)));
        return 1;
    }

    int keyboardWasPressed(lua_State* const L)
    {
        lua_pushboolean(L, input().wasKeyPressed(checkEnum(L, 1, core::Key::count)));
        return 1;
    }

    int keyboardWasReleased(lua_State* const L)
    {
        lua_pushboolean(L, input().wasKeyReleased(checkEnum(L, 1, core::Key::count)));
        return 1;
    }

    int mouseIsDown(lua_State* const L)
    {
        lua_pushboolean(L, input().isMouseButtonDown(checkEnum(L, 1, core::MouseButton::count)));
        return 1;
    }

    int mouseWasPressed(lua_State* const L)
    {
        lua_pushboolean(L, input().wasMouseButtonPressed(checkEnum(L, 1, core::MouseButton::count)));
        return 1;
    }

    int mouseWasReleased(lua_State* const L)
    {
        lua_pushboolean(L, input().wasMouseButtonReleased(checkEnum(L, 1, core::MouseButton::count)));
        return 1;
    }

    int mouseGetState(lua_State* const L)
    {
        const auto state = input().getMouseState();
        lua_createtable(L, 0, 6);
#define SET_NUMBER(name, value) lua_pushnumber(L, value); lua_setfield(L, -2, name)
        SET_NUMBER("x", state.x);
        SET_NUMBER("y", state.y);
        SET_NUMBER("delta_x", state.delta_x);
        SET_NUMBER("delta_y", state.delta_y);
        SET_NUMBER("wheel_x", state.wheel_x);
        SET_NUMBER("wheel_y", state.wheel_y);
#undef SET_NUMBER
        return 1;
    }

    int mouseGetPosition(lua_State* const L)
    {
        const auto state = input().getMouseState();
        lua_pushnumber(L, state.x);
        lua_pushnumber(L, state.y);
        return 2;
    }

    int mouseGetCanvasPosition(lua_State* const L)
    {
        input();
        bool inside{};
        const auto position = LAPP.GetMousePosition(false, &inside, true);
        lua_pushnumber(L, position.x);
        lua_pushnumber(L, position.y);
        lua_pushboolean(L, inside);
        return 3;
    }

    int mouseGetMovement(lua_State* const L)
    {
        const auto state = input().getMouseState();
        lua_pushnumber(L, state.delta_x);
        lua_pushnumber(L, state.delta_y);
        return 2;
    }

    int mouseGetWheel(lua_State* const L)
    {
        const auto state = input().getMouseState();
        lua_pushnumber(L, state.wheel_x);
        lua_pushnumber(L, state.wheel_y);
        return 2;
    }

    int deviceEquals(lua_State* const L)
    {
        if(lua_getmetatable(L, 1) == 0 || lua_getmetatable(L, 2) == 0) {
            lua_settop(L, 2);
            lua_pushboolean(L, false);
            return 1;
        }
        const bool same_type = lua_rawequal(L, -1, -2) != 0;
        lua_pop(L, 2);
        const auto* const left = static_cast<DeviceHandle*>(lua_touserdata(L, 1));
        const auto* const right = static_cast<DeviceHandle*>(lua_touserdata(L, 2));
        lua_pushboolean(L, same_type && left != nullptr && right != nullptr && left->id == right->id && left->gamepad == right->gamepad);
        return 1;
    }

    int gamepadToString(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        const auto value = std::string(gamepad_type_name) + "(" + std::to_string(self->id) + ")";
        lua_pushlstring(L, value.data(), value.size());
        return 1;
    }

    int gamepadGetAll(lua_State* const L)
    {
        const auto devices = input().getGamepads();
        lua_createtable(L, static_cast<int>(devices.size()), 0);
        for(size_t i = 0; i < devices.size(); ++i) {
            pushDevice(L, devices[i].id, gamepad_type_name, true);
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }

    int gamepadGetId(lua_State* const L)
    {
        lua_pushinteger(L, checkDevice(L, 1, gamepad_type_name)->id);
        return 1;
    }

    int gamepadIsConnected(lua_State* const L)
    {
        lua_pushboolean(L, input().isGamepadConnected(checkDevice(L, 1, gamepad_type_name)->id));
        return 1;
    }

    int gamepadGetName(lua_State* const L)
    {
        const auto name = input().getGamepadName(checkDevice(L, 1, gamepad_type_name)->id);
        lua_pushlstring(L, name.data(), name.size());
        return 1;
    }

    int gamepadIsDown(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        lua_pushboolean(L, input().isGamepadButtonDown(self->id, checkEnum(L, 2, core::GamepadButton::count)));
        return 1;
    }

    int gamepadWasPressed(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        lua_pushboolean(L, input().wasGamepadButtonPressed(self->id, checkEnum(L, 2, core::GamepadButton::count)));
        return 1;
    }

    int gamepadWasReleased(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        lua_pushboolean(L, input().wasGamepadButtonReleased(self->id, checkEnum(L, 2, core::GamepadButton::count)));
        return 1;
    }

    int gamepadGetAxis(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        lua_pushnumber(L, input().getGamepadAxis(self->id, checkEnum(L, 2, core::GamepadAxis::count)));
        return 1;
    }

    int gamepadGetPlayerIndex(lua_State* const L)
    {
        lua_pushinteger(L, input().getGamepadPlayerIndex(checkDevice(L, 1, gamepad_type_name)->id));
        return 1;
    }

    int gamepadSetPlayerIndex(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        const auto player_index = luaL_checkinteger(L, 2);
        luaL_argcheck(L, player_index >= -1 && player_index <= std::numeric_limits<int32_t>::max(), 2, "player index is out of range");
        lua_pushboolean(L, input().setGamepadPlayerIndex(self->id, static_cast<int32_t>(player_index)));
        return 1;
    }

    int gamepadRumble(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, gamepad_type_name);
        const auto low_number = luaL_checknumber(L, 2);
        const auto high_number = luaL_checknumber(L, 3);
        const auto duration_number = luaL_checknumber(L, 4);
        luaL_argcheck(L, std::isfinite(low_number), 2, "magnitude must be finite");
        luaL_argcheck(L, std::isfinite(high_number), 3, "magnitude must be finite");
        luaL_argcheck(L, std::isfinite(duration_number), 4, "duration must be finite");
        luaL_argcheck(L, duration_number >= 0.0 && duration_number <= std::numeric_limits<uint32_t>::max(), 4, "duration is out of range");
        luaL_argcheck(L, std::trunc(duration_number) == duration_number, 4, "duration must be an integer");
        const auto low = static_cast<float>(std::clamp(low_number, 0.0, 1.0));
        const auto high = static_cast<float>(std::clamp(high_number, 0.0, 1.0));
        const auto duration = static_cast<uint32_t>(duration_number);
        lua_pushboolean(L, input().rumbleGamepad(self->id, low, high, duration));
        return 1;
    }

    int joystickToString(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, joystick_type_name);
        const auto value = std::string(joystick_type_name) + "(" + std::to_string(self->id) + ")";
        lua_pushlstring(L, value.data(), value.size());
        return 1;
    }

    int joystickGetAll(lua_State* const L)
    {
        const auto devices = input().getJoysticks();
        lua_createtable(L, static_cast<int>(devices.size()), 0);
        for(size_t i = 0; i < devices.size(); ++i) {
            pushDevice(L, devices[i].id, joystick_type_name, false);
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }

    int joystickGetId(lua_State* const L)
    {
        lua_pushinteger(L, checkDevice(L, 1, joystick_type_name)->id);
        return 1;
    }

    int joystickIsConnected(lua_State* const L)
    {
        lua_pushboolean(L, input().isJoystickConnected(checkDevice(L, 1, joystick_type_name)->id));
        return 1;
    }

    int joystickGetName(lua_State* const L)
    {
        const auto name = input().getJoystickName(checkDevice(L, 1, joystick_type_name)->id);
        lua_pushlstring(L, name.data(), name.size());
        return 1;
    }

    uint32_t checkOneBasedIndex(lua_State* const L, const int argument)
    {
        const auto index = luaL_checkinteger(L, argument);
        luaL_argcheck(L, index >= 1 && static_cast<uint64_t>(index) <= std::numeric_limits<uint32_t>::max(), argument, "index is out of range");
        return static_cast<uint32_t>(index - 1);
    }

    int joystickGetButtonCount(lua_State* const L)
    {
        lua_pushinteger(L, input().getJoystickButtonCount(checkDevice(L, 1, joystick_type_name)->id));
        return 1;
    }

    int joystickIsDown(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, joystick_type_name);
        lua_pushboolean(L, input().isJoystickButtonDown(self->id, checkOneBasedIndex(L, 2)));
        return 1;
    }

    int joystickWasPressed(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, joystick_type_name);
        lua_pushboolean(L, input().wasJoystickButtonPressed(self->id, checkOneBasedIndex(L, 2)));
        return 1;
    }

    int joystickWasReleased(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, joystick_type_name);
        lua_pushboolean(L, input().wasJoystickButtonReleased(self->id, checkOneBasedIndex(L, 2)));
        return 1;
    }

    int joystickGetAxisCount(lua_State* const L)
    {
        lua_pushinteger(L, input().getJoystickAxisCount(checkDevice(L, 1, joystick_type_name)->id));
        return 1;
    }

    int joystickGetAxis(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, joystick_type_name);
        lua_pushnumber(L, input().getJoystickAxis(self->id, checkOneBasedIndex(L, 2)));
        return 1;
    }

    int joystickGetHatCount(lua_State* const L)
    {
        lua_pushinteger(L, input().getJoystickHatCount(checkDevice(L, 1, joystick_type_name)->id));
        return 1;
    }

    int joystickGetHat(lua_State* const L)
    {
        const auto* const self = checkDevice(L, 1, joystick_type_name);
        lua_pushinteger(L, static_cast<lua_Integer>(input().getJoystickHat(self->id, checkOneBasedIndex(L, 2))));
        return 1;
    }

    void registerDeviceType(lua_State* const L, const char* const type_name, const luaL_Reg* const methods, lua_CFunction to_string)
    {
        luaL_register(L, type_name, methods);
        const int method_table = lua_gettop(L);
        luaL_newmetatable(L, type_name);
        lua_pushcfunction(L, to_string);
        lua_setfield(L, -2, "__tostring");
        lua_pushcfunction(L, deviceEquals);
        lua_setfield(L, -2, "__eq");
        lua_pushvalue(L, method_table);
        lua_setfield(L, -2, "__index");
        lua_pop(L, 2);
    }
}

void luastg::binding::Input::Register(lua_State* const L) noexcept
{
    static constexpr luaL_Reg empty[] = { { nullptr, nullptr } };
    luaL_register(L, "lstg.Input", empty);
    lua_pop(L, 1);

    static constexpr std::array keys{
#define KEY(name) std::pair{ std::string_view{ #name }, core::Key::name }
        KEY(unknown),
        KEY(a), KEY(b), KEY(c), KEY(d), KEY(e), KEY(f), KEY(g), KEY(h), KEY(i), KEY(j), KEY(k), KEY(l), KEY(m),
        KEY(n), KEY(o), KEY(p), KEY(q), KEY(r), KEY(s), KEY(t), KEY(u), KEY(v), KEY(w), KEY(x), KEY(y), KEY(z),
        KEY(digit1), KEY(digit2), KEY(digit3), KEY(digit4), KEY(digit5), KEY(digit6), KEY(digit7), KEY(digit8), KEY(digit9), KEY(digit0),
        KEY(enter), KEY(escape), KEY(backspace), KEY(tab), KEY(space), KEY(minus), KEY(equals),
        KEY(left_bracket), KEY(right_bracket), KEY(backslash), KEY(semicolon), KEY(apostrophe), KEY(grave), KEY(comma), KEY(period), KEY(slash),
        KEY(caps_lock), KEY(f1), KEY(f2), KEY(f3), KEY(f4), KEY(f5), KEY(f6), KEY(f7), KEY(f8), KEY(f9), KEY(f10), KEY(f11), KEY(f12),
        KEY(f13), KEY(f14), KEY(f15), KEY(f16), KEY(f17), KEY(f18), KEY(f19), KEY(f20), KEY(f21), KEY(f22), KEY(f23), KEY(f24),
        KEY(print_screen), KEY(scroll_lock), KEY(pause), KEY(insert), KEY(home), KEY(page_up), KEY(delete_key), KEY(end_key), KEY(page_down),
        KEY(right), KEY(left), KEY(down), KEY(up), KEY(num_lock), KEY(keypad_divide), KEY(keypad_multiply), KEY(keypad_minus),
        KEY(keypad_plus), KEY(keypad_enter), KEY(keypad1), KEY(keypad2), KEY(keypad3), KEY(keypad4), KEY(keypad5),
        KEY(keypad6), KEY(keypad7), KEY(keypad8), KEY(keypad9), KEY(keypad0), KEY(keypad_period), KEY(keypad_equals), KEY(keypad_comma),
        KEY(application), KEY(power), KEY(media_next), KEY(media_previous), KEY(media_stop), KEY(media_play), KEY(mute), KEY(volume_up), KEY(volume_down),
        KEY(left_control), KEY(left_shift), KEY(left_alt), KEY(left_super), KEY(right_control), KEY(right_shift), KEY(right_alt), KEY(right_super),
#undef KEY
    };
    static constexpr std::array mouse_buttons{
#define BUTTON(name) std::pair{ std::string_view{ #name }, core::MouseButton::name }
        BUTTON(left), BUTTON(middle), BUTTON(right), BUTTON(x1), BUTTON(x2),
#undef BUTTON
    };
    static constexpr std::array gamepad_buttons{
#define BUTTON(name) std::pair{ std::string_view{ #name }, core::GamepadButton::name }
        BUTTON(south), BUTTON(east), BUTTON(west), BUTTON(north), BUTTON(back), BUTTON(guide), BUTTON(start),
        BUTTON(left_stick), BUTTON(right_stick), BUTTON(left_shoulder), BUTTON(right_shoulder),
        BUTTON(dpad_up), BUTTON(dpad_down), BUTTON(dpad_left), BUTTON(dpad_right), BUTTON(misc1),
        BUTTON(right_paddle1), BUTTON(left_paddle1), BUTTON(right_paddle2), BUTTON(left_paddle2), BUTTON(touchpad),
        BUTTON(misc2), BUTTON(misc3), BUTTON(misc4), BUTTON(misc5), BUTTON(misc6),
#undef BUTTON
    };
    static constexpr std::array gamepad_axes{
#define AXIS(name) std::pair{ std::string_view{ #name }, core::GamepadAxis::name }
        AXIS(left_x), AXIS(left_y), AXIS(right_x), AXIS(right_y), AXIS(left_trigger), AXIS(right_trigger),
#undef AXIS
    };
    static constexpr std::array joystick_hats{
        std::pair{ std::string_view{ "centered" }, core::JoystickHat::centered },
        std::pair{ std::string_view{ "up" }, core::JoystickHat::up },
        std::pair{ std::string_view{ "right" }, core::JoystickHat::right },
        std::pair{ std::string_view{ "down" }, core::JoystickHat::down },
        std::pair{ std::string_view{ "left" }, core::JoystickHat::left },
    };

    registerEnum(L, "lstg.Input.Key", keys);
    registerEnum(L, "lstg.Input.MouseButton", mouse_buttons);
    registerEnum(L, "lstg.Input.GamepadButton", gamepad_buttons);
    registerEnum(L, "lstg.Input.GamepadAxis", gamepad_axes);
    registerEnum(L, "lstg.Input.JoystickHat", joystick_hats);

    static constexpr luaL_Reg keyboard_methods[] = {
        { "isDown", keyboardIsDown },
        { "wasPressed", keyboardWasPressed },
        { "wasReleased", keyboardWasReleased },
        { nullptr, nullptr },
    };
    luaL_register(L, "lstg.Input.Keyboard", keyboard_methods);
    lua_pop(L, 1);

    static constexpr luaL_Reg mouse_methods[] = {
        { "isDown", mouseIsDown },
        { "wasPressed", mouseWasPressed },
        { "wasReleased", mouseWasReleased },
        { "getState", mouseGetState },
        { "getPosition", mouseGetPosition },
        { "getCanvasPosition", mouseGetCanvasPosition },
        { "getMovement", mouseGetMovement },
        { "getWheel", mouseGetWheel },
        { nullptr, nullptr },
    };
    luaL_register(L, "lstg.Input.Mouse", mouse_methods);
    lua_pop(L, 1);

    static constexpr luaL_Reg gamepad_methods[] = {
        { "getAll", gamepadGetAll },
        { "getId", gamepadGetId },
        { "isConnected", gamepadIsConnected },
        { "getName", gamepadGetName },
        { "isDown", gamepadIsDown },
        { "wasPressed", gamepadWasPressed },
        { "wasReleased", gamepadWasReleased },
        { "getAxis", gamepadGetAxis },
        { "getPlayerIndex", gamepadGetPlayerIndex },
        { "setPlayerIndex", gamepadSetPlayerIndex },
        { "rumble", gamepadRumble },
        { nullptr, nullptr },
    };
    registerDeviceType(L, gamepad_type_name, gamepad_methods, gamepadToString);

    static constexpr luaL_Reg joystick_methods[] = {
        { "getAll", joystickGetAll },
        { "getId", joystickGetId },
        { "isConnected", joystickIsConnected },
        { "getName", joystickGetName },
        { "getButtonCount", joystickGetButtonCount },
        { "isDown", joystickIsDown },
        { "wasPressed", joystickWasPressed },
        { "wasReleased", joystickWasReleased },
        { "getAxisCount", joystickGetAxisCount },
        { "getAxis", joystickGetAxis },
        { "getHatCount", joystickGetHatCount },
        { "getHat", joystickGetHat },
        { nullptr, nullptr },
    };
    registerDeviceType(L, joystick_type_name, joystick_methods, joystickToString);
}
