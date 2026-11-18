#include "core/ControllerInput.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
    using core::indexOf;

    SDL_GamepadButton toSDLButton(const core::GamepadButton button) noexcept
    {
        static constexpr std::array values{
            SDL_GAMEPAD_BUTTON_SOUTH,
            SDL_GAMEPAD_BUTTON_EAST,
            SDL_GAMEPAD_BUTTON_WEST,
            SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_BACK,
            SDL_GAMEPAD_BUTTON_GUIDE,
            SDL_GAMEPAD_BUTTON_START,
            SDL_GAMEPAD_BUTTON_LEFT_STICK,
            SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
            SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_DPAD_UP,
            SDL_GAMEPAD_BUTTON_DPAD_DOWN,
            SDL_GAMEPAD_BUTTON_DPAD_LEFT,
            SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_MISC1,
            SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
            SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
            SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
            SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
            SDL_GAMEPAD_BUTTON_TOUCHPAD,
            SDL_GAMEPAD_BUTTON_MISC2,
            SDL_GAMEPAD_BUTTON_MISC3,
            SDL_GAMEPAD_BUTTON_MISC4,
            SDL_GAMEPAD_BUTTON_MISC5,
            SDL_GAMEPAD_BUTTON_MISC6,
        };
        const size_t index = indexOf(button);
        return index < values.size() ? values[index] : SDL_GAMEPAD_BUTTON_INVALID;
    }

    core::GamepadButton toGamepadButton(const SDL_GamepadButton button) noexcept
    {
        for(size_t i = 0; i < indexOf(core::GamepadButton::count); ++i) {
            const auto candidate = static_cast<core::GamepadButton>(i);
            if(toSDLButton(candidate) == button) {
                return candidate;
            }
        }
        return core::GamepadButton::count;
    }

    SDL_GamepadAxis toSDLAxis(const core::GamepadAxis axis) noexcept
    {
        static constexpr std::array values{
            SDL_GAMEPAD_AXIS_LEFTX,
            SDL_GAMEPAD_AXIS_LEFTY,
            SDL_GAMEPAD_AXIS_RIGHTX,
            SDL_GAMEPAD_AXIS_RIGHTY,
            SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
            SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
        };
        const size_t index = indexOf(axis);
        return index < values.size() ? values[index] : SDL_GAMEPAD_AXIS_INVALID;
    }

    core::GamepadAxis toGamepadAxis(const SDL_GamepadAxis axis) noexcept
    {
        for(size_t i = 0; i < indexOf(core::GamepadAxis::count); ++i) {
            const auto candidate = static_cast<core::GamepadAxis>(i);
            if(toSDLAxis(candidate) == axis) {
                return candidate;
            }
        }
        return core::GamepadAxis::count;
    }

    core::JoystickHat toJoystickHat(const uint8_t value) noexcept
    {
        uint8_t result{};
        if((value & SDL_HAT_UP) != 0)
            result |= static_cast<uint8_t>(core::JoystickHat::up);
        if((value & SDL_HAT_RIGHT) != 0)
            result |= static_cast<uint8_t>(core::JoystickHat::right);
        if((value & SDL_HAT_DOWN) != 0)
            result |= static_cast<uint8_t>(core::JoystickHat::down);
        if((value & SDL_HAT_LEFT) != 0)
            result |= static_cast<uint8_t>(core::JoystickHat::left);
        return static_cast<core::JoystickHat>(result);
    }
}

namespace core
{
    ControllerInput::ControllerInput() = default;
    ControllerInput::~ControllerInput() = default;

    bool ControllerInput::initialize()
    {
        constexpr SDL_InitFlags required_subsystems = SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
        if((SDL_WasInit(required_subsystems) & required_subsystems) != required_subsystems) {
            Logger::error("[sdl] ControllerInput::initialize requires the SDL joystick and gamepad subsystems");
            return false;
        }

        int count{};
        if(SDL_JoystickID* ids = SDL_GetGamepads(&count); ids != nullptr) {
            for(int i = 0; i < count; ++i) {
                addGamepad(ids[i]);
            }
            SDL_free(ids);
        }
        if(SDL_JoystickID* ids = SDL_GetJoysticks(&count); ids != nullptr) {
            for(int i = 0; i < count; ++i) {
                addJoystick(ids[i]);
            }
            SDL_free(ids);
        }
        return true;
    }

