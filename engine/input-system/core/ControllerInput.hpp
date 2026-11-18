#pragma once

#include "core/InputSystem.hpp"
#include <array>
#include <unordered_map>

struct SDL_Gamepad;
struct SDL_Joystick;

namespace core
{
    template<typename Enum>
    constexpr size_t indexOf(const Enum value) noexcept
    {
        return static_cast<size_t>(value);
    }

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

    class ControllerInput
    {
    public:
        struct Gamepad
        {
#ifndef LUASTG_USE_GAMEINPUT
            SDL_Gamepad* handle{};
#endif
            std::string name;
            ButtonState<indexOf(GamepadButton::count)> buttons;
            std::array<int16_t, indexOf(GamepadAxis::count)> axes{};
        };

        struct Joystick
        {
#ifndef LUASTG_USE_GAMEINPUT
            SDL_Joystick* handle{};
#endif
            std::string name;
            std::vector<bool> buttons;
            std::vector<bool> pending_pressed;
            std::vector<bool> pending_released;
            std::vector<bool> pressed;
            std::vector<bool> released;
            std::vector<int16_t> axes;
            std::vector<JoystickHat> hats;
        };

        std::unordered_map<uint32_t, Gamepad> gamepads;
        std::unordered_map<uint32_t, Joystick> joysticks;

        ControllerInput();
        ~ControllerInput();
        bool initialize();
        void shutdown();
        void processEvent(const SDL_Event& event);
        void update();

        int32_t getGamepadPlayerIndex(uint32_t id) const noexcept;
        bool setGamepadPlayerIndex(uint32_t id, int32_t player_index) noexcept;
        bool rumbleGamepad(uint32_t id, float low, float high, uint32_t duration_ms) noexcept;

    private:
#ifdef LUASTG_USE_GAMEINPUT
        struct GameInputState;
        std::unique_ptr<GameInputState> m_game_input;
#else
        void addGamepad(uint32_t id);
        void addJoystick(uint32_t id);
        void removeGamepad(uint32_t id);
        void removeJoystick(uint32_t id);
        bool isJoystickPresent(uint32_t id) const;
#endif
    };
}
