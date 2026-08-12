#include "sdl/Window.hpp"
#include "core/Configuration.hpp"
#include "core/Logger.hpp"
#include "simdutf.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
    std::unordered_map<SDL_WindowID, core::WindowSDL3*> windows;

    bool requireMainThread(const std::string_view operation)
    {
        if(SDL_IsMainThread()) {
            return true;
        }
        core::Logger::error("[sdl] {} must be called on the main thread", operation);
        return false;
    }

    bool reportFailure(const bool result, const std::string_view operation)
    {
        if(!result) {
            core::Logger::error("[sdl] {} failed: {}", operation, SDL_GetError());
        }
        return result;
    }

    SDL_SystemCursor toSystemCursor(const core::WindowCursor cursor)
    {
        switch(cursor) {
            case core::WindowCursor::Arrow: return SDL_SYSTEM_CURSOR_DEFAULT;
            case core::WindowCursor::Hand: return SDL_SYSTEM_CURSOR_POINTER;
            case core::WindowCursor::Cross: return SDL_SYSTEM_CURSOR_CROSSHAIR;
            case core::WindowCursor::TextInput: return SDL_SYSTEM_CURSOR_TEXT;
            case core::WindowCursor::Resize: return SDL_SYSTEM_CURSOR_MOVE;
            case core::WindowCursor::ResizeEW: return SDL_SYSTEM_CURSOR_EW_RESIZE;
            case core::WindowCursor::ResizeNS: return SDL_SYSTEM_CURSOR_NS_RESIZE;
            case core::WindowCursor::ResizeNESW: return SDL_SYSTEM_CURSOR_NESW_RESIZE;
            case core::WindowCursor::ResizeNWSE: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
            case core::WindowCursor::NotAllowed: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
            case core::WindowCursor::Wait: return SDL_SYSTEM_CURSOR_WAIT;
            case core::WindowCursor::None: break;
        }
        return SDL_SYSTEM_CURSOR_DEFAULT;
    }

    SDL_WindowFlags toWindowFlags(const core::WindowFrameStyle style, const bool visible)
    {
        SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
        flags |= visible ? 0 : SDL_WINDOW_HIDDEN;
        if(style == core::WindowFrameStyle::None) {
            flags |= SDL_WINDOW_BORDERLESS;
        } else if(style == core::WindowFrameStyle::Normal) {
            flags |= SDL_WINDOW_RESIZABLE;
        }
        return flags;
    }

    SDL_WindowID eventWindowID(const SDL_Event& event)
    {
        switch(event.type) {
            case SDL_EVENT_TEXT_INPUT: return event.text.windowID;
            case SDL_EVENT_TEXT_EDITING: return event.edit.windowID;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP: return event.key.windowID;
            case SDL_EVENT_MOUSE_MOTION: return event.motion.windowID;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: return event.button.windowID;
            case SDL_EVENT_MOUSE_WHEEL: return event.wheel.windowID;
            default:
                if(event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST) {
                    return event.window.windowID;
                }
                return 0;
        }
    }

    bool isDeviceChangeEvent(const Uint32 type)
    {
        switch(type) {
            case SDL_EVENT_AUDIO_DEVICE_ADDED:
            case SDL_EVENT_AUDIO_DEVICE_REMOVED:
            case SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED:
            case SDL_EVENT_KEYBOARD_ADDED:
            case SDL_EVENT_KEYBOARD_REMOVED:
            case SDL_EVENT_MOUSE_ADDED:
            case SDL_EVENT_MOUSE_REMOVED:
            case SDL_EVENT_JOYSTICK_ADDED:
            case SDL_EVENT_JOYSTICK_REMOVED:
            case SDL_EVENT_GAMEPAD_ADDED:
            case SDL_EVENT_GAMEPAD_REMOVED:
            case SDL_EVENT_GAMEPAD_REMAPPED:
                return true;
            default:
                return false;
        }
    }

    std::vector<SDL_WindowID> getWindowIDs()
    {
        std::vector<SDL_WindowID> result;
        result.reserve(windows.size());
        for(const auto& [window_id, window] : windows) {
            (void)window;
            result.push_back(window_id);
        }
        return result;
    }
}

