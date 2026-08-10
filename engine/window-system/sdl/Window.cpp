#include "sdl/Window.hpp"
#include "core/Configuration.hpp"
#include "core/Logger.hpp"
#include "core/SmartReference.hpp"
#include "simdutf.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <stdexcept>
#include <unordered_map>

namespace
{
    std::unordered_map<SDL_WindowID, core::Window*> windows;

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
}

namespace core
{
    Window::Window(const Vector2U size, const StringView title, const WindowFrameStyle style, const bool visible)
        : m_title(title), m_style(style)
    {
        m_window = SDL_CreateWindow(
            m_title.c_str(),
            static_cast<int>(size.x),
            static_cast<int>(size.y),
            toWindowFlags(style, visible));
        if(m_window == nullptr) {
            Logger::error("[sdl] SDL_CreateWindow failed: {}", SDL_GetError());
            throw std::runtime_error("SDL_CreateWindow failed");
        }

        windows.emplace(SDL_GetWindowID(m_window), this);
        setCursor(WindowCursor::Arrow);
    }

    Window::~Window()
    {
        if(m_window != nullptr) {
            dispatch([](IWindowEventListener& listener) { listener.onWindowDestroy(); });
            windows.erase(SDL_GetWindowID(m_window));
        }
        if(m_cursor_handle != nullptr) {
            SDL_DestroyCursor(m_cursor_handle);
        }
        SDL_DestroyWindow(m_window);
    }

    void Window::dispatchSDLEvent(const SDL_Event& event)
    {
        const SDL_WindowID id = eventWindowID(event);
        if(id == 0) {
            return;
        }
        if(const auto it = windows.find(id); it != windows.end()) {
            it->second->handleEvent(event);
        }
    }

