#include "core/ControllerInput.hpp"
#include "core/Logger.hpp"
#include <GameInput.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <utility>

namespace
{
    using namespace GameInput::v4;
    using Microsoft::WRL::ComPtr;
    using core::GamepadButton;
    using core::JoystickHat;
    using core::indexOf;
    using Clock = std::chrono::steady_clock;

    const GameInputKind controller_kinds = GameInputKindGamepad | GameInputKindController;

    constexpr std::array gamepad_buttons{
        std::pair{ GamepadButton::south, GameInputGamepadA },
        std::pair{ GamepadButton::east, GameInputGamepadB },
        std::pair{ GamepadButton::west, GameInputGamepadX },
        std::pair{ GamepadButton::north, GameInputGamepadY },
        std::pair{ GamepadButton::back, GameInputGamepadView },
        std::pair{ GamepadButton::start, GameInputGamepadMenu },
        std::pair{ GamepadButton::left_stick, GameInputGamepadLeftThumbstick },
        std::pair{ GamepadButton::right_stick, GameInputGamepadRightThumbstick },
        std::pair{ GamepadButton::left_shoulder, GameInputGamepadLeftShoulder },
        std::pair{ GamepadButton::right_shoulder, GameInputGamepadRightShoulder },
        std::pair{ GamepadButton::dpad_up, GameInputGamepadDPadUp },
        std::pair{ GamepadButton::dpad_down, GameInputGamepadDPadDown },
        std::pair{ GamepadButton::dpad_left, GameInputGamepadDPadLeft },
        std::pair{ GamepadButton::dpad_right, GameInputGamepadDPadRight },
        std::pair{ GamepadButton::right_paddle1, GameInputGamepadPaddleRight1 },
        std::pair{ GamepadButton::left_paddle1, GameInputGamepadPaddleLeft1 },
        std::pair{ GamepadButton::right_paddle2, GameInputGamepadPaddleRight2 },
        std::pair{ GamepadButton::left_paddle2, GameInputGamepadPaddleLeft2 },
    };

    float clampUnit(const float value) noexcept
    {
        return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
    }

    int16_t toAxis(const float value) noexcept
    {
        const float axis = std::clamp(value, -1.0f, 1.0f);
        return std::isfinite(axis) ? static_cast<int16_t>(axis * (axis < 0.0f ? 32768.0f : 32767.0f)) : 0;
    }

    JoystickHat toHat(const GameInputSwitchPosition value) noexcept
    {
        constexpr uint8_t up = static_cast<uint8_t>(JoystickHat::up);
        constexpr uint8_t right = static_cast<uint8_t>(JoystickHat::right);
        constexpr uint8_t down = static_cast<uint8_t>(JoystickHat::down);
        constexpr uint8_t left = static_cast<uint8_t>(JoystickHat::left);
        switch(value) {
            case GameInputSwitchUp: return JoystickHat::up;
            case GameInputSwitchUpRight: return static_cast<JoystickHat>(up | right);
            case GameInputSwitchRight: return JoystickHat::right;
            case GameInputSwitchDownRight: return static_cast<JoystickHat>(down | right);
            case GameInputSwitchDown: return JoystickHat::down;
            case GameInputSwitchDownLeft: return static_cast<JoystickHat>(down | left);
            case GameInputSwitchLeft: return JoystickHat::left;
            case GameInputSwitchUpLeft: return static_cast<JoystickHat>(up | left);
            default: return JoystickHat::centered;
        }
    }
}

namespace core
{
    struct ControllerInput::GameInputState
    {
        struct Device
        {
            ComPtr<IGameInputDevice> handle;
            ComPtr<IGameInputReading> reading;
            ComPtr<IGameInputReading> system_reading;
            uint32_t id{};
            int32_t player_index{ -1 };
            bool is_gamepad{};
            bool tracks_system_buttons{};
            Clock::time_point rumble_end{};
            std::unique_ptr<bool[]> raw_buttons;
            std::vector<float> raw_axes;
            std::vector<GameInputSwitchPosition> raw_switches;
        };

        struct DeviceEvent
        {
            ComPtr<IGameInputDevice> handle;
            bool connected;
        };

        ComPtr<IGameInput> input;
        GameInputCallbackToken device_callback{};
        std::mutex events_mutex;
        std::vector<DeviceEvent> events;
        std::unordered_map<IGameInputDevice*, Device> devices;
        uint32_t next_id{ 1 };

        static void CALLBACK deviceChanged(GameInputCallbackToken, void* context, const GameInputDeviceEvent event, IGameInputDevice* device)
        {
            if(event == GameInputDeviceEventConnected || event == GameInputDeviceEventDisconnected) {
                auto& state = *static_cast<GameInputState*>(context);
                const std::lock_guard lock(state.events_mutex);
                state.events.push_back({ ComPtr<IGameInputDevice>(device), event == GameInputDeviceEventConnected });
            }
        }

