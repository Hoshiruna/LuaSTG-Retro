#include "AppFrame.h"
#include "core/InputSystem.hpp"
#include <cmath>

namespace
{
    core::Key keyFromLegacyCode(const int code) noexcept
    {
        if(code >= 0x41 && code <= 0x5A) {
            return static_cast<core::Key>(static_cast<int>(core::Key::a) + code - 0x41);
        }
        if(code >= 0x31 && code <= 0x39) {
            return static_cast<core::Key>(static_cast<int>(core::Key::digit1) + code - 0x31);
        }
        switch(code) {
            case 0x30: return core::Key::digit0;
            case 0x0D: return core::Key::enter;
            case 0x1B: return core::Key::escape;
            case 0x08: return core::Key::backspace;
            case 0x09: return core::Key::tab;
            case 0x20: return core::Key::space;
            case 0x21: return core::Key::page_up;
            case 0x22: return core::Key::page_down;
            case 0x23: return core::Key::end;
            case 0x24: return core::Key::home;
            case 0x25: return core::Key::left;
            case 0x26: return core::Key::up;
            case 0x27: return core::Key::right;
            case 0x28: return core::Key::down;
            case 0x2C: return core::Key::print_screen;
            case 0x2D: return core::Key::insert;
            case 0x2E: return core::Key::delete_key;
            case 0x60: return core::Key::keypad0;
            case 0x61: return core::Key::keypad1;
            case 0x62: return core::Key::keypad2;
            case 0x63: return core::Key::keypad3;
            case 0x64: return core::Key::keypad4;
            case 0x65: return core::Key::keypad5;
            case 0x66: return core::Key::keypad6;
            case 0x67: return core::Key::keypad7;
            case 0x68: return core::Key::keypad8;
            case 0x69: return core::Key::keypad9;
            case 0x6A: return core::Key::keypad_multiply;
            case 0x6B: return core::Key::keypad_plus;
            case 0x6D: return core::Key::keypad_minus;
            case 0x6E: return core::Key::keypad_period;
            case 0x6F: return core::Key::keypad_divide;
            case 0x70: return core::Key::f1;
            case 0x71: return core::Key::f2;
            case 0x72: return core::Key::f3;
            case 0x73: return core::Key::f4;
            case 0x74: return core::Key::f5;
            case 0x75: return core::Key::f6;
            case 0x76: return core::Key::f7;
            case 0x77: return core::Key::f8;
            case 0x78: return core::Key::f9;
            case 0x79: return core::Key::f10;
            case 0x7A: return core::Key::f11;
            case 0x7B: return core::Key::f12;
            case 0x90: return core::Key::num_lock;
            case 0x91: return core::Key::scroll_lock;
            case 0xBA: return core::Key::semicolon;
            case 0xBB: return core::Key::equals;
            case 0xBC: return core::Key::comma;
            case 0xBD: return core::Key::minus;
            case 0xBE: return core::Key::period;
            case 0xBF: return core::Key::slash;
            case 0xC0: return core::Key::grave;
            case 0xDB: return core::Key::left_bracket;
            case 0xDC: return core::Key::backslash;
            case 0xDD: return core::Key::right_bracket;
            case 0xDE: return core::Key::apostrophe;
            case 0xA2: return core::Key::left_control;
            case 0xA3: return core::Key::right_control;
            case 0xA0: return core::Key::left_shift;
            case 0xA1: return core::Key::right_shift;
            case 0xA4: return core::Key::left_alt;
            case 0xA5: return core::Key::right_alt;
            default: return core::Key::unknown;
        }
    }

    core::MouseButton mouseButtonFromLegacyCode(const int code) noexcept
    {
        switch(code) {
            case 0:
            case 1: return core::MouseButton::left;
            case 2: return core::MouseButton::right;
            case 3: return core::MouseButton::middle;
            case 4: return core::MouseButton::x1;
            case 5: return core::MouseButton::x2;
            default: return core::MouseButton::count;
        }
    }