namespace core
{
    WindowSDL3::WindowSDL3(const Vector2U size, const StringView title, const WindowFrameStyle style, const bool visible)
        : m_title(title), m_style(style), m_windowed_style(style), m_windowed_size(size)
    {
        if(!requireMainThread("SDL_CreateWindow")) {
            throw std::runtime_error("SDL window creation must run on the main thread");
        }
        if(size.x == 0 || size.y == 0 || size.x > static_cast<uint32_t>(INT_MAX) || size.y > static_cast<uint32_t>(INT_MAX)) {
            Logger::error("[sdl] SDL_CreateWindow received an invalid size: {}x{}", size.x, size.y);
            throw std::runtime_error("invalid SDL window size");
        }
        m_window = SDL_CreateWindow(
            m_title.c_str(),
            static_cast<int>(size.x),
            static_cast<int>(size.y),
            toWindowFlags(style, visible));
        if(m_window == nullptr) {
            Logger::error("[sdl] SDL_CreateWindow failed: {}", SDL_GetError());
            throw std::runtime_error("SDL_CreateWindow failed");
        }

        const SDL_WindowID id = SDL_GetWindowID(m_window);
        if(id == 0) {
            Logger::error("[sdl] SDL_GetWindowID failed: {}", SDL_GetError());
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            throw std::runtime_error("SDL_GetWindowID failed");
        }
        windows.emplace(id, this);
        setCursor(WindowCursor::Arrow);
    }

    WindowSDL3::~WindowSDL3()
    {
        assert(SDL_IsMainThread());
        requireMainThread("SDL_DestroyWindow");
        if(m_window != nullptr) {
            try {
                dispatch([](IWindowEventListener& listener) { listener.onWindowDestroy(); });
            } catch(...) {
                Logger::error("[sdl] A window listener threw during window destruction");
            }
            windows.erase(SDL_GetWindowID(m_window));
        }
        if(m_cursor_handle != nullptr) {
            bool can_destroy = true;
            if(SDL_GetCursor() == m_cursor_handle && !SDL_SetCursor(SDL_GetDefaultCursor())) {
                Logger::error("[sdl] SDL_SetCursor(default) failed during window destruction: {}", SDL_GetError());
                can_destroy = false;
            }
            if(can_destroy) {
                SDL_DestroyCursor(m_cursor_handle);
            }
            m_cursor_handle = nullptr;
        }
        SDL_DestroyWindow(m_window);
    }

    void WindowSDL3::dispatchSDLEvent(const SDL_Event& event)
    {
        if(!requireMainThread("SDL event dispatch")) {
            return;
        }
        if(isDeviceChangeEvent(event.type) ||
           (event.type >= SDL_EVENT_DISPLAY_FIRST && event.type <= SDL_EVENT_DISPLAY_LAST)) {
            for(const SDL_WindowID window_id : getWindowIDs()) {
                if(const auto it = windows.find(window_id); it != windows.end()) {
                    if(isDeviceChangeEvent(event.type) || event.type == SDL_EVENT_DISPLAY_ADDED || event.type == SDL_EVENT_DISPLAY_REMOVED) {
                        it->second->dispatch([](IWindowEventListener& listener) { listener.onDeviceChange(); });
                    }
                    if(event.type == SDL_EVENT_DISPLAY_MOVED) {
                        it->second->dispatch([](IWindowEventListener& listener) { listener.onDisplayMove(); });
                    }
                    if(event.type == SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED &&
                       SDL_GetDisplayForWindow(it->second->m_window) == event.display.displayID) {
                        it->second->dispatch([](IWindowEventListener& listener) { listener.onWindowDpiChange(); });
                    }
                }
            }
            return;
        }
        const SDL_WindowID id = eventWindowID(event);
        if(id == 0) {
            return;
        }
        if(const auto it = windows.find(id); it != windows.end()) {
            it->second->handleEvent(event);
        }
    }

