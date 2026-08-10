#pragma once
#include "core/Vector2.hpp"
#include "core/Rect.hpp"
#include "core/ReferenceCounted.hpp"
#include "core/ImmutableString.hpp"
#include "core/Display.hpp"

struct SDL_Window;

namespace core
{
    struct IWindowEventListener
    {
        virtual void onWindowCreate() {};
        virtual void onWindowDestroy() {};

        virtual void onWindowActive() {};
        virtual void onWindowInactive() {};

        virtual void onWindowSize(Vector2U size) { (void)size; };
        virtual void onWindowPixelSize(Vector2U size) { (void)size; };
        virtual void onWindowMove(Vector2I position) { (void)position; };
        virtual void onWindowExposed() {};
        virtual void onWindowFullscreenStateChange(bool state) { (void)state; }
        virtual void onWindowDpiChange() {};

        virtual void onWindowClose() {};

        virtual void onDeviceChange() {};

    };

    enum class WindowFrameStyle
    {
        None,
        Fixed,
        Normal,
    };

    enum class WindowCursor
    {
        None,

        Arrow,
        Hand,

        Cross,
        TextInput,

        Resize,
        ResizeEW,
        ResizeNS,
        ResizeNESW,
        ResizeNWSE,

        NotAllowed,
        Wait,
    };

    CORE_INTERFACE IWindow : IReferenceCounted
    {
        virtual void addEventListener(IWindowEventListener * e) = 0;
        virtual void removeEventListener(IWindowEventListener * e) = 0;

        virtual SDL_Window* getSDLWindow() const noexcept = 0;

        virtual void setIMEState(bool enable) = 0;
        virtual bool getIMEState() = 0;

        // vvvvvvvv BEGIN WIP

        virtual void setInputMethodPosition(Vector2I position) = 0;

        virtual bool textInput_isEnabled() = 0;
        virtual void textInput_setEnabled(bool enabled) = 0;
        virtual StringView textInput_getBuffer() = 0;
        virtual void textInput_clearBuffer() = 0;
        virtual uint32_t textInput_getCursorPosition() = 0;
        virtual void textInput_setCursorPosition(uint32_t code_point_index) = 0;
        virtual void textInput_addCursorPosition(int32_t offset_by_code_point) = 0;
        virtual void textInput_removeBufferRange(uint32_t code_point_index, uint32_t code_point_count) = 0;
        virtual void textInput_insertBufferRange(uint32_t code_point_index, StringView str) = 0;
        virtual void textInput_backspace(uint32_t code_point_count) = 0;

        // ^^^^^^^^ END WIP

        virtual void setTitleText(StringView str) = 0;
        virtual StringView getTitleText() = 0;

        virtual bool setFrameStyle(WindowFrameStyle style) = 0;
        virtual WindowFrameStyle getFrameStyle() = 0;

        virtual Vector2U getSize() = 0;
        virtual Vector2U getPixelSize() = 0;
        virtual bool setSize(Vector2U v) = 0;
        virtual Vector2I getPosition() = 0;
        virtual bool setPosition(Vector2I position) = 0;
        virtual bool setVisible(bool visible) = 0;
        virtual bool isVisible() = 0;
        virtual bool setAlwaysOnTop(bool enabled) = 0;
        virtual bool raise() = 0;

        virtual uint32_t getDPI() = 0;
        virtual float getDPIScaling() = 0;

        virtual void setWindowMode(Vector2U size, WindowFrameStyle style = WindowFrameStyle::Normal, IDisplay* display = nullptr) = 0;
        virtual void setFullScreenMode(IDisplay* display = nullptr) = 0;
        virtual void setCentered(bool show, IDisplay* display = nullptr) = 0;

        virtual bool setCursor(WindowCursor type) = 0;
        virtual WindowCursor getCursor() = 0;

        static bool create(IWindow * *pp_window);
        static bool create(Vector2U size, StringView title_text, WindowFrameStyle style, bool show, IWindow** pp_window);
    };

    CORE_INTERFACE_ID(IWindow, "9432a56d-e3d2-5173-b313-a9581b373155")
}
