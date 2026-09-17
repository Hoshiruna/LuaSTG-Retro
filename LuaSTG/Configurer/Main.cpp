#include "win32/win32.hpp"
#include "win32/abi.hpp"
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_freetype.h"
#include "imgui_impl_sdl3.h"
#if defined(LUASTG_GRAPHICS_SDLGPU)
#define LUASTG_IMGUI_SDL_INIT ImGui_ImplSDL3_InitForSDLGPU
#else
#define LUASTG_IMGUI_SDL_INIT ImGui_ImplSDL3_InitForD3D
#endif
#include "Core/Graphics/Runtime.hpp"
#include "core/SdlRuntime.hpp"
#include "core/SmartReference.hpp"
#include "core/Window.hpp"
#include "sdl/Window.hpp"
#include "luastg_config_generated.h"
#include "LConfig.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <cstdio>
#include <cstdlib>
#include <memory>

using std::string_view_literals::operator""sv;
using std::string_literals::operator""s;
using nlohmann::operator""_json_pointer;

enum class Language : size_t
{
    Chinese = 0,
    English = 1,
};
static Language i18n_map_index = Language::Chinese;
static std::unordered_map<std::string_view, std::string_view> i18n_map[2] = {
    {
        { "window-title", "启动配置" },
        { "language-chinese", "简体中文" },
        { "language-english", "English" },
        { "setting-language", "语言（Language）" },
        { "setting-graphic-card", "显卡" },
        { "setting-display-mode", "显示模式" },
        { "setting-canvas-size", "分辨率" },
        { "setting-fullscreen", "全屏" },
        { "setting-vsync", "垂直同步" },
        { "setting-cancel", "取消并退出" },
        { "setting-save", "保存并退出" },

        { "common-enable", "启用" },
        { "common-default1", "默认" },
        { "common-default2", "（默认）" },

        { "config-show-advance", "显示高级设置" },

        { "config-application", "应用" },
        { "config-application-uuid", "UUID" },
        { "config-application-single-instance", "单例模式" },
        { "config-debug", "调试" },
        { "config-debug-track-window-focus", "记录窗口焦点被哪个应用占用" },
        { "config-logging", "日志" },
        { "config-logging-level", "日志等级" },
        { "config-logging-level-debug", "调试（debug）" },
        { "config-logging-level-info", "信息（info）" },
        { "config-logging-level-warn", "警告（warn）" },
        { "config-logging-level-error", "错误（error）" },
        { "config-logging-level-fatal", "严重错误（fatal）" },
        { "config-logging-debugger", "Windows调试器" },
        { "config-logging-console", "控制台窗口" },
        { "config-logging-console-preserve", "关闭程序后保留控制台窗口" },
        { "config-logging-file", "日志文件" },
        { "config-logging-file-path", "文件路径" },
        { "config-logging-rolling-file", "滚动日志文件" },
        { "config-logging-rolling-file-path", "文件夹路径" },
        { "config-logging-rolling-file-max-history", "保留的文件数量" },
        { "config-timing", "计时系统" },
        { "config-timing-frame-rate", "目标帧率" },
        { "config-timing-frame-rate-warn", "警告：随意修改目标帧率可能会造成严重后果" },
        { "config-window", "窗口" },
        { "config-window-title", "窗口标题" },
        { "config-window-cursor-visible", "显示鼠标" },
        { "config-graphics-system", "显示" },
        { "config-graphics-system-resolution", "分辨率" },
        { "config-graphics-system-fullscreen", "全屏显示" },
        { "config-graphics-system-vsync", "垂直同步（防止画面撕裂）" },
        { "config-graphics-system-renderer-driver", "渲染驱动" },
        { "config-audio-system", "音频" },
        { "config-audio-system-master-volume", "主音量" },
        { "config-audio-system-sound-effect-volume", "音效音量" },
        { "config-audio-system-music-volume", "背景音乐音量" },
    },
    {
        { "window-title", "Configuer" },
        { "language-chinese", "简体中文" },
        { "language-english", "English" },
        { "setting-language", "Language (语言)" },
        { "setting-graphic-card", "Graphic Card" },
        { "setting-display-mode", "Display Mode" },
        { "setting-canvas-size", "Resolution" },
        { "setting-fullscreen", "Fullscreen" },
        { "setting-vsync", "VSync" },
        { "setting-cancel", "Cancel & Exit" },
        { "setting-save", "Save & Exit" },

        { "common-enable", "Enable" },
        { "common-default1", "Default" },
        { "common-default2", "(Default)" },

        { "config-show-advance", "Show Advance" },

        { "config-application", "Application" },
        { "config-application-uuid", "UUID" },
        { "config-application-single-instance", "Single instance" },
        { "config-debug", "Debug" },
        { "config-debug-track-window-focus", "Log which application has acquired the window focus" },
        { "config-logging", "Logging" },
        { "config-logging-level", "Logging level" },
        { "config-logging-level-debug", "Debug" },
        { "config-logging-level-info", "Info" },
        { "config-logging-level-warn", "Warn" },
        { "config-logging-level-error", "Error" },
        { "config-logging-level-fatal", "Fatal" },
        { "config-logging-debugger", "Windows Debugger" },
        { "config-logging-console", "Console Window" },
        { "config-logging-console-preserve", "Keep console window open after closing the program" },
        { "config-logging-file", "File" },
        { "config-logging-file-path", "File path" },
        { "config-logging-rolling-file", "Rolling File" },
        { "config-logging-rolling-file-path", "Folder path" },
        { "config-logging-rolling-file-max-history", "Number of files to retain" },
        { "config-timing", "Timing" },
        { "config-timing-frame-rate", "Target frame rate" },
        { "config-timing-frame-rate-warn", "Warning: Arbitrarily modifying the target frame rate may lead to serious consequences." },
        { "config-window", "Window" },
        { "config-window-title", "Window title" },
        { "config-window-cursor-visible", "Show mouse cursor" },
        { "config-graphics-system", "Display" },
        { "config-graphics-system-resolution", "Resolution" },
        { "config-graphics-system-fullscreen", "Fullscreen" },
        { "config-graphics-system-vsync", "VSync (prevent screen tearing)" },
        { "config-graphics-system-renderer-driver", "Renderer driver" },
        { "config-audio-system", "Audio" },
        { "config-audio-system-master-volume", "Master volume" },
        { "config-audio-system-sound-effect-volume", "Sound effect volume" },
        { "config-audio-system-music-volume", "Music volume" },
    },
};
std::string_view const&
i18n(std::string_view const& key)
{
    auto it = i18n_map[size_t(i18n_map_index)].find(key);
    if(it != i18n_map[size_t(i18n_map_index)].end()) {
        return it->second;
    }
    return key;
}
inline char const*
i18n_c_str(std::string_view const& key)
{
    return i18n(key).data();
}