        template<typename ApplyReading>
        void read(Device& device, ComPtr<IGameInputReading>& previous, const GameInputKind kind, const bool initial, ApplyReading apply)
        {
            ComPtr<IGameInputReading> current;
            if(FAILED(input->GetCurrentReading(kind, device.handle.Get(), current.GetAddressOf())) || current.Get() == previous.Get()) {
                return;
            }
            if(previous) {
                // Capture both edges when a button is pressed and released between updates.
                while(previous.Get() != current.Get()) {
                    ComPtr<IGameInputReading> next;
                    if(FAILED(input->GetNextReading(previous.Get(), kind, device.handle.Get(), next.GetAddressOf()))) {
                        break;
                    }
                    if(next->GetTimestamp() > current->GetTimestamp()) {
                        break;
                    }
                    apply(*next.Get(), false);
                    previous = std::move(next);
                }
                if(previous.Get() == current.Get()) {
                    return;
                }
            }
            // Seed new devices and resynchronize if the previous reading expired.
            apply(*current.Get(), initial);
            previous = std::move(current);
        }

        void readGamepad(Device& device, Gamepad& gamepad, const bool initial)
        {
            const auto set_button = [&gamepad](const GamepadButton button, const bool down, const bool seed) {
                if(seed) {
                    gamepad.buttons.current[indexOf(button)] = down;
                } else {
                    gamepad.buttons.set(indexOf(button), down);
                }
            };
            read(device, device.reading, GameInputKindGamepad, initial, [&](IGameInputReading& reading, const bool seed) {
                GameInputGamepadState state{};
                if(!reading.GetGamepadState(&state)) {
                    return;
                }
                for(const auto& [button, mask] : gamepad_buttons) {
                    set_button(button, (state.buttons & mask) != 0, seed);
                }
                gamepad.axes = { toAxis(state.leftThumbstickX), toAxis(-state.leftThumbstickY), toAxis(state.rightThumbstickX), toAxis(-state.rightThumbstickY), toAxis(clampUnit(state.leftTrigger)), toAxis(clampUnit(state.rightTrigger)) };
            });
            if(device.tracks_system_buttons) {
                read(device, device.system_reading, GameInputKindSystemButton, initial, [&](IGameInputReading& reading, const bool seed) {
                    GameInputSystemButtons buttons{};
                    if(reading.GetSystemButtonState(&buttons)) {
                        set_button(GamepadButton::guide, (buttons & GameInputSystemButtonGuide) != 0, seed);
                        set_button(GamepadButton::misc1, (buttons & GameInputSystemButtonShare) != 0, seed);
                    }
                });
            }
        }

        void readJoystick(Device& device, Joystick& joystick, const bool initial)
        {
            read(device, device.reading, GameInputKindController, initial, [&](IGameInputReading& reading, const bool seed) {
                const uint32_t buttons = joystick.buttons.empty() ? 0 : reading.GetControllerButtonState(static_cast<uint32_t>(joystick.buttons.size()), device.raw_buttons.get());
                for(uint32_t i = 0; i < buttons; ++i) {
                    const bool down = device.raw_buttons[i];
                    if(!seed && down != joystick.buttons[i]) {
                        (down ? joystick.pending_pressed : joystick.pending_released)[i] = true;
                    }
                    joystick.buttons[i] = down;
                }
                const uint32_t axes = device.raw_axes.empty() ? 0 : reading.GetControllerAxisState(static_cast<uint32_t>(device.raw_axes.size()), device.raw_axes.data());
                for(uint32_t i = 0; i < axes; ++i) {
                    joystick.axes[i] = toAxis(device.raw_axes[i] * 2.0f - 1.0f);
                }
                const uint32_t switches = device.raw_switches.empty() ? 0 : reading.GetControllerSwitchState(static_cast<uint32_t>(device.raw_switches.size()), device.raw_switches.data());
                for(uint32_t i = 0; i < switches; ++i) {
                    joystick.hats[i] = toHat(device.raw_switches[i]);
                }
            });
        }

        void addDevice(ControllerInput& controllers, const ComPtr<IGameInputDevice>& handle)
        {
            if(devices.contains(handle.Get()) || !handle->IsConnected()) {
                return;
            }
            const GameInputKind supported = handle->GetSupportedInput();
            if((supported & controller_kinds) == 0) {
                return;
            }

            Device device;
            device.handle = handle;
            device.id = next_id++;
            device.is_gamepad = (supported & GameInputKindGamepad) != 0;
            std::string name(handle->GetDisplayName(0, nullptr), '\0');
            if(!name.empty()) {
                handle->GetDisplayName(static_cast<uint32_t>(name.size()), name.data());
                name.resize(std::min(name.find('\0'), name.size()));
            }

            if(device.is_gamepad) {
                uint32_t player_index{};
                if(handle->GetXInputDeviceIndex(&player_index)) {
                    device.player_index = static_cast<int32_t>(player_index);
                }
                device.tracks_system_buttons = handle->GetSupportedSystemButtons() != GameInputSystemButtonNone;
                Gamepad& gamepad = controllers.gamepads[device.id];
                gamepad.name = std::move(name);
                readGamepad(device, gamepad, true);
            } else {
                const uint32_t buttons = handle->GetControllerButtonCount();
                const uint32_t axes = handle->GetControllerAxisCount();
                const uint32_t switches = handle->GetControllerSwitchCount();
                device.raw_buttons = std::make_unique<bool[]>(buttons);
                device.raw_axes.resize(axes);
                device.raw_switches.resize(switches);
                Joystick& joystick = controllers.joysticks[device.id];
                joystick.name = std::move(name);
                joystick.buttons.resize(buttons);
                joystick.pending_pressed.resize(buttons);
                joystick.pending_released.resize(buttons);
                joystick.pressed.resize(buttons);
                joystick.released.resize(buttons);
                joystick.axes.resize(axes);
                joystick.hats.resize(switches);
                readJoystick(device, joystick, true);
            }
            devices.emplace(handle.Get(), std::move(device));
        }

