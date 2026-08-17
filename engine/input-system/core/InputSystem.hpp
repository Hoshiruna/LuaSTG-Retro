#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "core/Vector2.hpp"

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
        f13, f14, f15, f16, f17, f18, f19, f20, f21, f22, f23, f24,
        print_screen, scroll_lock, pause, insert, home, page_up, delete_key, end_key, page_down,
        right, left, down, up,
        num_lock, keypad_divide, keypad_multiply, keypad_minus, keypad_plus, keypad_enter,
        keypad1, keypad2, keypad3, keypad4, keypad5, keypad6, keypad7, keypad8, keypad9, keypad0,
        keypad_period, keypad_equals, keypad_comma,
        application, power,
        media_next, media_previous, media_stop, media_play, mute, volume_up, volume_down,
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
        misc2,
        misc3,
        misc4,
        misc5,
        misc6,
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

    enum class JoystickHat : uint8_t
    {
        centered = 0,
        up = 1 << 0,
        right = 1 << 1,
        down = 1 << 2,
        left = 1 << 3,
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

    enum class CanvasScalingMode : uint8_t
    {
        stretch,
        aspect_ratio,
        integer_aspect_ratio,
    };

    struct CanvasMousePosition
    {
        Vector2F position{};
        bool inside{};
    };

    CanvasMousePosition mapMouseToCanvas(
        Vector2F position,
        Vector2U window_size,
        Vector2U canvas_size,
        CanvasScalingMode scaling_mode,
        bool flip_y = true) noexcept;

    class IInputSystem
    {
    public:
        virtual ~IInputSystem() = default;

        virtual bool initialize() = 0;
        virtual void shutdown() = 0;
        virtual void processEvent(const SDL_Event& event) = 0;
        virtual void update() = 0;
        virtual void resetKeyboardAndMouse() = 0;
        virtual void setGameplayCapture(bool keyboard, bool mouse) noexcept = 0;

        virtual bool isKeyDown(Key key) const noexcept = 0;
        virtual bool wasKeyPressed(Key key) const noexcept = 0;
        virtual bool wasKeyReleased(Key key) const noexcept = 0;
        virtual bool isMouseButtonDown(MouseButton button) const noexcept = 0;
        virtual bool wasMouseButtonPressed(MouseButton button) const noexcept = 0;
        virtual bool wasMouseButtonReleased(MouseButton button) const noexcept = 0;
        virtual MouseState getMouseState() const noexcept = 0;

        virtual std::vector<InputDeviceInfo> getGamepads() const = 0;
        virtual bool isGamepadConnected(uint32_t id) const noexcept = 0;
        virtual std::string getGamepadName(uint32_t id) const = 0;
        virtual bool isGamepadButtonDown(uint32_t id, GamepadButton button) const noexcept = 0;
        virtual bool wasGamepadButtonPressed(uint32_t id, GamepadButton button) const noexcept = 0;
        virtual bool wasGamepadButtonReleased(uint32_t id, GamepadButton button) const noexcept = 0;
        virtual float getGamepadAxis(uint32_t id, GamepadAxis axis) const noexcept = 0;
        virtual int32_t getGamepadPlayerIndex(uint32_t id) const noexcept = 0;
        virtual bool setGamepadPlayerIndex(uint32_t id, int32_t player_index) noexcept = 0;
        virtual bool rumbleGamepad(uint32_t id, float low_frequency, float high_frequency, uint32_t duration_ms) noexcept = 0;

        virtual std::vector<InputDeviceInfo> getJoysticks() const = 0;
        virtual bool isJoystickConnected(uint32_t id) const noexcept = 0;
        virtual std::string getJoystickName(uint32_t id) const = 0;
        virtual uint32_t getJoystickButtonCount(uint32_t id) const noexcept = 0;
        virtual bool isJoystickButtonDown(uint32_t id, uint32_t button) const noexcept = 0;
        virtual bool wasJoystickButtonPressed(uint32_t id, uint32_t button) const noexcept = 0;
        virtual bool wasJoystickButtonReleased(uint32_t id, uint32_t button) const noexcept = 0;
        virtual uint32_t getJoystickAxisCount(uint32_t id) const noexcept = 0;
        virtual float getJoystickAxis(uint32_t id, uint32_t axis) const noexcept = 0;
        virtual uint32_t getJoystickHatCount(uint32_t id) const noexcept = 0;
        virtual JoystickHat getJoystickHat(uint32_t id, uint32_t hat) const noexcept = 0;
    };

    class InputSystem final : public IInputSystem
    {
    public:
        InputSystem();
        InputSystem(const InputSystem&) = delete;
        InputSystem(InputSystem&&) = delete;
        ~InputSystem() override;

        InputSystem& operator=(const InputSystem&) = delete;
        InputSystem& operator=(InputSystem&&) = delete;

        static InputSystem& getInstance();

        bool initialize() override;
        void shutdown() override;
        void processEvent(const SDL_Event& event) override;
        void update() override;
        void resetKeyboardAndMouse() override;
        void setGameplayCapture(bool keyboard, bool mouse) noexcept override;

        bool isKeyDown(Key key) const noexcept override;
        bool wasKeyPressed(Key key) const noexcept override;
        bool wasKeyReleased(Key key) const noexcept override;
        bool isMouseButtonDown(MouseButton button) const noexcept override;
        bool wasMouseButtonPressed(MouseButton button) const noexcept override;
        bool wasMouseButtonReleased(MouseButton button) const noexcept override;
        MouseState getMouseState() const noexcept override;

        std::vector<InputDeviceInfo> getGamepads() const override;
        bool isGamepadConnected(uint32_t id) const noexcept override;
        std::string getGamepadName(uint32_t id) const override;
        bool isGamepadButtonDown(uint32_t id, GamepadButton button) const noexcept override;
        bool wasGamepadButtonPressed(uint32_t id, GamepadButton button) const noexcept override;
        bool wasGamepadButtonReleased(uint32_t id, GamepadButton button) const noexcept override;
        float getGamepadAxis(uint32_t id, GamepadAxis axis) const noexcept override;
        int32_t getGamepadPlayerIndex(uint32_t id) const noexcept override;
        bool setGamepadPlayerIndex(uint32_t id, int32_t player_index) noexcept override;
        bool rumbleGamepad(uint32_t id, float low_frequency, float high_frequency, uint32_t duration_ms) noexcept override;

        std::vector<InputDeviceInfo> getJoysticks() const override;
        bool isJoystickConnected(uint32_t id) const noexcept override;
        std::string getJoystickName(uint32_t id) const override;
        uint32_t getJoystickButtonCount(uint32_t id) const noexcept override;
        bool isJoystickButtonDown(uint32_t id, uint32_t button) const noexcept override;
        bool wasJoystickButtonPressed(uint32_t id, uint32_t button) const noexcept override;
        bool wasJoystickButtonReleased(uint32_t id, uint32_t button) const noexcept override;
        uint32_t getJoystickAxisCount(uint32_t id) const noexcept override;
        float getJoystickAxis(uint32_t id, uint32_t axis) const noexcept override;
        uint32_t getJoystickHatCount(uint32_t id) const noexcept override;
        JoystickHat getJoystickHat(uint32_t id, uint32_t hat) const noexcept override;

        bool isRawKeyDown(Key key) const noexcept;
        bool wasRawKeyPressed(Key key) const noexcept;
        bool wasRawKeyReleased(Key key) const noexcept;
        bool isRawMouseButtonDown(MouseButton button) const noexcept;
        MouseState getRawMouseState() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