    void ControllerInput::shutdown()
    {
        for(auto& entry : gamepads) {
            SDL_CloseGamepad(entry.second.handle);
        }
        gamepads.clear();
        for(auto& entry : joysticks) {
            SDL_CloseJoystick(entry.second.handle);
        }
        joysticks.clear();
    }

    void ControllerInput::update()
    {
    }

    void ControllerInput::addGamepad(const SDL_JoystickID id)
    {
        if(gamepads.contains(id)) {
            return;
        }
        removeJoystick(id);
        SDL_Gamepad* const handle = SDL_OpenGamepad(id);
        if(handle == nullptr) {
            Logger::error("[sdl] SDL_OpenGamepad({}) failed: {}", id, SDL_GetError());
            return;
        }
        const char* const name = SDL_GetGamepadName(handle);
        Gamepad gamepad{ handle, name != nullptr ? name : "" };
        for(size_t i = 0; i < gamepad.buttons.current.size(); ++i) {
            gamepad.buttons.current[i] = SDL_GetGamepadButton(handle, toSDLButton(static_cast<GamepadButton>(i)));
        }
        for(size_t i = 0; i < gamepad.axes.size(); ++i) {
            gamepad.axes[i] = SDL_GetGamepadAxis(handle, toSDLAxis(static_cast<GamepadAxis>(i)));
        }
        gamepads.emplace(id, std::move(gamepad));
    }

    void ControllerInput::addJoystick(const SDL_JoystickID id)
    {
        if(joysticks.contains(id) || gamepads.contains(id) || SDL_IsGamepad(id)) {
            return;
        }
        SDL_Joystick* const handle = SDL_OpenJoystick(id);
        if(handle == nullptr) {
            Logger::error("[sdl] SDL_OpenJoystick({}) failed: {}", id, SDL_GetError());
            return;
        }
        const char* const name = SDL_GetJoystickName(handle);
        Joystick joystick{ handle, name != nullptr ? name : "" };
        joystick.buttons.resize(static_cast<size_t>(std::max(SDL_GetNumJoystickButtons(handle), 0)));
        joystick.pending_pressed.resize(joystick.buttons.size());
        joystick.pending_released.resize(joystick.buttons.size());
        joystick.pressed.resize(joystick.buttons.size());
        joystick.released.resize(joystick.buttons.size());
        joystick.axes.resize(static_cast<size_t>(std::max(SDL_GetNumJoystickAxes(handle), 0)));
        joystick.hats.resize(static_cast<size_t>(std::max(SDL_GetNumJoystickHats(handle), 0)));
        for(size_t i = 0; i < joystick.buttons.size(); ++i) {
            joystick.buttons[i] = SDL_GetJoystickButton(handle, static_cast<int>(i));
        }
        for(size_t i = 0; i < joystick.axes.size(); ++i) {
            joystick.axes[i] = SDL_GetJoystickAxis(handle, static_cast<int>(i));
        }
        for(size_t i = 0; i < joystick.hats.size(); ++i) {
            joystick.hats[i] = toJoystickHat(SDL_GetJoystickHat(handle, static_cast<int>(i)));
        }
        joysticks.emplace(id, std::move(joystick));
    }

    void ControllerInput::removeGamepad(const uint32_t id)
    {
        if(const auto it = gamepads.find(id); it != gamepads.end()) {
            SDL_CloseGamepad(it->second.handle);
            gamepads.erase(it);
        }
    }

    void ControllerInput::removeJoystick(const uint32_t id)
    {
        if(const auto it = joysticks.find(id); it != joysticks.end()) {
            SDL_CloseJoystick(it->second.handle);
            joysticks.erase(it);
        }
    }

    bool ControllerInput::isJoystickPresent(const uint32_t id) const
    {
        int count{};
        SDL_JoystickID* const ids = SDL_GetJoysticks(&count);
        if(ids == nullptr) {
            return false;
        }
        const bool found = std::find(ids, ids + count, id) != ids + count;
        SDL_free(ids);
        return found;
    }

