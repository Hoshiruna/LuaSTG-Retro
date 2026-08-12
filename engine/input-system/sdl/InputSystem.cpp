#include "core/InputSystem.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <unordered_map>

namespace
{
    template<typename Enum>
    constexpr size_t indexOf(const Enum value) noexcept
    {
        return static_cast<size_t>(value);
    }

    core::Key toKey(const SDL_Scancode code)
    {
#define MAP_KEY(name, sdl_name) case SDL_SCANCODE_##sdl_name: return core::Key::name
        switch(code) {
            MAP_KEY(a, A); MAP_KEY(b, B); MAP_KEY(c, C); MAP_KEY(d, D); MAP_KEY(e, E); MAP_KEY(f, F); MAP_KEY(g, G);
            MAP_KEY(h, H); MAP_KEY(i, I); MAP_KEY(j, J); MAP_KEY(k, K); MAP_KEY(l, L); MAP_KEY(m, M); MAP_KEY(n, N);
            MAP_KEY(o, O); MAP_KEY(p, P); MAP_KEY(q, Q); MAP_KEY(r, R); MAP_KEY(s, S); MAP_KEY(t, T); MAP_KEY(u, U);
            MAP_KEY(v, V); MAP_KEY(w, W); MAP_KEY(x, X); MAP_KEY(y, Y); MAP_KEY(z, Z);
            MAP_KEY(digit1, 1); MAP_KEY(digit2, 2); MAP_KEY(digit3, 3); MAP_KEY(digit4, 4); MAP_KEY(digit5, 5);
            MAP_KEY(digit6, 6); MAP_KEY(digit7, 7); MAP_KEY(digit8, 8); MAP_KEY(digit9, 9); MAP_KEY(digit0, 0);
            MAP_KEY(enter, RETURN); MAP_KEY(escape, ESCAPE); MAP_KEY(backspace, BACKSPACE); MAP_KEY(tab, TAB); MAP_KEY(space, SPACE);
            MAP_KEY(minus, MINUS); MAP_KEY(equals, EQUALS); MAP_KEY(left_bracket, LEFTBRACKET); MAP_KEY(right_bracket, RIGHTBRACKET);
            MAP_KEY(backslash, BACKSLASH); MAP_KEY(semicolon, SEMICOLON); MAP_KEY(apostrophe, APOSTROPHE); MAP_KEY(grave, GRAVE);
            MAP_KEY(comma, COMMA); MAP_KEY(period, PERIOD); MAP_KEY(slash, SLASH); MAP_KEY(caps_lock, CAPSLOCK);
            MAP_KEY(f1, F1); MAP_KEY(f2, F2); MAP_KEY(f3, F3); MAP_KEY(f4, F4); MAP_KEY(f5, F5); MAP_KEY(f6, F6);
            MAP_KEY(f7, F7); MAP_KEY(f8, F8); MAP_KEY(f9, F9); MAP_KEY(f10, F10); MAP_KEY(f11, F11); MAP_KEY(f12, F12);
            MAP_KEY(print_screen, PRINTSCREEN); MAP_KEY(scroll_lock, SCROLLLOCK); MAP_KEY(pause, PAUSE); MAP_KEY(insert, INSERT);
            MAP_KEY(home, HOME); MAP_KEY(page_up, PAGEUP); MAP_KEY(delete_key, DELETE); MAP_KEY(end, END); MAP_KEY(page_down, PAGEDOWN);
            MAP_KEY(right, RIGHT); MAP_KEY(left, LEFT); MAP_KEY(down, DOWN); MAP_KEY(up, UP); MAP_KEY(num_lock, NUMLOCKCLEAR);
            MAP_KEY(keypad_divide, KP_DIVIDE); MAP_KEY(keypad_multiply, KP_MULTIPLY); MAP_KEY(keypad_minus, KP_MINUS);
            MAP_KEY(keypad_plus, KP_PLUS); MAP_KEY(keypad_enter, KP_ENTER); MAP_KEY(keypad1, KP_1); MAP_KEY(keypad2, KP_2);
            MAP_KEY(keypad3, KP_3); MAP_KEY(keypad4, KP_4); MAP_KEY(keypad5, KP_5); MAP_KEY(keypad6, KP_6);
            MAP_KEY(keypad7, KP_7); MAP_KEY(keypad8, KP_8); MAP_KEY(keypad9, KP_9); MAP_KEY(keypad0, KP_0);
            MAP_KEY(keypad_period, KP_PERIOD); MAP_KEY(left_control, LCTRL); MAP_KEY(left_shift, LSHIFT); MAP_KEY(left_alt, LALT);
            MAP_KEY(left_super, LGUI); MAP_KEY(right_control, RCTRL); MAP_KEY(right_shift, RSHIFT); MAP_KEY(right_alt, RALT);
            MAP_KEY(right_super, RGUI);
            default: return core::Key::unknown;
        }
#undef MAP_KEY
    }