    int legacyCodeFromKey(const core::Key key) noexcept
    {
        if(key >= core::Key::a && key <= core::Key::z) {
            return 0x41 + static_cast<int>(key) - static_cast<int>(core::Key::a);
        }
        if(key >= core::Key::digit1 && key <= core::Key::digit9) {
            return 0x31 + static_cast<int>(key) - static_cast<int>(core::Key::digit1);
        }
        switch(key) {
            case core::Key::digit0: return 0x30;
            case core::Key::enter: return 0x0D;
            case core::Key::escape: return 0x1B;
            case core::Key::backspace: return 0x08;
            case core::Key::tab: return 0x09;
            case core::Key::space: return 0x20;
            case core::Key::page_up: return 0x21;
            case core::Key::page_down: return 0x22;
            case core::Key::end: return 0x23;
            case core::Key::home: return 0x24;
            case core::Key::left: return 0x25;
            case core::Key::up: return 0x26;
            case core::Key::right: return 0x27;
            case core::Key::down: return 0x28;
            case core::Key::print_screen: return 0x2C;
            case core::Key::insert: return 0x2D;
            case core::Key::delete_key: return 0x2E;
            case core::Key::keypad0: return 0x60;
            case core::Key::keypad1: return 0x61;
            case core::Key::keypad2: return 0x62;
            case core::Key::keypad3: return 0x63;
            case core::Key::keypad4: return 0x64;
            case core::Key::keypad5: return 0x65;
            case core::Key::keypad6: return 0x66;
            case core::Key::keypad7: return 0x67;
            case core::Key::keypad8: return 0x68;
            case core::Key::keypad9: return 0x69;
            case core::Key::keypad_multiply: return 0x6A;
            case core::Key::keypad_plus: return 0x6B;
            case core::Key::keypad_minus: return 0x6D;
            case core::Key::keypad_period: return 0x6E;
            case core::Key::keypad_divide: return 0x6F;
            case core::Key::f1: return 0x70;
            case core::Key::f2: return 0x71;
            case core::Key::f3: return 0x72;
            case core::Key::f4: return 0x73;
            case core::Key::f5: return 0x74;
            case core::Key::f6: return 0x75;
            case core::Key::f7: return 0x76;
            case core::Key::f8: return 0x77;
            case core::Key::f9: return 0x78;
            case core::Key::f10: return 0x79;
            case core::Key::f11: return 0x7A;
            case core::Key::f12: return 0x7B;
            case core::Key::num_lock: return 0x90;
            case core::Key::scroll_lock: return 0x91;
            case core::Key::semicolon: return 0xBA;
            case core::Key::equals: return 0xBB;
            case core::Key::comma: return 0xBC;
            case core::Key::minus: return 0xBD;
            case core::Key::period: return 0xBE;
            case core::Key::slash: return 0xBF;
            case core::Key::grave: return 0xC0;
            case core::Key::left_bracket: return 0xDB;
            case core::Key::backslash: return 0xDC;
            case core::Key::right_bracket: return 0xDD;
            case core::Key::apostrophe: return 0xDE;
            case core::Key::left_control: return 0xA2;
            case core::Key::right_control: return 0xA3;
            case core::Key::left_shift: return 0xA0;
            case core::Key::right_shift: return 0xA1;
            case core::Key::left_alt: return 0xA4;
            case core::Key::right_alt: return 0xA5;
            default: return 0;
        }
    }
}

namespace luastg
{
    void AppFrame::OpenInput()
    {
        core::InputSystem::getInstance().resetKeyboardAndMouse();
    }

    void AppFrame::CloseInput()
    {
        core::InputSystem::getInstance().resetKeyboardAndMouse();
    }

    void AppFrame::UpdateInput()
    {
    }

    void AppFrame::ResetKeyboardInput()
    {
        core::InputSystem::getInstance().resetKeyboardAndMouse();
    }

