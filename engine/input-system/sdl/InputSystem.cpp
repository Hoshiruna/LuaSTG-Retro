#include "core/InputSystem.hpp"
#include "core/Logger.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>
#include <unordered_map>
#include <utility>

namespace
{
    template<typename Enum>
    constexpr size_t indexOf(const Enum value) noexcept
    {
        return static_cast<size_t>(value);
    }

    core::Key toKey(const SDL_Scancode code) noexcept
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
            MAP_KEY(f13, F13); MAP_KEY(f14, F14); MAP_KEY(f15, F15); MAP_KEY(f16, F16); MAP_KEY(f17, F17); MAP_KEY(f18, F18);
            MAP_KEY(f19, F19); MAP_KEY(f20, F20); MAP_KEY(f21, F21); MAP_KEY(f22, F22); MAP_KEY(f23, F23); MAP_KEY(f24, F24);
            MAP_KEY(print_screen, PRINTSCREEN); MAP_KEY(scroll_lock, SCROLLLOCK); MAP_KEY(pause, PAUSE); MAP_KEY(insert, INSERT);
            MAP_KEY(home, HOME); MAP_KEY(page_up, PAGEUP); MAP_KEY(delete_key, DELETE); MAP_KEY(end_key, END); MAP_KEY(page_down, PAGEDOWN);
            MAP_KEY(right, RIGHT); MAP_KEY(left, LEFT); MAP_KEY(down, DOWN); MAP_KEY(up, UP); MAP_KEY(num_lock, NUMLOCKCLEAR);
            MAP_KEY(keypad_divide, KP_DIVIDE); MAP_KEY(keypad_multiply, KP_MULTIPLY); MAP_KEY(keypad_minus, KP_MINUS);
            MAP_KEY(keypad_plus, KP_PLUS); MAP_KEY(keypad_enter, KP_ENTER); MAP_KEY(keypad1, KP_1); MAP_KEY(keypad2, KP_2);
            MAP_KEY(keypad3, KP_3); MAP_KEY(keypad4, KP_4); MAP_KEY(keypad5, KP_5); MAP_KEY(keypad6, KP_6);
            MAP_KEY(keypad7, KP_7); MAP_KEY(keypad8, KP_8); MAP_KEY(keypad9, KP_9); MAP_KEY(keypad0, KP_0);
            MAP_KEY(keypad_period, KP_PERIOD); MAP_KEY(keypad_equals, KP_EQUALS); MAP_KEY(keypad_comma, KP_COMMA);
            MAP_KEY(application, APPLICATION); MAP_KEY(power, POWER); MAP_KEY(media_next, MEDIA_NEXT_TRACK);
            MAP_KEY(media_previous, MEDIA_PREVIOUS_TRACK); MAP_KEY(media_stop, MEDIA_STOP); MAP_KEY(media_play, MEDIA_PLAY);
            MAP_KEY(mute, MUTE); MAP_KEY(volume_up, VOLUMEUP); MAP_KEY(volume_down, VOLUMEDOWN);
            MAP_KEY(left_control, LCTRL); MAP_KEY(left_shift, LSHIFT); MAP_KEY(left_alt, LALT); MAP_KEY(left_super, LGUI);
            MAP_KEY(right_control, RCTRL); MAP_KEY(right_shift, RSHIFT); MAP_KEY(right_alt, RALT); MAP_KEY(right_super, RGUI);
            default: return core::Key::unknown;
        }