    void Window::handleEvent(const SDL_Event& event)
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
                dispatch([](IWindowEventListener& listener) { listener.onDeviceChange(); });
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
                        setWindowMode(getSize(), m_style, nullptr);
                    } else {
                        setFullScreenMode(nullptr);
                    }
                }
                break;
            default:
                break;
        }
    }

    void Window::dispatch(const std::function<void(IWindowEventListener&)>& callback)
    {
        m_dispatching = true;
        for(IWindowEventListener* listener : m_listeners) {
            if(listener != nullptr) {
                callback(*listener);
            }
        }
        m_dispatching = false;
        std::erase(m_listeners, nullptr);
        for(IWindowEventListener* listener : m_pending_listeners) {
            addEventListener(listener);
        }
        m_pending_listeners.clear();
    }

    void Window::addEventListener(IWindowEventListener* const listener)
    {
        removeEventListener(listener);
        if(m_dispatching) {
            m_pending_listeners.push_back(listener);
        } else if(listener != nullptr) {
            m_listeners.push_back(listener);
        }
    }

    void Window::removeEventListener(IWindowEventListener* const listener)
    {
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

    void Window::setIMEState(const bool enabled)
    {
        textInput_setEnabled(enabled);
    }

    bool Window::getIMEState()
    {
        return textInput_isEnabled();
    }

    void Window::setInputMethodPosition(const Vector2I position)
    {
        m_text_input_position = position;
        const SDL_Rect area{ position.x, position.y, 1, 1 };
        if(!SDL_SetTextInputArea(m_window, &area, 0)) {
            Logger::error("[sdl] SDL_SetTextInputArea failed: {}", SDL_GetError());
        }
    }

    bool Window::textInput_isEnabled()
    {
        return m_text_input_enabled;
    }

    void Window::textInput_setEnabled(const bool enabled)
    {
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

    StringView Window::textInput_getBuffer()
    {
        return m_text_buffer_utf8;
    }

    void Window::textInput_clearBuffer()
    {
        m_text_buffer.clear();
        m_text_buffer_utf8.clear();
        m_text_cursor = 0;
    }

    uint32_t Window::textInput_getCursorPosition()
    {
        return m_text_cursor;
    }

    void Window::textInput_setCursorPosition(const uint32_t position)
    {
        m_text_cursor = std::min(position, static_cast<uint32_t>(m_text_buffer.size()));
    }

    void Window::textInput_addCursorPosition(const int32_t offset)
    {
        const int64_t position = std::clamp<int64_t>(
            static_cast<int64_t>(m_text_cursor) + offset,
            0,
            static_cast<int64_t>(m_text_buffer.size()));
        m_text_cursor = static_cast<uint32_t>(position);
    }

    void Window::textInput_removeBufferRange(const uint32_t position, const uint32_t count)
    {
        if(position >= m_text_buffer.size()) {
            return;
        }
        m_text_buffer.erase(position, std::min<size_t>(count, m_text_buffer.size() - position));
        m_text_cursor = std::min(m_text_cursor, static_cast<uint32_t>(m_text_buffer.size()));
        updateTextBuffer();
    }

    void Window::textInput_insertBufferRange(const uint32_t position, const StringView text)
    {
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

    void Window::textInput_backspace(const uint32_t count)
    {
        const uint32_t removed = std::min(count, m_text_cursor);
        m_text_cursor -= removed;
        m_text_buffer.erase(m_text_cursor, removed);
        updateTextBuffer();
    }

    void Window::appendText(const StringView text)
    {
        textInput_insertBufferRange(m_text_cursor, text);
    }

    void Window::updateTextBuffer()
    {
        m_text_buffer_utf8.resize(simdutf::utf8_length_from_utf32(m_text_buffer.data(), m_text_buffer.size()));
        simdutf::convert_valid_utf32_to_utf8(m_text_buffer.data(), m_text_buffer.size(), m_text_buffer_utf8.data());
    }

    void Window::setTitleText(const StringView text)
    {
        m_title.assign(text);
        if(!SDL_SetWindowTitle(m_window, m_title.c_str())) {
            Logger::error("[sdl] SDL_SetWindowTitle failed: {}", SDL_GetError());
        }
    }

    StringView Window::getTitleText()
    {
        return m_title;
    }

    bool Window::setFrameStyle(const WindowFrameStyle style)
    {
        const bool bordered = style != WindowFrameStyle::None;
        const bool resizable = style == WindowFrameStyle::Normal;
        if(!SDL_SetWindowBordered(m_window, bordered) || !SDL_SetWindowResizable(m_window, resizable)) {
            Logger::error("[sdl] Failed to change window frame style: {}", SDL_GetError());
            return false;
        }
        m_style = style;
        return true;
    }

    WindowFrameStyle Window::getFrameStyle()
    {
        return m_style;
    }

    Vector2U Window::getSize()
    {
        int width{};
        int height{};
        if(!SDL_GetWindowSize(m_window, &width, &height)) {
            Logger::error("[sdl] SDL_GetWindowSize failed: {}", SDL_GetError());
        }
        return { static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0)) };
    }

    Vector2U Window::getPixelSize()
    {
        int width{};
        int height{};
        if(!SDL_GetWindowSizeInPixels(m_window, &width, &height)) {
            Logger::error("[sdl] SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());
        }
        return { static_cast<uint32_t>(std::max(width, 0)), static_cast<uint32_t>(std::max(height, 0)) };
    }

    bool Window::setSize(const Vector2U size)
    {
        return SDL_SetWindowSize(m_window, static_cast<int>(size.x), static_cast<int>(size.y));
    }

    Vector2I Window::getPosition()
    {
        int x{};
        int y{};
        if(!SDL_GetWindowPosition(m_window, &x, &y)) {
            Logger::error("[sdl] SDL_GetWindowPosition failed: {}", SDL_GetError());
        }
        return { x, y };
    }

    bool Window::setPosition(const Vector2I position)
    {
        return SDL_SetWindowPosition(m_window, position.x, position.y);
    }

    bool Window::setVisible(const bool visible)
    {
        return visible ? SDL_ShowWindow(m_window) : SDL_HideWindow(m_window);
    }

    bool Window::isVisible()
    {
        return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_HIDDEN) == 0;
    }

    bool Window::setAlwaysOnTop(const bool enabled)
    {
        return SDL_SetWindowAlwaysOnTop(m_window, enabled);
    }

    bool Window::raise()
    {
        return SDL_RaiseWindow(m_window);
    }

    uint32_t Window::getDPI()
    {
        return static_cast<uint32_t>(96.0f * getDPIScaling() + 0.5f);
    }

    float Window::getDPIScaling()
    {
        const float scale = SDL_GetWindowDisplayScale(m_window);
        return scale > 0.0f ? scale : 1.0f;
    }

    void Window::setWindowMode(const Vector2U size, const WindowFrameStyle style, IDisplay* const display)
    {
        if(!SDL_SetWindowFullscreen(m_window, false)) {
            Logger::error("[sdl] Failed to leave fullscreen mode: {}", SDL_GetError());
        }
        m_fullscreen = false;
        setFrameStyle(style);
        setSize(size);
        setCentered(true, display);
    }

    void Window::setFullScreenMode(IDisplay* const display)
    {
        if(display != nullptr) {
            const SDL_DisplayID id = display->getSDLDisplayID();
            SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id));
        }
        if(!SDL_SetWindowFullscreen(m_window, true)) {
            Logger::error("[sdl] Failed to enter fullscreen mode: {}", SDL_GetError());
        }
    }

    void Window::setCentered(const bool visible, IDisplay* display)
    {
        SmartReference<IDisplay> nearest;
        if(display == nullptr && IDisplay::getNearestFromWindow(this, nearest.put())) {
            display = nearest.get();
        }
        const SDL_DisplayID id = display != nullptr ? display->getSDLDisplayID() : SDL_GetPrimaryDisplay();
        SDL_SetWindowPosition(m_window, SDL_WINDOWPOS_CENTERED_DISPLAY(id), SDL_WINDOWPOS_CENTERED_DISPLAY(id));
        if(visible) {
            setVisible(true);
        }
    }

    bool Window::setCursor(const WindowCursor type)
    {
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
        if(m_cursor_handle != nullptr) {
            SDL_DestroyCursor(m_cursor_handle);
        }
        m_cursor_handle = cursor;
        if(!SDL_SetCursor(m_cursor_handle) || !SDL_ShowCursor()) {
            Logger::error("[sdl] Failed to set cursor: {}", SDL_GetError());
            return false;
        }
        m_cursor = type;
        return true;
    }

    WindowCursor Window::getCursor()
    {
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
            auto* const window = new Window(
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
            *output = new Window(size, title, style, visible);
            return true;
        } catch(...) {
            *output = nullptr;
            return false;
        }
    }
}