        Device* findGamepad(const uint32_t id) noexcept
        {
            for(auto& [handle, device] : devices) {
                if(device.id == id && device.is_gamepad) {
                    return &device;
                }
            }
            return nullptr;
        }
    };

    ControllerInput::ControllerInput() = default;
    ControllerInput::~ControllerInput() = default;

    bool ControllerInput::initialize()
    {
        auto state = std::make_unique<GameInputState>();
        HRESULT result = GameInputCreate(state->input.GetAddressOf());
        if(FAILED(result)) {
            Logger::error("[gameinput] GameInputCreate failed (0x{:08X}). Check the GameInput runtime.",
                static_cast<uint32_t>(result));
            return false;
        }
        result = state->input->RegisterDeviceCallback(nullptr, controller_kinds, state.get(), GameInputState::deviceChanged, &state->device_callback);
        if(FAILED(result)) {
            Logger::error("[gameinput] RegisterDeviceCallback failed (0x{:08X})", static_cast<uint32_t>(result));
            return false;
        }
        m_game_input = std::move(state);
        update();
        return true;
    }

    void ControllerInput::shutdown()
    {
        if(m_game_input) {
            m_game_input->input->UnregisterCallback(m_game_input->device_callback);
            for(auto& [handle, device] : m_game_input->devices) {
                if(device.rumble_end != Clock::time_point{}) {
                    handle->SetRumbleState(nullptr);
                }
            }
            m_game_input.reset();
        }
        gamepads.clear();
        joysticks.clear();
    }

    void ControllerInput::processEvent(const SDL_Event&)
    {
    }

    void ControllerInput::update()
    {
        if(!m_game_input) {
            return;
        }
        GameInputState& state = *m_game_input;
        std::vector<GameInputState::DeviceEvent> events;
        {
            const std::lock_guard lock(state.events_mutex);
            events.swap(state.events);
        }
        for(const auto& event : events) {
            if(event.connected) {
                state.addDevice(*this, event.handle);
            } else if(const auto it = state.devices.find(event.handle.Get()); it != state.devices.end()) {
                gamepads.erase(it->second.id);
                joysticks.erase(it->second.id);
                state.devices.erase(it);
            }
        }
        const auto now = Clock::now();
        for(auto& [handle, device] : state.devices) {
            if(device.is_gamepad) {
                state.readGamepad(device, gamepads.at(device.id), false);
                if(device.rumble_end != Clock::time_point{} && now >= device.rumble_end) {
                    handle->SetRumbleState(nullptr);
                    device.rumble_end = {};
                }
            } else {
                state.readJoystick(device, joysticks.at(device.id), false);
            }
        }
    }

    int32_t ControllerInput::getGamepadPlayerIndex(const uint32_t id) const noexcept
    {
        const auto* device = m_game_input ? m_game_input->findGamepad(id) : nullptr;
        return device ? device->player_index : -1;
    }

    bool ControllerInput::setGamepadPlayerIndex(const uint32_t id, const int32_t player_index) noexcept
    {
        auto* device = m_game_input ? m_game_input->findGamepad(id) : nullptr;
        if(!device) {
            return false;
        }
        device->player_index = player_index;
        return true;
    }

    bool ControllerInput::rumbleGamepad(const uint32_t id, const float low, const float high, const uint32_t duration_ms) noexcept
    {
        auto* device = m_game_input ? m_game_input->findGamepad(id) : nullptr;
        const auto motors = GameInputRumbleLowFrequency | GameInputRumbleHighFrequency;
        if(!device || !device->handle->IsConnected() || (device->handle->GetSupportedRumbleMotors() & motors) == 0) {
            return false;
        }

        const GameInputRumbleParams params{ clampUnit(low), clampUnit(high), 0.0f, 0.0f };
        if(duration_ms == 0 || (params.lowFrequency == 0.0f && params.highFrequency == 0.0f)) {
            device->handle->SetRumbleState(nullptr);
            device->rumble_end = {};
        } else {
            device->handle->SetRumbleState(&params);
            device->rumble_end = Clock::now() + std::chrono::milliseconds(duration_ms);
        }
        return true;
    }
}