    core::MouseButton toMouseButton(const uint8_t button)
    {
        switch(button) {
            case SDL_BUTTON_LEFT: return core::MouseButton::left;
            case SDL_BUTTON_MIDDLE: return core::MouseButton::middle;
            case SDL_BUTTON_RIGHT: return core::MouseButton::right;
            case SDL_BUTTON_X1: return core::MouseButton::x1;
            case SDL_BUTTON_X2: return core::MouseButton::x2;
            default: return core::MouseButton::count;
        }
    }

    SDL_GamepadButton toSDLButton(const core::GamepadButton button)
    {
        static constexpr std::array values{
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
            SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_MISC1, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
            SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, SDL_GAMEPAD_BUTTON_TOUCHPAD,
        };
        const size_t index = indexOf(button);
        return index < values.size() ? values[index] : SDL_GAMEPAD_BUTTON_INVALID;
    }

    core::GamepadButton toGamepadButton(const SDL_GamepadButton button)
    {
        for(size_t i = 0; i < indexOf(core::GamepadButton::count); ++i) {
            const auto candidate = static_cast<core::GamepadButton>(i);
            if(toSDLButton(candidate) == button) {
                return candidate;
            }
        }
        return core::GamepadButton::count;
    }

    SDL_GamepadAxis toSDLAxis(const core::GamepadAxis axis)
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

    float normalizeAxis(const int16_t value) noexcept
    {
        return value < 0 ? static_cast<float>(value) / 32768.0f : static_cast<float>(value) / 32767.0f;
    }

    float normalizeTrigger(const int16_t value) noexcept
    {
        return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
    }
}

namespace core
{
    struct InputSystem::Impl
    {
        struct Gamepad
        {
            SDL_Gamepad* handle{};
            std::string name;
            std::array<bool, indexOf(GamepadButton::count)> pressed{};
            std::array<bool, indexOf(GamepadButton::count)> released{};
        };

        struct Joystick
        {
            SDL_Joystick* handle{};
            std::string name;
        };

        std::array<bool, indexOf(Key::count)> keys{};
        std::array<bool, indexOf(Key::count)> key_pressed{};
        std::array<bool, indexOf(Key::count)> key_released{};
        Key last_pressed_key{ Key::unknown };
        std::array<bool, indexOf(MouseButton::count)> mouse_buttons{};
        std::array<bool, indexOf(MouseButton::count)> mouse_pressed{};
        std::array<bool, indexOf(MouseButton::count)> mouse_released{};
        MouseState mouse;
        std::unordered_map<uint32_t, Gamepad> gamepads;
        std::unordered_map<uint32_t, Joystick> joysticks;

        void addGamepad(const SDL_JoystickID id)
        {
            if(gamepads.contains(id)) {
                return;
            }
            SDL_Gamepad* const handle = SDL_OpenGamepad(id);
            if(handle == nullptr) {
                Logger::error("[sdl] SDL_OpenGamepad failed: {}", SDL_GetError());
                return;
            }
            const char* const name = SDL_GetGamepadName(handle);
            gamepads.emplace(id, Gamepad{ handle, name != nullptr ? name : "" });
        }

        void addJoystick(const SDL_JoystickID id)
        {
            if(joysticks.contains(id) || SDL_IsGamepad(id)) {
                return;
            }
            SDL_Joystick* const handle = SDL_OpenJoystick(id);
            if(handle == nullptr) {
                Logger::error("[sdl] SDL_OpenJoystick failed: {}", SDL_GetError());
                return;
            }
            const char* const name = SDL_GetJoystickName(handle);
            joysticks.emplace(id, Joystick{ handle, name != nullptr ? name : "" });
        }

        void removeGamepad(const uint32_t id)
        {
            if(const auto it = gamepads.find(id); it != gamepads.end()) {
                SDL_CloseGamepad(it->second.handle);
                gamepads.erase(it);
            }
        }

        void removeJoystick(const uint32_t id)
        {
            if(const auto it = joysticks.find(id); it != joysticks.end()) {
                SDL_CloseJoystick(it->second.handle);
                joysticks.erase(it);
            }
        }
    };