// Common Controls

namespace
{
#define EDIT_COMMON_PARAMS nlohmann::json &json, const nlohmann::json_pointer<std::string>&path, const std::string_view i18n_id
    void showCheckBoxEdit(EDIT_COMMON_PARAMS, const bool default_value = false)
    {
        bool enable = json.value(path, default_value);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str(i18n_id));
        ImGui::PushID(i18n_c_str(i18n_id));
        if(ImGui::Checkbox("##", &enable)) {
            json[path] = enable;
        }
        ImGui::PopID();
    }
    void showIntegerEdit(EDIT_COMMON_PARAMS, const int default_value = 0, const int min_value = 0)
    {
        int value = json.value(path, default_value);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str(i18n_id));
        ImGui::PushID(i18n_c_str(i18n_id));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::InputInt("##", &value)) {
            json[path] = (std::max)(min_value, value);
        }
        ImGui::PopID();
    }
    void showNumberSliderEdit(EDIT_COMMON_PARAMS, const double default_value = 0, const double min_value = 0.0, const double max_value = 1.0)
    {
        double value = json.value(path, default_value);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str(i18n_id));
        ImGui::PushID(i18n_c_str(i18n_id));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::SliderScalar("##", ImGuiDataType_Double, &value, &min_value, &max_value)) {
            json[path] = value;
        }
        ImGui::PopID();
    }
    void showTextFieldEdit(EDIT_COMMON_PARAMS, const std::string& default_value)
    {
        std::string value = json.value(path, default_value);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str(i18n_id));
        ImGui::PushID(i18n_c_str(i18n_id));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::InputText("##", &value)) {
            json[path] = value;
        }
        ImGui::PopID();
    }