    void AppFrame::ResetMouseInput()
    {
        core::InputSystem::getInstance().resetKeyboardAndMouse();
    }

    bool AppFrame::GetKeyState(const int code) noexcept
    {
        return core::InputSystem::getInstance().isKeyDown(keyFromLegacyCode(code));
    }

    int AppFrame::GetLastKey() noexcept
    {
        return legacyCodeFromKey(core::InputSystem::getInstance().getLastPressedKey());
    }

    bool AppFrame::GetMouseState_legacy(const int button) noexcept
    {
        core::MouseButton mapped = core::MouseButton::count;
        switch(button) {
            case 0: mapped = core::MouseButton::left; break;
            case 1: mapped = core::MouseButton::middle; break;
            case 2: mapped = core::MouseButton::right; break;
            case 3: mapped = core::MouseButton::x1; break;
            case 4: mapped = core::MouseButton::x2; break;
            default: break;
        }
        return mapped != core::MouseButton::count && core::InputSystem::getInstance().isMouseButtonDown(mapped);
    }

    bool AppFrame::GetMouseState(const int button) noexcept
    {
        const auto mapped = mouseButtonFromLegacyCode(button);
        return mapped != core::MouseButton::count && core::InputSystem::getInstance().isMouseButtonDown(mapped);
    }

    core::Vector2F AppFrame::GetMousePosition(const bool no_flip) noexcept
    {
        const auto mouse = core::InputSystem::getInstance().getMouseState();
        const auto canvas_size = GetAppModel()->getSwapChain()->getCanvasSize();
        const auto transform = GetMousePositionTransformF();
        core::Vector2F position(
            (mouse.x + transform.x) * transform.z,
            (mouse.y + transform.y) * transform.w);
        if(!no_flip) {
            position.y = static_cast<float>(canvas_size.y) - position.y;
        }
        return position;
    }

    core::Vector2F AppFrame::GetCurrentWindowSizeF()
    {
        const auto size = GetAppModel()->getWindow()->getSize();
        return { static_cast<float>(size.x), static_cast<float>(size.y) };
    }

    core::Vector4F AppFrame::GetMousePositionTransformF()
    {
        const auto window_size = GetAppModel()->getWindow()->getSize();
        const auto canvas_size = GetAppModel()->getSwapChain()->getCanvasSize();
        if(window_size.x == 0 || window_size.y == 0 || canvas_size.x == 0 || canvas_size.y == 0) {
            return {};
        }

        const auto scaling_mode = GetAppModel()->getSwapChain()->getScalingMode();
        if(scaling_mode == core::Graphics::SwapChainScalingMode::Stretch) {
            return {
                0.0f,
                0.0f,
                static_cast<float>(canvas_size.x) / static_cast<float>(window_size.x),
                static_cast<float>(canvas_size.y) / static_cast<float>(window_size.y),
            };
        }

        float scale = std::min(
            static_cast<float>(window_size.x) / static_cast<float>(canvas_size.x),
            static_cast<float>(window_size.y) / static_cast<float>(canvas_size.y));
        if(scaling_mode == core::Graphics::SwapChainScalingMode::IntegerAspectRatio && scale >= 1.0f) {
            scale = static_cast<float>(static_cast<uint32_t>(scale));
        }

        const float width = scale * static_cast<float>(canvas_size.x);
        const float height = scale * static_cast<float>(canvas_size.y);
        const float x = (static_cast<float>(window_size.x) - width) * 0.5f;
        const float y = (static_cast<float>(window_size.y) - height) * 0.5f;
        return { -x, -y, 1.0f / scale, 1.0f / scale };
    }

    int32_t AppFrame::GetMouseWheelDelta() noexcept
    {
        constexpr float wheel_delta = 120.0f;
        return static_cast<int32_t>(std::lround(core::InputSystem::getInstance().getMouseState().wheel_y * wheel_delta));
    }
}