#undef MAP_KEY
    }

    core::MouseButton toMouseButton(const uint8_t button) noexcept
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

    SDL_GamepadButton toSDLButton(const core::GamepadButton button) noexcept
    {
        static constexpr std::array values{
            SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
            SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
            SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
            SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
            SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
            SDL_GAMEPAD_BUTTON_MISC1, SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1, SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
            SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2, SDL_GAMEPAD_BUTTON_LEFT_PADDLE2, SDL_GAMEPAD_BUTTON_TOUCHPAD,
            SDL_GAMEPAD_BUTTON_MISC2, SDL_GAMEPAD_BUTTON_MISC3, SDL_GAMEPAD_BUTTON_MISC4,
            SDL_GAMEPAD_BUTTON_MISC5, SDL_GAMEPAD_BUTTON_MISC6,
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

    float normalizeAxis(const int16_t value) noexcept
    {
        return value < 0 ? static_cast<float>(value) / 32768.0f : static_cast<float>(value) / 32767.0f;
    }

    float normalizeTrigger(const int16_t value) noexcept
    {
        return std::clamp(static_cast<float>(value) / 32767.0f, 0.0f, 1.0f);
    }

    core::JoystickHat toJoystickHat(const uint8_t value) noexcept
    {
        uint8_t result{};
        if((value & SDL_HAT_UP) != 0) result |= static_cast<uint8_t>(core::JoystickHat::up);
        if((value & SDL_HAT_RIGHT) != 0) result |= static_cast<uint8_t>(core::JoystickHat::right);
        if((value & SDL_HAT_DOWN) != 0) result |= static_cast<uint8_t>(core::JoystickHat::down);
        if((value & SDL_HAT_LEFT) != 0) result |= static_cast<uint8_t>(core::JoystickHat::left);
        return static_cast<core::JoystickHat>(result);
    }
}

namespace core
{
    CanvasMousePosition mapMouseToCanvas(
        const Vector2F position,
        const Vector2U window_size,
        const Vector2U canvas_size,
        const CanvasScalingMode scaling_mode,
        const bool flip_y) noexcept
    {
        if(window_size.x == 0 || window_size.y == 0 || canvas_size.x == 0 || canvas_size.y == 0) {
            return {};
        }

        float viewport_x{};
        float viewport_y{};
        float scale_x = static_cast<float>(window_size.x) / static_cast<float>(canvas_size.x);
        float scale_y = static_cast<float>(window_size.y) / static_cast<float>(canvas_size.y);
        if(scaling_mode != CanvasScalingMode::stretch) {
            float scale = std::min(scale_x, scale_y);
            if(scaling_mode == CanvasScalingMode::integer_aspect_ratio && scale >= 1.0f) {
                scale = std::floor(scale);
            }
            scale_x = scale;
            scale_y = scale;
            viewport_x = (static_cast<float>(window_size.x) - scale * static_cast<float>(canvas_size.x)) * 0.5f;
            viewport_y = (static_cast<float>(window_size.y) - scale * static_cast<float>(canvas_size.y)) * 0.5f;
        }

        Vector2F mapped{
            (position.x - viewport_x) / scale_x,
            (position.y - viewport_y) / scale_y,
        };
        const bool inside = mapped.x >= 0.0f && mapped.x < static_cast<float>(canvas_size.x)
            && mapped.y >= 0.0f && mapped.y < static_cast<float>(canvas_size.y);
        if(flip_y) {
            mapped.y = static_cast<float>(canvas_size.y) - mapped.y;
        }
        return { mapped, inside };
    }

    struct InputSystem::Impl
    {
        template<size_t Size>
        struct ButtonState
        {
            std::array<bool, Size> current{};
            std::array<bool, Size> pending_pressed{};
            std::array<bool, Size> pending_released{};
            std::array<bool, Size> pressed{};
            std::array<bool, Size> released{};

            void publish() noexcept
            {
                pressed = pending_pressed;
                released = pending_released;
                pending_pressed.fill(false);
                pending_released.fill(false);
            }

            void set(const size_t index, const bool down) noexcept
            {
                if(down != current[index]) {
                    if(down) {
                        pending_pressed[index] = true;
                    } else {
                        pending_released[index] = true;
                    }
                    current[index] = down;
                }
            }

            void releaseAll() noexcept
            {
                for(size_t i = 0; i < current.size(); ++i) {
                    if(current[i]) {
                        current[i] = false;
                        pending_released[i] = true;
                    }
                }
            }

            void clear() noexcept
            {
                current.fill(false);
                pending_pressed.fill(false);
                pending_released.fill(false);
                pressed.fill(false);
                released.fill(false);
            }
        };

        struct Gamepad
        {
            SDL_Gamepad* handle{};
            std::string name;
            ButtonState<indexOf(GamepadButton::count)> buttons;
            std::array<int16_t, indexOf(GamepadAxis::count)> axes{};
        };

        struct Joystick
        {
            SDL_Joystick* handle{};
            std::string name;
            std::vector<bool> buttons;
            std::vector<bool> pending_pressed;
            std::vector<bool> pending_released;
            std::vector<bool> pressed;
            std::vector<bool> released;
            std::vector<int16_t> axes;
            std::vector<uint8_t> hats;
        };

        ButtonState<indexOf(Key::count)> keys;
        ButtonState<indexOf(MouseButton::count)> mouse_buttons;
        MouseState raw_mouse;
        MouseState frame_mouse;
        float pending_delta_x{};
        float pending_delta_y{};
        float pending_wheel_x{};
        float pending_wheel_y{};
        bool capture_keyboard{};
        bool capture_mouse{};
        bool initialized{};
        std::unordered_map<uint32_t, Gamepad> gamepads;
        std::unordered_map<uint32_t, Joystick> joysticks;

        void addGamepad(const SDL_JoystickID id)
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

        void addJoystick(const SDL_JoystickID id)
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
                joystick.hats[i] = SDL_GetJoystickHat(handle, static_cast<int>(i));
            }
            joysticks.emplace(id, std::move(joystick));
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

        bool isJoystickPresent(const uint32_t id) const
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
    };

    InputSystem::InputSystem()
        : m_impl(std::make_unique<Impl>())
    {
    }

    InputSystem::~InputSystem()
    {
        shutdown();
    }

    bool InputSystem::initialize()
    {
        if(m_impl->initialized) {
            return true;
        }

        constexpr SDL_InitFlags required_subsystems = SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;
        if((SDL_WasInit(required_subsystems) & required_subsystems) != required_subsystems) {
            Logger::error("[sdl] InputSystem::initialize requires the SDL events, joystick, and gamepad subsystems");
            return false;
        }

        int scancode_count{};
        if(const bool* const keyboard = SDL_GetKeyboardState(&scancode_count); keyboard != nullptr) {
            for(int i = 0; i < scancode_count; ++i) {
                const Key key = toKey(static_cast<SDL_Scancode>(i));
                if(key != Key::unknown) {
                    m_impl->keys.current[indexOf(key)] = keyboard[i];
                }
            }
        }

        const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(&m_impl->raw_mouse.x, &m_impl->raw_mouse.y);
        static constexpr std::array sdl_mouse_buttons{
            SDL_BUTTON_LEFT,
            SDL_BUTTON_MIDDLE,
            SDL_BUTTON_RIGHT,
            SDL_BUTTON_X1,
            SDL_BUTTON_X2,
        };
        for(size_t i = 0; i < sdl_mouse_buttons.size(); ++i) {
            m_impl->mouse_buttons.current[i] = (mouse_buttons & SDL_BUTTON_MASK(sdl_mouse_buttons[i])) != 0;
        }
        m_impl->frame_mouse = m_impl->raw_mouse;

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
        m_impl->initialized = true;
        return true;
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
        m_impl->keys.clear();
        m_impl->mouse_buttons.clear();
        m_impl->raw_mouse = {};
        m_impl->frame_mouse = {};
        m_impl->pending_delta_x = 0.0f;
        m_impl->pending_delta_y = 0.0f;
        m_impl->pending_wheel_x = 0.0f;
        m_impl->pending_wheel_y = 0.0f;
        m_impl->capture_keyboard = false;
        m_impl->capture_mouse = false;
        m_impl->initialized = false;
    }

    InputSystem& InputSystem::getInstance()
    {
        static InputSystem instance;
        return instance;
    }

    void InputSystem::processEvent(const SDL_Event& event)
    {
        switch(event.type) {
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: {
                const Key key = toKey(event.key.scancode);
                if(key != Key::unknown) {
                    const bool down = event.type == SDL_EVENT_KEY_DOWN;
                    if(!down || !event.key.repeat) {
                        m_impl->keys.set(indexOf(key), down);
                    }
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                m_impl->raw_mouse.x = event.button.x;
                m_impl->raw_mouse.y = event.button.y;
                const MouseButton button = toMouseButton(event.button.button);
                if(button != MouseButton::count) {
                    m_impl->mouse_buttons.set(indexOf(button), event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                m_impl->raw_mouse.x = event.motion.x;
                m_impl->raw_mouse.y = event.motion.y;
                m_impl->pending_delta_x += event.motion.xrel;
                m_impl->pending_delta_y += event.motion.yrel;
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                m_impl->raw_mouse.x = event.wheel.mouse_x;
                m_impl->raw_mouse.y = event.wheel.mouse_y;
                const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                m_impl->pending_wheel_x += event.wheel.x * direction;
                m_impl->pending_wheel_y += event.wheel.y * direction;
                break;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                resetKeyboardAndMouse();
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                m_impl->addGamepad(event.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                m_impl->removeGamepad(event.gdevice.which);
                if(m_impl->isJoystickPresent(event.gdevice.which)) {
                    m_impl->addJoystick(event.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMAPPED:
                m_impl->removeGamepad(event.gdevice.which);
                if(SDL_IsGamepad(event.gdevice.which)) {
                    m_impl->addGamepad(event.gdevice.which);
                } else if(m_impl->isJoystickPresent(event.gdevice.which)) {
                    m_impl->addJoystick(event.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
                if(const auto it = m_impl->gamepads.find(event.gdevice.which); it != m_impl->gamepads.end()) {
                    const char* const name = SDL_GetGamepadName(it->second.handle);
                    it->second.name = name != nullptr ? name : "";
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                if(const auto it = m_impl->gamepads.find(event.gbutton.which); it != m_impl->gamepads.end()) {
                    const GamepadButton button = toGamepadButton(static_cast<SDL_GamepadButton>(event.gbutton.button));
                    if(button != GamepadButton::count) {
                        it->second.buttons.set(indexOf(button), event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                    }
                }
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                if(const auto it = m_impl->gamepads.find(event.gaxis.which); it != m_impl->gamepads.end()) {
                    const GamepadAxis axis = toGamepadAxis(static_cast<SDL_GamepadAxis>(event.gaxis.axis));
                    if(axis != GamepadAxis::count) {
                        it->second.axes[indexOf(axis)] = event.gaxis.value;
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_ADDED:
                m_impl->addJoystick(event.jdevice.which);
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                m_impl->removeJoystick(event.jdevice.which);
                m_impl->removeGamepad(event.jdevice.which);
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                if(const auto it = m_impl->joysticks.find(event.jbutton.which); it != m_impl->joysticks.end()) {
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
                if(const auto it = m_impl->joysticks.find(event.jaxis.which); it != m_impl->joysticks.end()) {
                    if(event.jaxis.axis < it->second.axes.size()) {
                        it->second.axes[event.jaxis.axis] = event.jaxis.value;
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
                if(const auto it = m_impl->joysticks.find(event.jhat.which); it != m_impl->joysticks.end()) {
                    if(event.jhat.hat < it->second.hats.size()) {
                        it->second.hats[event.jhat.hat] = event.jhat.value;
                    }
                }
                break;
            default:
                break;
        }
    }

    void InputSystem::update()
    {
        m_impl->keys.publish();
        m_impl->mouse_buttons.publish();
        m_impl->frame_mouse = m_impl->raw_mouse;
        m_impl->frame_mouse.delta_x = std::exchange(m_impl->pending_delta_x, 0.0f);
        m_impl->frame_mouse.delta_y = std::exchange(m_impl->pending_delta_y, 0.0f);
        m_impl->frame_mouse.wheel_x = std::exchange(m_impl->pending_wheel_x, 0.0f);
        m_impl->frame_mouse.wheel_y = std::exchange(m_impl->pending_wheel_y, 0.0f);
        for(auto& entry : m_impl->gamepads) {
            entry.second.buttons.publish();
        }
        for(auto& entry : m_impl->joysticks) {
            entry.second.pressed = entry.second.pending_pressed;
            entry.second.released = entry.second.pending_released;
            std::fill(entry.second.pending_pressed.begin(), entry.second.pending_pressed.end(), false);
            std::fill(entry.second.pending_released.begin(), entry.second.pending_released.end(), false);
        }
    }

    void InputSystem::resetKeyboardAndMouse()
    {
        m_impl->keys.releaseAll();
        m_impl->mouse_buttons.releaseAll();
        m_impl->pending_delta_x = 0.0f;
        m_impl->pending_delta_y = 0.0f;
        m_impl->pending_wheel_x = 0.0f;
        m_impl->pending_wheel_y = 0.0f;
    }

    void InputSystem::setGameplayCapture(const bool keyboard, const bool mouse) noexcept
    {
        m_impl->capture_keyboard = keyboard;
        m_impl->capture_mouse = mouse;
    }

    bool InputSystem::isRawKeyDown(const Key key) const noexcept
    {
        return key < Key::count && m_impl->keys.current[indexOf(key)];
    }

    bool InputSystem::isKeyDown(const Key key) const noexcept
    {
        return !m_impl->capture_keyboard && isRawKeyDown(key);
    }

    bool InputSystem::wasRawKeyPressed(const Key key) const noexcept
    {
        return key < Key::count && m_impl->keys.pressed[indexOf(key)];
    }

    bool InputSystem::wasKeyPressed(const Key key) const noexcept
    {
        return !m_impl->capture_keyboard && wasRawKeyPressed(key);
    }

    bool InputSystem::wasRawKeyReleased(const Key key) const noexcept
    {
        return key < Key::count && m_impl->keys.released[indexOf(key)];
    }

    bool InputSystem::wasKeyReleased(const Key key) const noexcept
    {
        return !m_impl->capture_keyboard && wasRawKeyReleased(key);
    }

    bool InputSystem::isRawMouseButtonDown(const MouseButton button) const noexcept
    {
        return button < MouseButton::count && m_impl->mouse_buttons.current[indexOf(button)];
    }

    bool InputSystem::isMouseButtonDown(const MouseButton button) const noexcept
    {
        return !m_impl->capture_mouse && isRawMouseButtonDown(button);
    }

    bool InputSystem::wasMouseButtonPressed(const MouseButton button) const noexcept
    {
        return !m_impl->capture_mouse && button < MouseButton::count && m_impl->mouse_buttons.pressed[indexOf(button)];
    }

    bool InputSystem::wasMouseButtonReleased(const MouseButton button) const noexcept
    {
        return !m_impl->capture_mouse && button < MouseButton::count && m_impl->mouse_buttons.released[indexOf(button)];
    }

    MouseState InputSystem::getRawMouseState() const noexcept
    {
        return m_impl->frame_mouse;
    }

    MouseState InputSystem::getMouseState() const noexcept
    {
        MouseState result = m_impl->frame_mouse;
        if(m_impl->capture_mouse) {
            result.delta_x = 0.0f;
            result.delta_y = 0.0f;
            result.wheel_x = 0.0f;
            result.wheel_y = 0.0f;
        }
        return result;
    }

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

    bool InputSystem::isGamepadConnected(const uint32_t id) const noexcept
    {
        return m_impl->gamepads.contains(id);
    }

    std::string InputSystem::getGamepadName(const uint32_t id) const
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() ? it->second.name : std::string{};
    }

    bool InputSystem::isGamepadButtonDown(const uint32_t id, const GamepadButton button) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() && button < GamepadButton::count && it->second.buttons.current[indexOf(button)];
    }

    bool InputSystem::wasGamepadButtonPressed(const uint32_t id, const GamepadButton button) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() && button < GamepadButton::count && it->second.buttons.pressed[indexOf(button)];
    }

    bool InputSystem::wasGamepadButtonReleased(const uint32_t id, const GamepadButton button) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() && button < GamepadButton::count && it->second.buttons.released[indexOf(button)];
    }

    float InputSystem::getGamepadAxis(const uint32_t id, const GamepadAxis axis) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        if(it == m_impl->gamepads.end() || axis >= GamepadAxis::count) {
            return 0.0f;
        }
        const int16_t value = it->second.axes[indexOf(axis)];
        return axis == GamepadAxis::left_trigger || axis == GamepadAxis::right_trigger ? normalizeTrigger(value) : normalizeAxis(value);
    }

    int32_t InputSystem::getGamepadPlayerIndex(const uint32_t id) const noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        return it != m_impl->gamepads.end() ? SDL_GetGamepadPlayerIndex(it->second.handle) : -1;
    }

    bool InputSystem::setGamepadPlayerIndex(const uint32_t id, const int32_t player_index) noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        if(it == m_impl->gamepads.end()) {
            return false;
        }
        return SDL_SetGamepadPlayerIndex(it->second.handle, player_index);
    }

    bool InputSystem::rumbleGamepad(const uint32_t id, const float low, const float high, const uint32_t duration_ms) noexcept
    {
        const auto it = m_impl->gamepads.find(id);
        if(it == m_impl->gamepads.end()) {
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

    bool InputSystem::isJoystickConnected(const uint32_t id) const noexcept
    {
        return m_impl->joysticks.contains(id);
    }

    std::string InputSystem::getJoystickName(const uint32_t id) const
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? it->second.name : std::string{};
    }

    uint32_t InputSystem::getJoystickButtonCount(const uint32_t id) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? static_cast<uint32_t>(it->second.buttons.size()) : 0;
    }

    bool InputSystem::isJoystickButtonDown(const uint32_t id, const uint32_t button) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && button < it->second.buttons.size() && it->second.buttons[button];
    }

    bool InputSystem::wasJoystickButtonPressed(const uint32_t id, const uint32_t button) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && button < it->second.pressed.size() && it->second.pressed[button];
    }

    bool InputSystem::wasJoystickButtonReleased(const uint32_t id, const uint32_t button) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && button < it->second.released.size() && it->second.released[button];
    }

    uint32_t InputSystem::getJoystickAxisCount(const uint32_t id) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? static_cast<uint32_t>(it->second.axes.size()) : 0;
    }

    float InputSystem::getJoystickAxis(const uint32_t id, const uint32_t axis) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && axis < it->second.axes.size() ? normalizeAxis(it->second.axes[axis]) : 0.0f;
    }

    uint32_t InputSystem::getJoystickHatCount(const uint32_t id) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() ? static_cast<uint32_t>(it->second.hats.size()) : 0;
    }

    JoystickHat InputSystem::getJoystickHat(const uint32_t id, const uint32_t hat) const noexcept
    {
        const auto it = m_impl->joysticks.find(id);
        return it != m_impl->joysticks.end() && hat < it->second.hats.size() ? toJoystickHat(it->second.hats[hat]) : JoystickHat::centered;
    }
}