    void ControllerInput::processEvent(const SDL_Event& event)
    {
        switch(event.type) {
            case SDL_EVENT_GAMEPAD_ADDED:
                addGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                removeGamepad(event.gdevice.which);
                if(isJoystickPresent(event.gdevice.which)) {
                    addJoystick(event.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMAPPED:
                removeGamepad(event.gdevice.which);
                if(SDL_IsGamepad(event.gdevice.which)) {
                    addGamepad(event.gdevice.which);
                } else if(isJoystickPresent(event.gdevice.which)) {
                    addJoystick(event.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
                if(const auto it = gamepads.find(event.gdevice.which); it != gamepads.end()) {
                    const char* const name = SDL_GetGamepadName(it->second.handle);
                    it->second.name = name != nullptr ? name : "";
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if(const auto it = gamepads.find(event.gbutton.which); it != gamepads.end()) {
                    const GamepadButton button = toGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button));
                    if(button != GamepadButton::count) {
                        it->second.buttons.set(indexOf(button), event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if(const auto it = gamepads.find(event.gaxis.which); it != gamepads.end()) {
                    const GamepadAxis axis = toGamepadAxis(static_cast<SDL_GamepadAxis>(event.gaxis.axis));
                    if(axis != GamepadAxis::count) {
                        it->second.axes[indexOf(axis)] = event.gaxis.value;
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_ADDED:
                addJoystick(event.jdevice.which);
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                removeJoystick(event.jdevice.which);
                removeGamepad(event.jdevice.which);
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                if(const auto it = joysticks.find(event.jbutton.which); it != joysticks.end()) {
                    if(event.jbutton.button < it->second.buttons.size()) {
                        const bool down = event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN;
                        if(down != it->second.buttons[event.jbutton.button]) {
                            if(down) {
                                it->second.pending_pressed[event.jbutton.button] = true;
                            } else {
                                it->second.pending_released[event.jbutton.button] = true;
                            }
                            it->second.buttons[event.jbutton.button] = down;
                        }
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                if(const auto it = joysticks.find(event.jaxis.which); it != joysticks.end()) {
                    if(event.jaxis.axis < it->second.axes.size()) {
                        it->second.axes[event.jaxis.axis] = event.jaxis.value;
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
                if(const auto it = joysticks.find(event.jhat.which); it != joysticks.end()) {
                    if(event.jhat.hat < it->second.hats.size()) {
                        it->second.hats[event.jhat.hat] = toJoystickHat(event.jhat.value);
                    }
                }
                break;
            default:
                break;
        }
    }

    int32_t ControllerInput::getGamepadPlayerIndex(const uint32_t id) const noexcept
    {
        const auto it = gamepads.find(id);
        return it != gamepads.end() ? SDL_GetGamepadPlayerIndex(it->second.handle) : -1;
    }

    bool ControllerInput::setGamepadPlayerIndex(const uint32_t id, const int32_t player_index) noexcept
    {
        const auto it = gamepads.find(id);
        if(it == gamepads.end()) {
            return false;
        }
        return SDL_SetGamepadPlayerIndex(it->second.handle, player_index);
    }

    bool ControllerInput::rumbleGamepad(const uint32_t id, const float low, const float high, const uint32_t duration_ms) noexcept
    {
        const auto it = gamepads.find(id);
        if(it == gamepads.end()) {
            return false;
        }
        const SDL_PropertiesID properties = SDL_GetGamepadProperties(it->second.handle);
        if(properties == 0 || !SDL_GetBooleanProperty(properties, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN, false)) {
            return false;
        }
        const auto toMagnitude = [](const float value) {
            const float finite_value = std::isfinite(value) ? value : 0.0f;
            return static_cast<uint16_t>(std::clamp(finite_value, 0.0f, 1.0f) * 65535.0f);
        };
        return SDL_RumbleGamepad(it->second.handle, toMagnitude(low), toMagnitude(high), duration_ms);
    }

}