#undef EDIT_COMMON_PARAMS
}

// Logging Level

namespace
{
    enum class LoggingLevel
    {
        debug,
        info,
        warn,
        error,
        fatal,
    };
    LoggingLevel toLoggingLevel(const std::string& s)
    {
        if(s == "debug"sv)
            return LoggingLevel::debug;
        if(s == "info"sv)
            return LoggingLevel::info;
        if(s == "warn"sv)
            return LoggingLevel::warn;
        if(s == "error"sv)
            return LoggingLevel::error;
        if(s == "fatal"sv)
            return LoggingLevel::fatal;
        return LoggingLevel::info;
    }
    std::string_view toLocalizedStringView(const LoggingLevel l)
    {
        switch(l) {
            case LoggingLevel::debug: return i18n("config-logging-level-debug"sv);
            case LoggingLevel::info: return i18n("config-logging-level-info"sv);
            case LoggingLevel::warn: return i18n("config-logging-level-warn"sv);
            case LoggingLevel::error: return i18n("config-logging-level-error"sv);
            case LoggingLevel::fatal: return i18n("config-logging-level-fatal"sv);
            default: return i18n("config-logging-level-info"sv);
        }
    }
    std::string_view toStringView(const LoggingLevel l)
    {
        switch(l) {
            case LoggingLevel::debug: return "debug"sv;
            case LoggingLevel::info: return "info"sv;
            case LoggingLevel::warn: return "warn"sv;
            case LoggingLevel::error: return "error"sv;
            case LoggingLevel::fatal: return "fatal"sv;
            default: return "info"sv;
        }
    }
    void showLoggingEnableEdit(nlohmann::json& json, const nlohmann::json_pointer<std::string>& path, const bool default_value = false)
    {
        showCheckBoxEdit(json, path, "common-enable"sv, default_value);
    }
    void showLoggingThresholdEdit(nlohmann::json& json, const nlohmann::json_pointer<std::string>& path)
    {
        LoggingLevel threshold = toLoggingLevel(json.value(path, "info"s));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str("config-logging-level"sv));
        ImGui::PushID(i18n_c_str("config-logging-level"sv));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("##", toLocalizedStringView(threshold).data())) {
            bool changed = false;
            if(ImGui::Selectable(i18n_c_str("config-logging-level-debug"sv), threshold == LoggingLevel::debug)) {
                threshold = LoggingLevel::debug;
                changed = true;
            }
            if(ImGui::Selectable(i18n_c_str("config-logging-level-info"sv), threshold == LoggingLevel::info)) {
                threshold = LoggingLevel::info;
                changed = true;
            }
            if(ImGui::Selectable(i18n_c_str("config-logging-level-warn"sv), threshold == LoggingLevel::warn)) {
                threshold = LoggingLevel::warn;
                changed = true;
            }
            if(ImGui::Selectable(i18n_c_str("config-logging-level-error"sv), threshold == LoggingLevel::error)) {
                threshold = LoggingLevel::error;
                changed = true;
            }
            if(ImGui::Selectable(i18n_c_str("config-logging-level-fatal"sv), threshold == LoggingLevel::fatal)) {
                threshold = LoggingLevel::fatal;
                changed = true;
            }
            if(changed) {
                json[path] = toStringView(threshold);
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
}

// Renderer Driver

namespace
{
    // The driver vocabulary belongs to SDL, so enumerate it at runtime rather than
    // hardcoding a list that would go stale the next time SDL gains a backend.
    void showRendererDriverEdit(nlohmann::json& json, const nlohmann::json_pointer<std::string>& path)
    {
        const std::string current = json.value(path, "auto"s);
        const std::string automatic = "auto "s + std::string(i18n("common-default2"sv));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str("config-graphics-system-renderer-driver"sv));
        ImGui::PushID(i18n_c_str("config-graphics-system-renderer-driver"sv));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("##", current == "auto"sv ? automatic.c_str() : current.c_str())) {
            if(ImGui::Selectable(automatic.c_str(), current == "auto"sv)) {
                json[path] = "auto"sv;
            }
            for(int i = 0; i < SDL_GetNumGPUDrivers(); ++i) {
                char const* const driver = SDL_GetGPUDriver(i);
                if(driver == nullptr) {
                    continue;
                }
                // Offer only drivers that can both run here and consume the shader formats
                // the engine produces. A driver named explicitly is never fallen back from,
                // so listing an unusable one would just hand the user a startup failure.
                constexpr SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
                if(!SDL_GPUSupportsShaderFormats(formats, driver)) {
                    continue;
                }
                if(ImGui::Selectable(driver, current == driver)) {
                    json[path] = driver;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }
}

constexpr UINT WINDOW_SIZE_X = 400;
constexpr UINT WINDOW_SIZE_Y = 300;

struct Window
{
    core::SmartReference<core::IWindow> window;
    SDL_WindowID window_id{};
    UINT window_width{ WINDOW_SIZE_X };
    UINT window_height{ WINDOW_SIZE_Y };
    UINT pixel_width{ WINDOW_SIZE_X };
    UINT pixel_height{ WINDOW_SIZE_Y };
    float window_scale{ 1.0f };

    std::unique_ptr<core::Graphics::IGraphicsRuntime> graphics;

    nlohmann::json config_json = nlohmann::json::object();
    bool show_advance{ false };

    bool is_open{};
    bool want_exit{};
    bool is_updating{};
    bool is_rendering{};
    bool event_watch_registered{};
    bool graphics_initialized{};
    bool imgui_context_created{};
    bool imgui_platform_initialized{};
    bool imgui_renderer_initialized{};

    static bool SDLCALL EventWatch(void* const userdata, SDL_Event* const event)
    {
        auto* const self = static_cast<Window*>(userdata);
        if(self == nullptr || event == nullptr || event->type != SDL_EVENT_WINDOW_EXPOSED || !self->is_open) {
            return true;
        }
        if(event->window.windowID == self->window_id) {
            self->RenderCurrentFrame();
        }
        return true;
    }

    int Run()
    {
        if(!SDL_AddEventWatch(&Window::EventWatch, this)) {
            throw std::runtime_error(std::string("SDL_AddEventWatch failed: ") + SDL_GetError());
        }
        event_watch_registered = true;

        while(!want_exit) {
            SDL_Event event{};
            while(SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                core::WindowSDL3::dispatchSDLEvent(event);
                if(event.type == SDL_EVENT_QUIT) {
                    want_exit = true;
                    continue;
                }
                if(event.type == SDL_EVENT_SYSTEM_THEME_CHANGED) {
                    ApplyStyle();
                    continue;
                }
                if(event.type < SDL_EVENT_WINDOW_FIRST || event.type > SDL_EVENT_WINDOW_LAST || event.window.windowID != window_id) {
                    continue;
                }

                switch(event.type) {
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        want_exit = true;
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                        window_width = static_cast<UINT>(event.window.data1);
                        window_height = static_cast<UINT>(event.window.data2);
                        break;
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        OnPixelSize(static_cast<UINT>(event.window.data1), static_cast<UINT>(event.window.data2));
                        break;
                    case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                        OnScaling(window->getDPIScaling());
                        break;
                    default:
                        break;
                }
            }

            if(!want_exit) {
                OnUpdate();
                SDL_Delay(10);
            }
        }

        SDL_RemoveEventWatch(&Window::EventWatch, this);
        event_watch_registered = false;
        return 0;
    }
    void OnPixelSize(const UINT width, const UINT height)
    {
        pixel_width = width;
        pixel_height = height;
        if(graphics && width > 0 && height > 0 && !graphics->swapChain()->setCanvasSize({ width, height })) {
            throw std::runtime_error("Resize Settings canvas failed");
        }
    }
    void OnUpdate()
    {
        if(!is_open || is_updating)
            return;

        is_updating = true;
        graphics->newImGuiFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();

        Layout();

        ImGui::EndFrame();
        ImGui::Render();
        is_updating = false;
        RenderCurrentFrame();
    }
    void RenderCurrentFrame()
    {
        if(!is_open || is_updating || is_rendering || ImGui::GetDrawData() == nullptr) {
            return;
        }

        is_rendering = true;
        const auto status = graphics->beginFrame();
        if(status == core::Graphics::FrameStatus::Ready) {
            graphics->renderImGui(ImGui::GetDrawData());
            if(!graphics->submitFrame(true)) {
                throw std::runtime_error("Present Settings frame failed");
            }
        } else if(status == core::Graphics::FrameStatus::Failed) {
            throw std::runtime_error("Begin Settings frame failed");
        }
        is_rendering = false;
    }
    void OnScaling(const float scale)
    {
        window_scale = scale > 0.0f ? scale : 1.0f;
        UpdateStyleAndFont();
    }

    void wantExit()
    {
        want_exit = true;
        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;
        if(!SDL_PushEvent(&event)) {
            spdlog::error("[sdl] SDL_PushEvent failed: {}", SDL_GetError());
        }
    }
    void UpdateStyleAndFont()
    {
        if(!ImGui::GetCurrentContext())
            return;
        ApplyStyle();
    }
    void ApplyStyle()
    {
        if(!ImGui::GetCurrentContext())
            return;

        const float scaling = window_scale;

        ImGuiStyle style;
        if(SDL_GetSystemTheme() == SDL_SYSTEM_THEME_DARK)
            ImGui::StyleColorsDark(&style);
        else
            ImGui::StyleColorsLight(&style);
        style.ChildBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        style.TabBorderSize = 1.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.TabRounding = 0.0f;
        style.WindowRounding = 0.0f;
        style.ScaleAllSizes(scaling);
        style.FontSizeBase = 16.0f;
        style.FontScaleMain = 1.0f;
        style.FontScaleDpi = scaling;
        ImGui::GetStyle() = style;
    }
    void LayoutApplicationTab()
    {
        showTextFieldEdit(config_json, "/application/uuid"_json_pointer, "config-application-uuid"sv, ""s);
        showCheckBoxEdit(config_json, "/application/single_instance"_json_pointer, "config-application-single-instance"sv, false);
    }
    void LayoutDebugTab()
    {
        showCheckBoxEdit(config_json, "/debug/track_window_focus"_json_pointer, "config-debug-track-window-focus"sv, false);
    }
    void LayoutLoggingTab()
    {
        ImGui::PushID(1);
        ImGui::SeparatorText(i18n_c_str("config-logging-debugger"));
        showLoggingEnableEdit(config_json, "/logging/debugger/enable"_json_pointer);
        showLoggingThresholdEdit(config_json, "/logging/debugger/threshold"_json_pointer);
        ImGui::PopID();

        ImGui::PushID(2);
        ImGui::SeparatorText(i18n_c_str("config-logging-console"));
        showLoggingEnableEdit(config_json, "/logging/console/enable"_json_pointer);
        showLoggingThresholdEdit(config_json, "/logging/console/threshold"_json_pointer);
        showCheckBoxEdit(config_json, "/logging/console/preserve"_json_pointer, "config-logging-console-preserve"sv);
        ImGui::PopID();

        ImGui::PushID(3);
        ImGui::SeparatorText(i18n_c_str("config-logging-file"));
        showLoggingEnableEdit(config_json, "/logging/file/enable"_json_pointer, true);
        showLoggingThresholdEdit(config_json, "/logging/file/threshold"_json_pointer);
        showTextFieldEdit(config_json, "/logging/file/path"_json_pointer, "config-logging-file-path"sv, LUASTG_LOGGING_DEFAULT_FILE_PATH ""s);
        ImGui::PopID();

        ImGui::PushID(4);
        ImGui::SeparatorText(i18n_c_str("config-logging-rolling-file"));
        showLoggingEnableEdit(config_json, "/logging/rolling_file/enable"_json_pointer);
        showLoggingThresholdEdit(config_json, "/logging/rolling_file/threshold"_json_pointer);
        showTextFieldEdit(config_json, "/logging/rolling_file/path"_json_pointer, "config-logging-rolling-file-path"sv, ""s);
        showIntegerEdit(config_json, "/logging/rolling_file/max_history"_json_pointer, "config-logging-rolling-file-max-history"sv, 10, 1);
        ImGui::PopID();
    }
    void LayoutTimingTab()
    {
        showIntegerEdit(config_json, "/timing/frame_rate"_json_pointer, "config-timing-frame-rate"sv, 60, 1);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(ImColor(1.0f, 0.1f, 0.1f), i18n_c_str("config-timing-frame-rate-warn"sv));
    }
    void LayoutWindowTab()
    {
        if(show_advance) {
            showTextFieldEdit(config_json, "/window/title"_json_pointer, "config-window-title"sv, LUASTG_INFO ""s);
            showCheckBoxEdit(config_json, "/window/cursor_visible"_json_pointer, "config-window-cursor-visible"sv, true);
        }
    }
    void LayoutGraphicsSystemTab()
    {
        const int width = config_json.value("/graphics_system/width"_json_pointer, 640);
        const int height = config_json.value("/graphics_system/height"_json_pointer, 480);
        const auto resolution = std::format("{}x{}"sv, width, height);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(i18n_c_str("config-graphics-system-resolution"sv));
        ImGui::PushID(i18n_c_str("config-graphics-system-resolution"sv));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::BeginCombo("##", resolution.c_str())) {
            for(double s = 1.0; s < 9.00001; s += 0.25) {
                const int w = static_cast<int>(640 * s);
                const int h = static_cast<int>(480 * s);
                const auto r = std::format("{}x{}", w, h);
                if(ImGui::Selectable(r.c_str(), width == w && height == h)) {
                    config_json["/graphics_system/width"_json_pointer] = w;
                    config_json["/graphics_system/height"_json_pointer] = h;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();

        showCheckBoxEdit(config_json, "/graphics_system/fullscreen"_json_pointer, "config-graphics-system-fullscreen"sv, false);
        showCheckBoxEdit(config_json, "/graphics_system/vsync"_json_pointer, "config-graphics-system-vsync"sv, false);
        // Not gated behind the advanced toggle: with adapter selection gone, this is the only
        // graphics knob left for a user whose machine misbehaves on one driver.
        showRendererDriverEdit(config_json, "/graphics_system/renderer_driver"_json_pointer);
    }
    void LayoutAudioSystemTab()
    {
        // TODO: "/audio_system/preferred_output_name"_json_pointer and max voice count.
        showNumberSliderEdit(config_json, "/audio_system/master_volume"_json_pointer, "config-audio-system-master-volume"sv, 1.0);
        showNumberSliderEdit(config_json, "/audio_system/sound_effect_volume"_json_pointer, "config-audio-system-sound-effect-volume"sv, 1.0);
        showNumberSliderEdit(config_json, "/audio_system/music_volume"_json_pointer, "config-audio-system-music-volume"sv, 1.0);
    }
    void Layout()
    {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(window_width), static_cast<float>(window_height)), ImGuiCond_Always);
        if(ImGui::Begin("##MainWindow", nullptr, (ImGuiWindowFlags_NoDecoration ^ ImGuiWindowFlags_NoScrollbar) | ImGuiWindowFlags_NoBackground)) {
            int select_lang = (int)i18n_map_index;
            char const* langs[2] = {
                i18n_c_str("language-chinese"),
                i18n_c_str("language-english"),
            };
            if(ImGui::Combo(i18n_c_str("setting-language"), &select_lang, langs, 2)) {
                i18n_map_index = (Language)select_lang;
                updateTitle();
            }

            ImGui::Separator();

            if(ImGui::Button(i18n_c_str("setting-cancel"))) {
                wantExit();
            }
            ImGui::SameLine();
            if(ImGui::Button(i18n_c_str("setting-save"))) {
                saveConfigToJson();
                wantExit();
            }
            ImGui::SameLine();
            ImGui::Checkbox(i18n_c_str("config-show-advance"), &show_advance);

            if(ImGui::BeginTabBar("##SettingTabs")) {
                if(show_advance && ImGui::BeginTabItem(i18n_c_str("config-application"))) {
                    LayoutApplicationTab();
                    ImGui::EndTabItem();
                }
                if(show_advance && ImGui::BeginTabItem(i18n_c_str("config-debug"))) {
                    LayoutDebugTab();
                    ImGui::EndTabItem();
                }
                if(show_advance && ImGui::BeginTabItem(i18n_c_str("config-logging"))) {
                    LayoutLoggingTab();
                    ImGui::EndTabItem();
                }
                if(show_advance && ImGui::BeginTabItem(i18n_c_str("config-timing"))) {
                    LayoutTimingTab();
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem(i18n_c_str("config-window"))) {
                    LayoutWindowTab();
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem(i18n_c_str("config-graphics-system"))) {
                    LayoutGraphicsSystemTab();
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem(i18n_c_str("config-audio-system"))) {
                    LayoutAudioSystemTab();
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void moveToCenter()
    {
        if(!window->setCentered(false)) {
            throw std::runtime_error("IWindow::setCentered failed");
        }
    }
    void updateWindowSize()
    {
        if(!window->setSize({ WINDOW_SIZE_X, WINDOW_SIZE_Y })) {
            throw std::runtime_error("IWindow::setSize failed");
        }
        if(!SDL_SyncWindow(window->getSDLWindow())) {
            throw std::runtime_error(std::string("SDL_SyncWindow timed out after resizing the window: ") + SDL_GetError());
        }
        const auto logical_size = window->getSize();
        window_width = logical_size.x;
        window_height = logical_size.y;
        const auto drawable_size = window->getPixelSize();
        pixel_width = drawable_size.x;
        pixel_height = drawable_size.y;
    }
    void updateTitle()
    {
        window->setTitleText(i18n("window-title"));
    }

    void loadConfigFromJson()
    {
        if(std::filesystem::is_regular_file(L"config.json")) {
            std::ifstream file(L"config.json", std::ios::in | std::ios::binary);
            if(file.is_open()) {
                config_json = nlohmann::json::object();
                file >> config_json;
            }
        }
    }
    void saveConfigToJson()
    {
        if(config_json.contains("graphics_system") && config_json["graphics_system"].is_object()) {
            auto& graphics_config = config_json["graphics_system"];
            for(auto const* key : { "preferred_device_name", "allow_software_device", "allow_exclusive_fullscreen", "allow_modern_swap_chain", "allow_direct_composition" }) {
                graphics_config.erase(key);
            }
        }
        std::ofstream file(L"config.json", std::ios::out | std::ios::binary | std::ios::trunc);
        if(file.is_open()) {
            file << config_json.dump(4);
        }
    }

    Window()
    {
        try {
            // Before the graphics runtime, which is created on the configured driver.
            loadConfigFromJson();
            if(!core::IWindow::create({ WINDOW_SIZE_X, WINDOW_SIZE_Y }, "", core::WindowFrameStyle::Normal, false, window.put())) {
                throw std::runtime_error("IWindow::create failed");
            }
            window_id = SDL_GetWindowID(window->getSDLWindow());
            if(window_id == 0) {
                throw std::runtime_error(std::string("SDL_GetWindowID failed: ") + SDL_GetError());
            }
            window_scale = window->getDPIScaling();
            if(window_scale <= 0.0f) {
                throw std::runtime_error(std::string("SDL_GetWindowDisplayScale failed: ") + SDL_GetError());
            }
            updateWindowSize();
            moveToCenter();
            updateTitle();

            if(!window->setVisible(true)) {
                throw std::runtime_error("IWindow::setVisible failed");
            }

            // The Configurer is where a bad driver name gets fixed, so it has to start even
            // when the configured driver cannot run here: fall back and let the user choose.
            const std::string driver = config_json.value("/graphics_system/renderer_driver"_json_pointer, "auto"s);
            try {
                graphics = core::Graphics::IGraphicsRuntime::create(window.get(), driver);
            } catch(const std::exception& error) {
                if(driver == "auto"s) {
                    throw;
                }
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Configured renderer driver \"%s\" is unusable (%s); showing settings on an automatically selected driver", driver.c_str(), error.what());
                graphics = core::Graphics::IGraphicsRuntime::create(window.get(), "auto");
            }
            if(!graphics->swapChain()->setCanvasSize({ pixel_width, pixel_height }) ||
                !graphics->swapChain()->setWindowMode({ pixel_width, pixel_height })) {
                throw std::runtime_error("Create Settings presentation failed");
            }
            graphics->swapChain()->setScalingMode(core::Graphics::SwapChainScalingMode::Stretch);
            graphics_initialized = true;
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            imgui_context_created = true;
            ImGui::GetIO().IniFilename = NULL;
            if(!LUASTG_IMGUI_SDL_INIT(window->getSDLWindow())) {
                throw std::runtime_error("ImGui SDL platform initialization failed.");
            }
            imgui_platform_initialized = true;
            if(!graphics->initializeImGui()) {
                throw std::runtime_error("Initialize Settings ImGui renderer failed.");
            }
            imgui_renderer_initialized = true;

            ImFontConfig font_cfg;
            font_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_NoHinting | ImGuiFreeTypeLoaderFlags_LoadColor;
            ImFont* font = ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttc", 16.0f * window_scale, &font_cfg, nullptr);
            if(!font) {
                font = ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msyh.ttf", 16.0f * window_scale, &font_cfg, nullptr);
            }
            if(!font) {
                throw std::runtime_error("ImFontAtlas::AddFontFromFileTTF failed.");
            }

            UpdateStyleAndFont();
            is_open = true;
        } catch(...) {
            Shutdown();
            throw;
        }
    }
    void Shutdown() noexcept
    {
        if(event_watch_registered) {
            SDL_RemoveEventWatch(&Window::EventWatch, this);
            event_watch_registered = false;
        }
        is_open = false;
        if(imgui_renderer_initialized) {
            graphics->shutdownImGui();
            imgui_renderer_initialized = false;
        }
        if(imgui_platform_initialized) {
            ImGui_ImplSDL3_Shutdown();
            imgui_platform_initialized = false;
        }
        if(imgui_context_created) {
            ImGui::DestroyContext();
            imgui_context_created = false;
        }
        if(graphics_initialized) {
            graphics.reset();
            graphics_initialized = false;
        }
        window.reset();
        window_id = 0;
    }
    ~Window()
    {
        Shutdown();
    }
};

int
main(int, char**)
{
    core::SdlRuntime sdl_runtime;
    if(!sdl_runtime.initialize()) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    try {
        Window window;
        return window.Run();
    } catch(std::runtime_error const& e) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", e.what(), nullptr);
    }
    return EXIT_FAILURE;
}