    void WindowSDL3::handleEvent(const SDL_Event& event)
    {
        switch(event.type) {
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                dispatch([](IWindowEventListener& listener) { listener.onWindowActive(); });
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                dispatch([](IWindowEventListener& listener) { listener.onWindowInactive(); });
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                const Vector2U size{ static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2) };
                if(!m_fullscreen) {
                    m_windowed_size = size;
                }
                dispatch([size](IWindowEventListener& listener) { listener.onWindowSize(size); });
                break;
            }
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                const Vector2U size{ static_cast<uint32_t>(event.window.data1), static_cast<uint32_t>(event.window.data2) };
                dispatch([size](IWindowEventListener& listener) { listener.onWindowPixelSize(size); });
                break;
            }
            case SDL_EVENT_WINDOW_MOVED: {
                const Vector2I position{ event.window.data1, event.window.data2 };
                dispatch([position](IWindowEventListener& listener) { listener.onWindowMove(position); });
                break;
            }
            case SDL_EVENT_WINDOW_EXPOSED:
                dispatch([](IWindowEventListener& listener) { listener.onWindowExposed(); });
                break;
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                dispatch([](IWindowEventListener& listener) { listener.onWindowDpiChange(); });
                break;
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                dispatch([](IWindowEventListener& listener) { listener.onWindowDisplayChange(); });
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                dispatch([](IWindowEventListener& listener) { listener.onWindowClose(); });
                break;
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
                m_fullscreen = true;
                dispatch([](IWindowEventListener& listener) { listener.onWindowFullscreenStateChange(true); });
                break;
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                m_fullscreen = false;
                dispatch([](IWindowEventListener& listener) { listener.onWindowFullscreenStateChange(false); });
                break;
            case SDL_EVENT_TEXT_INPUT:
                if(m_text_input_enabled && event.text.text != nullptr) {
                    appendText(event.text.text);
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if(!event.key.repeat && event.key.scancode == SDL_SCANCODE_RETURN && (event.key.mod & SDL_KMOD_ALT) != 0) {
                    if(m_fullscreen) {
                        setWindowMode(m_windowed_size, m_windowed_style, nullptr);
                    } else {
                        setFullScreenMode(nullptr);
                    }
                }
                break;
            default:
                break;
        }
    }

    void WindowSDL3::dispatch(const std::function<void(IWindowEventListener&)>& callback)
    {
        assert(SDL_IsMainThread());
        m_dispatching = true;
        try {
            for(IWindowEventListener* listener : m_listeners) {
                if(listener != nullptr) {
                    callback(*listener);
                }
            }
        } catch(...) {
            m_dispatching = false;
            std::erase(m_listeners, nullptr);
            auto pending_listeners = std::move(m_pending_listeners);
            m_pending_listeners.clear();
            for(IWindowEventListener* listener : pending_listeners) {
                addEventListener(listener);
            }
            throw;
        }
        m_dispatching = false;
        std::erase(m_listeners, nullptr);
        auto pending_listeners = std::move(m_pending_listeners);
        m_pending_listeners.clear();
        for(IWindowEventListener* listener : pending_listeners) {
            addEventListener(listener);
        }
    }

    void WindowSDL3::addEventListener(IWindowEventListener* const listener)
    {
        if(!requireMainThread("IWindow::addEventListener")) {
            return;
        }
        removeEventListener(listener);
        if(m_dispatching) {
            m_pending_listeners.push_back(listener);
        } else if(listener != nullptr) {
            m_listeners.push_back(listener);
        }
    }

    void WindowSDL3::removeEventListener(IWindowEventListener* const listener)
    {
        if(!requireMainThread("IWindow::removeEventListener")) {
            return;
        }
        if(m_dispatching) {
            for(IWindowEventListener*& current : m_listeners) {
                if(current == listener) {
                    current = nullptr;
                }
            }
        } else {
            std::erase(m_listeners, listener);
        }
        std::erase(m_pending_listeners, listener);
    }

    void WindowSDL3::setIMEState(const bool enabled)
    {
        textInput_setEnabled(enabled);
    }

    bool WindowSDL3::getIMEState()
    {
        return textInput_isEnabled();
    }

    SDL_Window* WindowSDL3::getSDLWindow() const
    {
        if(!requireMainThread("IWindow::getSDLWindow")) {
            return nullptr;
        }
        return m_window;
    }

    void WindowSDL3::setInputMethodPosition(const Vector2I position)
    {
        if(!requireMainThread("SDL_SetTextInputArea")) {
            return;
        }
        m_text_input_position = position;
        const SDL_Rect area{ position.x, position.y, 1, 1 };
        if(!SDL_SetTextInputArea(m_window, &area, 0)) {
            Logger::error("[sdl] SDL_SetTextInputArea failed: {}", SDL_GetError());
        }
    }

    bool WindowSDL3::textInput_isEnabled()
    {
        if(!requireMainThread("IWindow::textInput_isEnabled")) {
            return false;
        }
        return m_text_input_enabled;
    }

    void WindowSDL3::textInput_setEnabled(const bool enabled)
    {
        if(!requireMainThread(enabled ? "SDL_StartTextInput" : "SDL_StopTextInput")) {
            return;
        }
        if(enabled == m_text_input_enabled) {
            return;
        }
        const bool ok = enabled ? SDL_StartTextInput(m_window) : SDL_StopTextInput(m_window);
        if(!ok) {
            Logger::error("[sdl] Failed to change text input state: {}", SDL_GetError());
            return;
        }
        m_text_input_enabled = enabled;
        if(enabled) {
            setInputMethodPosition(m_text_input_position);
        }
    }

    StringView WindowSDL3::textInput_getBuffer()
    {
        if(!requireMainThread("IWindow::textInput_getBuffer")) {
            return {};
        }
        return m_text_buffer_utf8;
    }

    void WindowSDL3::textInput_clearBuffer()
    {
        if(!requireMainThread("IWindow::textInput_clearBuffer")) {
            return;
        }
        m_text_buffer.clear();
        m_text_buffer_utf8.clear();
        m_text_cursor = 0;
    }

    uint32_t WindowSDL3::textInput_getCursorPosition()
    {
        if(!requireMainThread("IWindow::textInput_getCursorPosition")) {
            return 0;
        }
        return m_text_cursor;
    }

    void WindowSDL3::textInput_setCursorPosition(const uint32_t position)
    {
        if(!requireMainThread("IWindow::textInput_setCursorPosition")) {
            return;
        }
        m_text_cursor = std::min(position, static_cast<uint32_t>(m_text_buffer.size()));
    }

    void WindowSDL3::textInput_addCursorPosition(const int32_t offset)
    {
        if(!requireMainThread("IWindow::textInput_addCursorPosition")) {
            return;
        }
        const int64_t position = std::clamp<int64_t>(
            static_cast<int64_t>(m_text_cursor) + offset,
            0,
            static_cast<int64_t>(m_text_buffer.size()));
        m_text_cursor = static_cast<uint32_t>(position);
    }

    void WindowSDL3::textInput_removeBufferRange(const uint32_t position, const uint32_t count)
    {
        if(!requireMainThread("IWindow::textInput_removeBufferRange")) {
            return;
        }
        if(position >= m_text_buffer.size()) {
            return;
        }
        m_text_buffer.erase(position, std::min<size_t>(count, m_text_buffer.size() - position));
        m_text_cursor = std::min(m_text_cursor, static_cast<uint32_t>(m_text_buffer.size()));
        updateTextBuffer();
    }

    void WindowSDL3::textInput_insertBufferRange(const uint32_t position, const StringView text)
    {
        if(!requireMainThread("IWindow::textInput_insertBufferRange")) {
            return;
        }
        if(!simdutf::validate_utf8(text.data(), text.size())) {
            return;
        }
        std::u32string converted(simdutf::utf32_length_from_utf8(text.data(), text.size()), U'\0');
        simdutf::convert_valid_utf8_to_utf32(text.data(), text.size(), converted.data());
        const size_t insert_position = std::min<size_t>(position, m_text_buffer.size());
        m_text_buffer.insert(insert_position, converted);
        m_text_cursor = static_cast<uint32_t>(insert_position + converted.size());
        updateTextBuffer();
    }

    void WindowSDL3::textInput_backspace(const uint32_t count)
    {
        if(!requireMainThread("IWindow::textInput_backspace")) {
            return;
        }
        const uint32_t removed = std::min(count, m_text_cursor);
        m_text_cursor -= removed;
        m_text_buffer.erase(m_text_cursor, removed);
        updateTextBuffer();
    }

    void WindowSDL3::appendText(const StringView text)
    {
        textInput_insertBufferRange(m_text_cursor, text);
    }

    void WindowSDL3::updateTextBuffer()
    {
        m_text_buffer_utf8.resize(simdutf::utf8_length_from_utf32(m_text_buffer.data(), m_text_buffer.size()));
        simdutf::convert_valid_utf32_to_utf8(m_text_buffer.data(), m_text_buffer.size(), m_text_buffer_utf8.data());
    }

    void WindowSDL3::setTitleText(const StringView text)
    {
        if(!requireMainThread("SDL_SetWindowTitle")) {
            return;
        }
        const std::string title(text);
        if(!SDL_SetWindowTitle(m_window, title.c_str())) {
            Logger::error("[sdl] SDL_SetWindowTitle failed: {}", SDL_GetError());
            return;
        }
        m_title = title;
    }

    StringView WindowSDL3::getTitleText()
    {
        if(!requireMainThread("IWindow::getTitleText")) {
            return {};
        }
        return m_title;
    }

    bool WindowSDL3::setFrameStyle(const WindowFrameStyle style)
    {
        if(!requireMainThread("SDL_SetWindowBordered/SDL_SetWindowResizable")) {
            return false;
        }
        const bool bordered = style != WindowFrameStyle::None;
        const bool resizable = style == WindowFrameStyle::Normal;
        if(!reportFailure(SDL_SetWindowBordered(m_window, bordered), "SDL_SetWindowBordered")) {
            return false;
        }
        if(!reportFailure(SDL_SetWindowResizable(m_window, resizable), "SDL_SetWindowResizable")) {
            return false;
        }
        m_style = style;
        if(!m_fullscreen) {
            m_windowed_style = style;
        }
        return true;
    }

    WindowFrameStyle WindowSDL3::getFrameStyle()
    {
        if(!requireMainThread("IWindow::getFrameStyle")) {
            return WindowFrameStyle::Normal;
        }
        return m_style;
    }

    Vector2U WindowSDL3::getSize()
    {
        if(!requireMainThread("SDL_GetWindowSize")) {
            return {};
        }
        int width{};
        int height{};
        if(!SDL_GetWindowSize(m_window, &width, &height)) {
            Logger::error("[sdl] SDL_GetWindowSize failed: {}", SDL_GetError());
        }
        return { static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0)) };
    }

    Vector2U WindowSDL3::getPixelSize()
    {
        if(!requireMainThread("SDL_GetWindowSizeInPixels")) {
            return {};
        }
        int width{};
        int height{};
        if(!SDL_GetWindowSizeInPixels(m_window, &width, &height)) {
            Logger::error("[sdl] SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());
        }
        return { static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0)) };
    }

    bool WindowSDL3::setSize(const Vector2U size)
    {
        if(!requireMainThread("SDL_SetWindowSize")) {
            return false;
        }
        if(size.x == 0 || size.y == 0 || size.x > static_cast<uint32_t>(INT_MAX) || size.y > static_cast<uint32_t>(INT_MAX)) {
            Logger::error("[sdl] SDL_SetWindowSize received an invalid size: {}x{}", size.x, size.y);
            return false;
        }
        if(!reportFailure(SDL_SetWindowSize(m_window, static_cast<int>(size.x), static_cast<int>(size.y)), "SDL_SetWindowSize")) {
            return false;
        }
        if(!m_fullscreen) {
            m_windowed_size = size;
        }
        return true;
    }

    Vector2I WindowSDL3::getPosition()
    {
        if(!requireMainThread("SDL_GetWindowPosition")) {
            return {};
        }
        int x{};
        int y{};
        if(!SDL_GetWindowPosition(m_window, &x, &y)) {
            Logger::error("[sdl] SDL_GetWindowPosition failed: {}", SDL_GetError());
        }
        return { x, y };
    }

    bool WindowSDL3::setPosition(const Vector2I position)
    {
        return requireMainThread("SDL_SetWindowPosition") &&
               reportFailure(SDL_SetWindowPosition(m_window, position.x, position.y), "SDL_SetWindowPosition");
    }

    bool WindowSDL3::setVisible(const bool visible)
    {
        if(!requireMainThread(visible ? "SDL_ShowWindow" : "SDL_HideWindow")) {
            return false;
        }
        return reportFailure(visible ? SDL_ShowWindow(m_window) : SDL_HideWindow(m_window), visible ? "SDL_ShowWindow" : "SDL_HideWindow");
    }

    bool WindowSDL3::isVisible()
    {
        if(!requireMainThread("SDL_GetWindowFlags")) {
            return false;
        }
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_HIDDEN) == 0;
    }

    bool WindowSDL3::setAlwaysOnTop(const bool enabled)
    {
        return requireMainThread("SDL_SetWindowAlwaysOnTop") &&
               reportFailure(SDL_SetWindowAlwaysOnTop(m_window, enabled), "SDL_SetWindowAlwaysOnTop");
    }

    bool WindowSDL3::raise()
    {
        return requireMainThread("SDL_RaiseWindow") && reportFailure(SDL_RaiseWindow(m_window), "SDL_RaiseWindow");
    }

    uint32_t WindowSDL3::getDPI()
    {
        return static_cast<uint32_t>(96.0f * getDPIScaling() + 0.5f);
    }

    float WindowSDL3::getDPIScaling()
    {
        if(!requireMainThread("SDL_GetWindowDisplayScale")) {
            return 1.0f;
        }
        const float scale = SDL_GetWindowDisplayScale(m_window);
        if(scale <= 0.0f) {
            Logger::error("[sdl] SDL_GetWindowDisplayScale failed: {}", SDL_GetError());
            return 1.0f;
        }
        return scale;
    }

    bool WindowSDL3::setWindowMode(const Vector2U size, const WindowFrameStyle style, IDisplay* const display)
    {
        if(!requireMainThread("SDL_SetWindowFullscreen")) {
            return false;
        }
        if((SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0) {
            if(!reportFailure(SDL_SetWindowFullscreen(m_window, false), "SDL_SetWindowFullscreen(false)")) {
                return false;
            }
            if(!reportFailure(SDL_SyncWindow(m_window), "SDL_SyncWindow")) {
                return false;
            }
        }
        m_fullscreen = false;
        m_windowed_size = size;
        m_windowed_style = style;
        return setFrameStyle(style) && setSize(size) && setCentered(true, display);
    }

    bool WindowSDL3::setFullScreenMode(IDisplay* const display)
    {
        if(!requireMainThread("SDL_SetWindowFullscreen")) {
            return false;
        }
        if(!m_fullscreen) {
            m_windowed_size = getSize();
            m_windowed_style = m_style;
        }
        if(display != nullptr) {
            const SDL_DisplayID id = display->getSDLDisplayID();
            if(!reportFailure(
                SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id)),
                "SDL_SetWindowPosition")) {
                return false;
            }
        }
        if(!reportFailure(SDL_SetWindowFullscreenMode(m_window, nullptr), "SDL_SetWindowFullscreenMode")) {
            return false;
        }
        if(!reportFailure(SDL_SetWindowFullscreen(m_window, true), "SDL_SetWindowFullscreen(true)")) {
            return false;
        }
        m_fullscreen = true;
        return reportFailure(SDL_SyncWindow(m_window), "SDL_SyncWindow");
    }

    bool WindowSDL3::setCentered(const bool visible, IDisplay* display)
    {
        if(!requireMainThread("SDL_SetWindowPosition")) {
            return false;
        }
        SDL_DisplayID id{};
        if(display != nullptr) {
            id = display->getSDLDisplayID();
        } else {
            id = SDL_GetDisplayForWindow(m_window);
            if(id == 0) {
                Logger::error("[sdl] SDL_GetDisplayForWindow failed: {}", SDL_GetError());
                id = SDL_GetPrimaryDisplay();
            }
        }
        if(id == 0) {
            Logger::error("[sdl] SDL_GetPrimaryDisplay failed: {}", SDL_GetError());
            return false;
        }
        if(!reportFailure(
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id)),
            "SDL_SetWindowPosition")) {
            return false;
        }
        if(visible) {
            return setVisible(true);
        }
        return true;
    }

    bool WindowSDL3::setCursor(const WindowCursor type)
    {
        if(!requireMainThread("SDL_SetCursor")) {
            return false;
        }
        if(type == WindowCursor::None) {
            if(!SDL_HideCursor()) {
                Logger::error("[sdl] SDL_HideCursor failed: {}", SDL_GetError());
                return false;
            }
            m_cursor = type;
            return true;
        }

        SDL_Cursor* const cursor = SDL_CreateSystemCursor(toSystemCursor(type));
        if(cursor == nullptr) {
            Logger::error("[sdl] SDL_CreateSystemCursor failed: {}", SDL_GetError());
            return false;
        }
        if(!SDL_SetCursor(cursor)) {
            Logger::error("[sdl] SDL_SetCursor failed: {}", SDL_GetError());
            SDL_DestroyCursor(cursor);
            return false;
        }
        if(!SDL_ShowCursor()) {
            Logger::error("[sdl] SDL_ShowCursor failed: {}", SDL_GetError());
            SDL_Cursor* const previous = m_cursor_handle != nullptr ? m_cursor_handle : SDL_GetDefaultCursor();
            if(SDL_SetCursor(previous)) {
                SDL_DestroyCursor(cursor);
            } else {
                Logger::error("[sdl] Failed to restore the previous cursor: {}", SDL_GetError());
                if(m_cursor_handle != nullptr) {
                    SDL_DestroyCursor(m_cursor_handle);
                }
                m_cursor_handle = cursor;
                m_cursor = type;
            }
            return false;
        }
        if(m_cursor_handle != nullptr) {
            SDL_DestroyCursor(m_cursor_handle);
        }
        m_cursor_handle = cursor;
        m_cursor = type;
        return true;
    }

    WindowCursor WindowSDL3::getCursor()
    {
        if(!requireMainThread("IWindow::getCursor")) {
            return WindowCursor::Arrow;
        }
        return m_cursor;
    }
}

namespace core
{
    bool IWindow::create(IWindow** const output)
    {
        assert(output != nullptr);
        try {
            const auto& window_config = ConfigurationLoader::getInstance().getWindow();
            const auto& graphics_config = ConfigurationLoader::getInstance().getGraphicsSystem();
            const StringView title = window_config.hasTitle() ? window_config.getTitle() : "LuaSTG Retro";
            auto* const window = new WindowSDL3(
                { graphics_config.getWidth(), graphics_config.getHeight() },
                title,
                WindowFrameStyle::Normal,
                false);
            window->setCursor(window_config.isCursorVisible() ? WindowCursor::Arrow : WindowCursor::None);
            *output = window;
            return true;
        } catch(...) {
            *output = nullptr;
            return false;
        }
    }

    bool IWindow::create(const Vector2U size, const StringView title, const WindowFrameStyle style, const bool visible, IWindow** const output)
    {
        assert(output != nullptr);
        try {
            *output = new WindowSDL3(size, title, style, visible);
            return true;
        } catch(...) {
            *output = nullptr;
            return false;
        }
    }
}