    InputSystem::InputSystem()
        : m_impl(std::make_unique<Impl>())
    {
        int count{};
        if(SDL_JoystickID* ids = SDL_GetGamepads(&count); ids != nullptr) {
            for(int i = 0; i < count; ++i) {
                m_impl->addGamepad(ids[i]);
            }
            SDL_free(ids);
        }
        if(SDL_JoystickID* ids = SDL_GetJoysticks(&count); ids != nullptr) {
            for(int i = 0; i < count; ++i) {
                m_impl->addJoystick(ids[i]);
            }
            SDL_free(ids);
        }
    }

    InputSystem::~InputSystem()
    {
        shutdown();
    }

    void InputSystem::shutdown()
    {
        for(auto& entry : m_impl->gamepads) {
            SDL_CloseGamepad(entry.second.handle);
        }
        m_impl->gamepads.clear();
        for(auto& entry : m_impl->joysticks) {
            SDL_CloseJoystick(entry.second.handle);
        }
        m_impl->joysticks.clear();
    }

    InputSystem& InputSystem::getInstance()
    {
        static InputSystem instance;
        return instance;
    }

    void InputSystem::beginFrame()
    {
        m_impl->key_pressed.fill(false);
        m_impl->key_released.fill(false);
        m_impl->last_pressed_key = Key::unknown;
        m_impl->mouse_pressed.fill(false);
        m_impl->mouse_released.fill(false);
        m_impl->mouse.delta_x = 0.0f;
        m_impl->mouse.delta_y = 0.0f;
        m_impl->mouse.wheel_x = 0.0f;
        m_impl->mouse.wheel_y = 0.0f;
        for(auto& entry : m_impl->gamepads) {
            entry.second.pressed.fill(false);
            entry.second.released.fill(false);
        }
    }

    void InputSystem::processEvent(const SDL_Event& event)
    {
        switch(event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const Key key = toKey(event.key.scancode);
                if(key == Key::unknown) {
                    break;
                }
                const size_t index = indexOf(key);
                const bool down = event.type == SDL_EVENT_KEY_DOWN;
                if(down && !event.key.repeat) {
                    m_impl->key_pressed[index] = true;
                    m_impl->last_pressed_key = key;
                } else if(!down) {
                    m_impl->key_released[index] = true;
                }
                m_impl->keys[index] = down;
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const MouseButton button = toMouseButton(event.button.button);
                if(button == MouseButton::count) {
                    break;
                }
                const size_t index = indexOf(button);
                const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                (down ? m_impl->mouse_pressed : m_impl->mouse_released)[index] = true;
                m_impl->mouse_buttons[index] = down;
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                m_impl->mouse.x = event.motion.x;
                m_impl->mouse.y = event.motion.y;
                m_impl->mouse.delta_x += event.motion.xrel;
                m_impl->mouse.delta_y += event.motion.yrel;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                m_impl->mouse.wheel_x += event.wheel.x;
                m_impl->mouse.wheel_y += event.wheel.y;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                resetKeyboardAndMouse();
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                m_impl->addGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                m_impl->removeGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if(const auto it = m_impl->gamepads.find(event.gbutton.which); it != m_impl->gamepads.end()) {
                    const GamepadButton button = toGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button));
                    if(button != GamepadButton::count) {
                        const bool down = event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
                        (down ? it->second.pressed : it->second.released)[indexOf(button)] = true;
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_ADDED:
                m_impl->addJoystick(event.jdevice.which);
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                m_impl->removeJoystick(event.jdevice.which);
                break;
            default:
                break;
        }
    }

    void InputSystem::resetKeyboardAndMouse()
    {
        m_impl->keys.fill(false);
        m_impl->last_pressed_key = Key::unknown;
        m_impl->mouse_buttons.fill(false);
        m_impl->mouse.delta_x = 0.0f;
        m_impl->mouse.delta_y = 0.0f;
        m_impl->mouse.wheel_x = 0.0f;
        m_impl->mouse.wheel_y = 0.0f;
    }

    bool InputSystem::isKeyDown(const Key key) const noexcept { return key < Key::count && m_impl->keys[indexOf(key)]; }
    bool InputSystem::wasKeyPressed(const Key key) const noexcept { return key < Key::count && m_impl->key_pressed[indexOf(key)]; }
    bool InputSystem::wasKeyReleased(const Key key) const noexcept { return key < Key::count && m_impl->key_released[indexOf(key)]; }
    Key InputSystem::getLastPressedKey() const noexcept { return m_impl->last_pressed_key; }
    bool InputSystem::isMouseButtonDown(const MouseButton button) const noexcept { return button < MouseButton::count && m_impl->mouse_buttons[indexOf(button)]; }
    bool InputSystem::wasMouseButtonPressed(const MouseButton button) const noexcept { return button < MouseButton::count && m_impl->mouse_pressed[indexOf(button)]; }
    bool InputSystem::wasMouseButtonReleased(const MouseButton button) const noexcept { return button < MouseButton::count && m_impl->mouse_released[indexOf(button)]; }
    MouseState InputSystem::getMouseState() const noexcept { return m_impl->mouse; }

