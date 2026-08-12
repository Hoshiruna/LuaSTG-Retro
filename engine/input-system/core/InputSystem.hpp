#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

union SDL_Event;

namespace core
{
    enum class Key : uint16_t
    {
        unknown,
        a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
        digit1, digit2, digit3, digit4, digit5, digit6, digit7, digit8, digit9, digit0,
        enter, escape, backspace, tab, space,
        minus, equals, left_bracket, right_bracket, backslash, semicolon, apostrophe, grave, comma, period, slash,
        caps_lock,
        f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12,
        print_screen, scroll_lock, pause, insert, home, page_up, delete_key, end, page_down,
        right, left, down, up,
        num_lock, keypad_divide, keypad_multiply, keypad_minus, keypad_plus, keypad_enter,
        keypad1, keypad2, keypad3, keypad4, keypad5, keypad6, keypad7, keypad8, keypad9, keypad0, keypad_period,
        left_control, left_shift, left_alt, left_super,
        right_control, right_shift, right_alt, right_super,
        count,
    };

    enum class MouseButton : uint8_t
    {
        left,
        middle,
        right,
        x1,
        x2,
        count,
    };

    enum class GamepadButton : uint8_t
    {
        south,
        east,
        west,
        north,
        back,
        guide,
        start,
        left_stick,
        right_stick,
        left_shoulder,
        right_shoulder,
        dpad_up,
        dpad_down,
        dpad_left,
        dpad_right,
        misc1,
        right_paddle1,
        left_paddle1,
        right_paddle2,
        left_paddle2,
        touchpad,
        count,
    };

    enum class GamepadAxis : uint8_t
    {
        left_x,
        left_y,
        right_x,
        right_y,
        left_trigger,
        right_trigger,
        count,
    };

    struct MouseState
    {
        float x{};
        float y{};
        float delta_x{};
        float delta_y{};
        float wheel_x{};
        float wheel_y{};
    };

    struct InputDeviceInfo
    {
        uint32_t id{};
        std::string name;
    };

    class InputSystem
    {
    public:
        InputSystem();
        InputSystem(const InputSystem&) = delete;
        InputSystem(InputSystem&&) = delete;
        ~InputSystem();

        InputSystem& operator=(const InputSystem&) = delete;
        InputSystem& operator=(InputSystem&&) = delete;

        static InputSystem& getInstance();

        void shutdown();
        void beginFrame();
        void processEvent(const SDL_Event& event);
        void resetKeyboardAndMouse();

        bool isKeyDown(Key key) const noexcept;
        bool wasKeyPressed(Key key) const noexcept;
        bool wasKeyReleased(Key key) const noexcept;
        Key getLastPressedKey() const noexcept;
        bool isMouseButtonDown(MouseButton button) const noexcept;
        bool wasMouseButtonPressed(MouseButton button) const noexcept;
        bool wasMouseButtonReleased(MouseButton button) const noexcept;
        MouseState getMouseState() const noexcept;

        std::vector<InputDeviceInfo> getGamepads() const;
        bool isGamepadConnected(uint32_t id) const noexcept;
        bool isGamepadButtonDown(uint32_t id, GamepadButton button) const noexcept;
        bool wasGamepadButtonPressed(uint32_t id, GamepadButton button) const noexcept;
        bool wasGamepadButtonReleased(uint32_t id, GamepadButton button) const noexcept;
        float getGamepadAxis(uint32_t id, GamepadAxis axis) const noexcept;
        int32_t getGamepadPlayerIndex(uint32_t id) const noexcept;
        bool rumbleGamepad(uint32_t id, float low_frequency, float high_frequency, uint32_t duration_ms) noexcept;

        std::vector<InputDeviceInfo> getJoysticks() const;
        bool isJoystickConnected(uint32_t id) const noexcept;
        uint32_t getJoystickButtonCount(uint32_t id) const noexcept;
        bool isJoystickButtonDown(uint32_t id, uint32_t button) const noexcept;
        uint32_t getJoystickAxisCount(uint32_t id) const noexcept;
        float getJoystickAxis(uint32_t id, uint32_t axis) const noexcept;
        uint32_t getJoystickHatCount(uint32_t id) const noexcept;
        uint8_t getJoystickHat(uint32_t id, uint32_t hat) const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
