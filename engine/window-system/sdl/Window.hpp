#pragma once
#include "core/Window.hpp"
#include "core/implement/ReferenceCounted.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <string>
#include <vector>

namespace core
{
    class Window final : public implement::ReferenceCounted<IWindow>
    {
    public:
        Window(Vector2U size, StringView title, WindowFrameStyle style, bool visible);
        ~Window() override;

        static void dispatchSDLEvent(const SDL_Event& event);

        void addEventListener(IWindowEventListener* listener) override;
        void removeEventListener(IWindowEventListener* listener) override;

        SDL_Window* getSDLWindow() const noexcept override { return m_window; }

        void setIMEState(bool enabled) override;
        bool getIMEState() override;
        void setInputMethodPosition(Vector2I position) override;

        bool textInput_isEnabled() override;
        void textInput_setEnabled(bool enabled) override;
        StringView textInput_getBuffer() override;
        void textInput_clearBuffer() override;
        uint32_t textInput_getCursorPosition() override;
        void textInput_setCursorPosition(uint32_t position) override;
        void textInput_addCursorPosition(int32_t offset) override;
        void textInput_removeBufferRange(uint32_t position, uint32_t count) override;
        void textInput_insertBufferRange(uint32_t position, StringView text) override;
        void textInput_backspace(uint32_t count) override;

        void setTitleText(StringView text) override;
        StringView getTitleText() override;
        bool setFrameStyle(WindowFrameStyle style) override;
        WindowFrameStyle getFrameStyle() override;
        Vector2U getSize() override;
        Vector2U getPixelSize() override;
        bool setSize(Vector2U size) override;
        Vector2I getPosition() override;
        bool setPosition(Vector2I position) override;
        bool setVisible(bool visible) override;
        bool isVisible() override;
        bool setAlwaysOnTop(bool enabled) override;
        bool raise() override;
        uint32_t getDPI() override;
        float getDPIScaling() override;
        void setWindowMode(Vector2U size, WindowFrameStyle style, IDisplay* display) override;
        void setFullScreenMode(IDisplay* display) override;
        void setCentered(bool visible, IDisplay* display) override;
        bool setCursor(WindowCursor type) override;
        WindowCursor getCursor() override;

    private:
        void handleEvent(const SDL_Event& event);
        void appendText(StringView text);
        void updateTextBuffer();
        void dispatch(const std::function<void(IWindowEventListener&)>& callback);

        SDL_Window* m_window{};
        SDL_Cursor* m_cursor_handle{};
        std::string m_title;
        WindowFrameStyle m_style{ WindowFrameStyle::Normal };
        WindowCursor m_cursor{ WindowCursor::Arrow };
        bool m_fullscreen{};
        bool m_dispatching{};
        std::vector<IWindowEventListener*> m_listeners;
        std::vector<IWindowEventListener*> m_pending_listeners;

        std::u32string m_text_buffer;
        std::string m_text_buffer_utf8;
        uint32_t m_text_cursor{};
        Vector2I m_text_input_position{};
        bool m_text_input_enabled{};
    };
}