    std::vector<InputDeviceInfo> InputSystem::getGamepads() const
    {
        std::vector<InputDeviceInfo> result;
        result.reserve(m_impl->gamepads.size());
        for(const auto& [id, gamepad] : m_impl->gamepads) {
            result.push_back({ id, gamepad.name });
        }
        std::ranges::sort(result, {}, &InputDeviceInfo::id);
        return result;
    }

    bool InputSystem::isGamepadConnected(const uint32_t id) const noexcept { return m_impl->gamepads.contains(id); }
    bool InputSystem::isGamepadButtonDown(const uint32_t id, const GamepadButton button) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() && button < GamepadButton::count && SDL_GetGamepadButton(it->second.handle, toSDLButton(button));
    }
    bool InputSystem::wasGamepadButtonPressed(const uint32_t id, const GamepadButton button) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() && button < GamepadButton::count && it->second.pressed[indexOf(button)];
    }
    bool InputSystem::wasGamepadButtonReleased(const uint32_t id, const GamepadButton button) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() && button < GamepadButton::count && it->second.released[indexOf(button)];
    }
    float InputSystem::getGamepadAxis(const uint32_t id, const GamepadAxis axis) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        if(it == m_impl->gamepads.end() || axis >= GamepadAxis::count) {
            return 0.0f;
        }
        const int16_t value = SDL_GetGamepadAxis(it->second.handle, toSDLAxis(axis));
        return axis == GamepadAxis::left_trigger || axis == GamepadAxis::right_trigger ? normalizeTrigger(value) : normalizeAxis(value);
    }
    int32_t InputSystem::getGamepadPlayerIndex(const uint32_t id) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() ? SDL_GetGamepadPlayerIndex(it->second.handle) : -1;
    }
    bool InputSystem::rumbleGamepad(const uint32_t id, const float low, const float high, const uint32_t duration_ms) noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        if(it == m_impl->gamepads.end()) {
            return false;
        }
        const auto toMagnitude = [](const float value) { return static_cast<uint16_t>(std::clamp(value, 0.0f, 1.0f) * 65535.0f); };
        return SDL_RumbleGamepad(it->second.handle, toMagnitude(low), toMagnitude(high), duration_ms);
    }

    std::vector<InputDeviceInfo> InputSystem::getJoysticks() const
    {
        std::vector<InputDeviceInfo> result;
        result.reserve(m_impl->joysticks.size());
        for(const auto& [id, joystick] : m_impl->joysticks) {
            result.push_back({ id, joystick.name });
        }
        std::ranges::sort(result, {}, &InputDeviceInfo::id);
        return result;
    }
    bool InputSystem::isJoystickConnected(const uint32_t id) const noexcept { return m_impl->joysticks.contains(id); }
    uint32_t InputSystem::getJoystickButtonCount(const uint32_t id) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? static_cast<uint32_t>(SDL_GetNumJoystickButtons(it->second.handle)) : 0;
    }
    bool InputSystem::isJoystickButtonDown(const uint32_t id, const uint32_t button) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && button < getJoystickButtonCount(id) && SDL_GetJoystickButton(it->second.handle, static_cast<int>(button));
    }
    uint32_t InputSystem::getJoystickAxisCount(const uint32_t id) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? static_cast<uint32_t>(SDL_GetNumJoystickAxes(it->second.handle)) : 0;
    }
    float InputSystem::getJoystickAxis(const uint32_t id, const uint32_t axis) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && axis < getJoystickAxisCount(id) ? normalizeAxis(SDL_GetJoystickAxis(it->second.handle, static_cast<int>(axis))) : 0.0f;
    }
    uint32_t InputSystem::getJoystickHatCount(const uint32_t id) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? static_cast<uint32_t>(SDL_GetNumJoystickHats(it->second.handle)) : 0;
    }
    uint8_t InputSystem::getJoystickHat(const uint32_t id, const uint32_t hat) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && hat < getJoystickHatCount(id) ? SDL_GetJoystickHat(it->second.handle, static_cast<int>(hat)) : SDL_HAT_CENTERED;
    }
}
