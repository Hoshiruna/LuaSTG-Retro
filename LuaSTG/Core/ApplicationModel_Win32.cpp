#include "Core/ApplicationModel_Win32.hpp"
#include "Core/i18n.hpp"
#include "core/Configuration.hpp"
#include "core/InputSystem.hpp"
#include "Platform/WindowsVersion.hpp"
#include "Platform/ProcessorInfo.hpp"
#include "sdl/EventDispatcher.hpp"
#include "sdl/Window.hpp"
#include <SDL3/SDL.h>

namespace core
{
    double FrameRateController::indexFPS(size_t idx)
    {
        return fps_[(fps_index_ + std::size(fps_) - 1 - idx) % std::size(fps_)];
    }

    double FrameRateController::udateData(int64_t curr)
    {
        // 更新各项数值
        double const fps = (double)freq_ / (double)(curr - last_);
        double const s = 1.0 / fps;
        total_frame_ += 1;
        total_time_ += s;
        fps_[fps_index_] = fps;
        fps_index_ = (fps_index_ + 1) % std::size(fps_);
        last_ = curr;

        // 更新统计数据
        fps_min_ = DBL_MAX;
        fps_max_ = -DBL_MAX;
        size_t const total_history = total_frame_ < std::size(fps_) ? (size_t)total_frame_ : std::size(fps_);
        if(total_history > 0) {
            double total_fps = 0.0;
            double total_time = 0.0;
            size_t history_count = 0;
            for(size_t i = 0; i < total_history; i += 1) {
                double const fps_history = indexFPS(i);
                total_fps += fps_history;
                fps_min_ = std::min(fps_min_, fps_history);
                fps_max_ = std::max(fps_max_, fps_history);
                total_time += 1.0 / fps_history;
                history_count += 1;
                if(total_time >= 0.25) {
                    break;
                }
            }
            fps_avg_ = total_fps / (double)history_count;
        } else {
            fps_avg_ = 0.0;
            fps_min_ = 0.0;
            fps_max_ = 0.0;
        }

        return s;
    }
    bool FrameRateController::arrive()
    {
        // 先获取当前计数器
        LARGE_INTEGER curr_{};
        QueryPerformanceCounter(&curr_);
        // 判断
        if((curr_.QuadPart - last_) < wait_) {
            return false;
        } else {
            udateData(curr_.QuadPart);
            return true;
        }
    }
    double FrameRateController::update()
    {
        // 先获取当前计数器
        LARGE_INTEGER curr_{};
        QueryPerformanceCounter(&curr_);

        // 当设备运行时间足够长后，即使是 int64 也会溢出
        if(curr_.QuadPart < last_) {
            total_frame_ += 1;
            total_time_ += 1.0 / indexFPS(0);
            last_ = curr_.QuadPart;
            return indexFPS(0);
        }

        // 在启用了高精度计时器的情况下，可以用 Sleep 等待，不需要占用太多 CPU
        LONGLONG const sleep_ms = (((wait_ - (curr_.QuadPart - last_)) - _2ms_) * 1000ll) / freq_;
        if(sleep_ms > 0) {
            Sleep((DWORD)sleep_ms);
        }

        // 轮询等待
        do {
            QueryPerformanceCounter(&curr_);
        } while((curr_.QuadPart - last_) < wait_);

        return udateData(curr_.QuadPart);
    }

    uint32_t FrameRateController::getTargetFPS()
    {
        return (uint32_t)target_fps_;
    }
    void FrameRateController::setTargetFPS(uint32_t target_FPS)
    {
        target_fps_ = (double)(target_FPS > 0 ? target_FPS : 1);
        LARGE_INTEGER lli{};
        QueryPerformanceFrequency(&lli);
        freq_ = lli.QuadPart;
        wait_ = (LONGLONG)((double)freq_ / target_fps_);
        _2ms_ = (2ll * freq_) / 1000ll;
    }
    double FrameRateController::getFPS()
    {
        return indexFPS(0);
    }
    uint64_t FrameRateController::getTotalFrame()
    {
        return total_frame_;
    }
    double FrameRateController::getTotalTime()
    {
        return total_time_;
    }
    double FrameRateController::getAvgFPS()
    {
        return fps_avg_;
    }
    double FrameRateController::getMinFPS()
    {
        return fps_min_;
    }
    double FrameRateController::getMaxFPS()
    {
        return fps_max_;
    }

    FrameRateController::FrameRateController(uint32_t target_FPS)
    {
        timeBeginPeriod(1);
        setTargetFPS(target_FPS);
        LARGE_INTEGER lli{};
        QueryPerformanceCounter(&lli);
        last_ = lli.QuadPart;
    }
    FrameRateController::~FrameRateController()
    {
        timeEndPeriod(1);
    }
}

namespace core
{
    // 基于时间戳的帧率控制器
    // 当帧率有波动时，追赶或者等待更长时间

    inline int64_t winQPC()
    {
        LARGE_INTEGER ll = {};
        QueryPerformanceCounter(&ll);
        return ll.QuadPart;
    }
    inline int64_t winQPF()
    {
        LARGE_INTEGER ll = {};
        QueryPerformanceFrequency(&ll);
        return ll.QuadPart;
    }

    bool TimeStampFrameRateController::arrive()
    {
        // 计算下一个要到达的时间戳
        int64_t const target_pc_ = begin_pc_ + (frame_count_ + 1) * clock_pcpf_;
        int64_t cur_pc_ = winQPC();
        return cur_pc_ >= target_pc_;
    }
    double TimeStampFrameRateController::update()
    {
        // 计算下一个要到达的时间戳
        int64_t const target_pc_ = begin_pc_ + (frame_count_ + 1) * clock_pcpf_;
        int64_t cur_pc_ = winQPC();
        if(cur_pc_ > target_pc_) {
            // 已经超过
            if((cur_pc_ - target_pc_) >= (clock_pcpf_ * 10)) {
                // 落后超过 10 帧，放弃追赶，并设置新的基准点
                begin_pc_ = cur_pc_;
                frame_count_ = 0;
            } else {
                // 立即推进 1 帧
                frame_count_ += 1;
            }
        } else {
            // 当需要等待的时间 Tms 大于 2ms 时，用Sleep 等待 (T - 2)ms
            int64_t const err_pc_ = clock_freq_ * 2 / 1000;
            int64_t const ddt_pc_ = target_pc_ - cur_pc_;
            if(ddt_pc_ > err_pc_) {
                DWORD const ms_ = (DWORD)((ddt_pc_ - err_pc_) / (err_pc_ / 2));
                Sleep(ms_);
            }
            // 精确的轮询等待
            for(;;) {
                cur_pc_ = winQPC();
                if(cur_pc_ >= target_pc_) {
                    break;
                }
            }
            // 推进 1 帧
            frame_count_ += 1;
        }
        // 刷新数值
        int64_t const delta_pc_ = cur_pc_ - last_pc_;
        delta_time_ = (double)delta_pc_ / (double)clock_freq_;
        last_pc_ = cur_pc_;
        return delta_time_;
    }

    uint32_t TimeStampFrameRateController::getTargetFPS()
    {
        return (uint32_t)target_fps_;
    }
    void TimeStampFrameRateController::setTargetFPS(uint32_t target_FPS)
    {
        if(std::abs(target_fps_ - (double)target_FPS) < 0.01) {
            return;
        }

        target_fps_ = (double)target_FPS;
        target_spf_ = 1.0 / target_fps_;

        clock_freq_ = winQPF();
        clock_pcpf_ = (int64_t)(target_spf_ * (double)clock_freq_);

        begin_pc_ = winQPC();
        last_pc_ = begin_pc_;
        frame_count_ = 0;
    }
    double TimeStampFrameRateController::getFPS()
    {
        return 1.0 / delta_time_;
    }
    uint64_t TimeStampFrameRateController::getTotalFrame()
    {
        return 0;
    }
    double TimeStampFrameRateController::getTotalTime()
    {
        return 0.0;
    }
    double TimeStampFrameRateController::getAvgFPS()
    {
        return getFPS();
    }
    double TimeStampFrameRateController::getMinFPS()
    {
        return getFPS();
    }
    double TimeStampFrameRateController::getMaxFPS()
    {
        return getFPS();
    }

    TimeStampFrameRateController::TimeStampFrameRateController(uint32_t target_FPS)
    {
        timeBeginPeriod(1);
        setTargetFPS(target_FPS);
    }
    TimeStampFrameRateController::~TimeStampFrameRateController()
    {
        timeEndPeriod(1);
    }
}

namespace core
{
    static std::string bytes_count_to_string(DWORDLONG size)
    {
        int count = 0;
        char buffer[64] = {};
        if(size < 1024llu) // B
        {
            count = std::snprintf(buffer, 64, "%u B", (unsigned int)size);
        } else if(size < (1024llu * 1024llu)) // KB
        {
            count = std::snprintf(buffer, 64, "%.2f KiB", (double)size / 1024.0);
        } else if(size < (1024llu * 1024llu * 1024llu)) // MB
        {
            count = std::snprintf(buffer, 64, "%.2f MiB", (double)size / 1048576.0);
        } else // GB
        {
            count = std::snprintf(buffer, 64, "%.2f GiB", (double)size / 1073741824.0);
        }
        return std::string(buffer, count);
    }
    static void get_system_memory_status()
    {
        MEMORYSTATUSEX info = { sizeof(MEMORYSTATUSEX) };
        if(GlobalMemoryStatusEx(&info)) {
            spdlog::info("[core] System memory usage:\n"
                         "    Physical memory usage: {}%\n"
                         "    Total physical memory: {}\n"
                         "    Available physical memory: {}\n"
                         "    Current process commit memory limit: {}\n"
                         "    Current process available commit memory: {}\n"
                         "    Current process user-mode address space limit*1: {}\n"
                         "    Current process available user-mode address space: {}\n"
                         "        *1 This value reflects the maximum memory actually available to this program. "
                         "For 32-bit applications, it is usually 2 GB, but it may range from 1 GB to 3 GB "
                         "after modifying the Windows registry.",
                info.dwMemoryLoad,
                bytes_count_to_string(info.ullTotalPhys),
                bytes_count_to_string(info.ullAvailPhys),
                bytes_count_to_string(info.ullTotalPageFile),
                bytes_count_to_string(info.ullAvailPageFile),
                bytes_count_to_string(info.ullTotalVirtual),
                bytes_count_to_string(info.ullAvailVirtual));
        } else {
            spdlog::error("[core] Failed to retrieve system memory usage");
        }
    }

    struct ScopeTimer
    {
        LARGE_INTEGER freq{};
        LARGE_INTEGER last{};
        double& t;
        ScopeTimer(double& v_ref)
            : t(v_ref)
        {
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&last);
        }
        ~ScopeTimer()
        {
            LARGE_INTEGER curr{};
            QueryPerformanceCounter(&curr);
            t = (double)(curr.QuadPart - last.QuadPart) / (double)freq.QuadPart;
        }
    };

    bool SDLCALL ApplicationModel_Win32::sdlEventWatch(void* const userdata, SDL_Event* const event)
    {
        auto* const self = static_cast<ApplicationModel_Win32*>(userdata);
        if(self == nullptr || event == nullptr || event->type != SDL_EVENT_WINDOW_EXPOSED || !self->m_running) {
            return true;
        }

        auto* const window = self->m_window->getSDLWindow();
        if(window != nullptr && event->window.windowID == SDL_GetWindowID(window)) {
            self->renderExposedFrame();
        }
        return true;
    }

    void ApplicationModel_Win32::renderExposedFrame()
    {
        if(m_updating || m_rendering || m_exit_flag.load(std::memory_order_relaxed)) {
            return;
        }

        m_rendering = true;
        m_swapchain->applyRenderAttachment();
        m_swapchain->clearRenderAttachment();
        if(m_listener->onRender()) {
            m_swapchain->present();
            TracyD3D11Collect(m_device->GetTracyContext());
        }
        m_rendering = false;
    }

    bool ApplicationModel_Win32::runSingleThread()
    {
        // 设置线程优先级为高，并尽量让它运行在同一个 CPU 核心上，降低切换开销
        SetThreadAffinityMask(GetCurrentThread(), 1);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

        // 初次收集诊断信息
        TracyD3D11Collect(m_device->GetTracyContext());
        FrameMark;
        {
            tracy_zone_scoped_with_name("OnInitWait");
            m_swapchain->waitFrameLatency();
            m_p_frame_rate_controller->update();
        }

        if(!SDL_AddEventWatch(&ApplicationModel_Win32::sdlEventWatch, this)) {
            spdlog::error("[sdl] SDL_AddEventWatch failed: {}", SDL_GetError());
            return false;
        }

        m_exit_flag.store(false, std::memory_order_relaxed);
        m_running = true;
        while(!m_exit_flag.load(std::memory_order_relaxed)) {
            InputSystem::getInstance().beginFrame();
            SDL_Event event{};
            while(SDL_PollEvent(&event)) {
                SDLEventDispatcher::dispatch(event);
                InputSystem::getInstance().processEvent(event);
                WindowSDL3::dispatchSDLEvent(event);
                if(event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                    m_exit_flag.store(true, std::memory_order_relaxed);
                }
            }

            if(m_exit_flag.load(std::memory_order_relaxed)) {
                break;
            }
            runFrame();
        }

        m_running = false;
        SDL_RemoveEventWatch(&ApplicationModel_Win32::sdlEventWatch, this);
        return true;
    }
    void ApplicationModel_Win32::runFrame()
    {
        size_t const i = (m_framestate_index + 1) % 2;
        FrameStatistics& d = m_framestate[i];
        ScopeTimer gt(d.total_time);
        size_t const next_frame_query_index = (m_frame_query_index + 1) % m_frame_query_list.size();
        FrameQuery& frame_query = m_frame_query_list[next_frame_query_index];

        bool update_result = false;

        // 更新
        {
            tracy_zone_scoped_with_name("OnUpdate");
            ScopeTimer t(d.update_time);
            m_updating = true;
            update_result = m_listener->onUpdate();
            m_updating = false;
        }

        if(update_result && !m_rendering) {
            m_rendering = true;
            bool render_result = false;
            tracy_zone_scoped_with_name("OnRender");
            tracy_d3d11_context_zone(m_device->GetTracyContext(), "OnRender");
            {
                ScopeTimer t(d.render_time);
                frame_query.begin(); // TODO: enable/disable by configuration
                m_swapchain->applyRenderAttachment();
                m_swapchain->clearRenderAttachment();
                render_result = m_listener->onRender();
                frame_query.end(); // TODO: enable/disable by configuration
            }

            if(render_result) {
                tracy_zone_scoped_with_name("OnPresent");
                ScopeTimer t(d.present_time);
                m_swapchain->present();
                TracyD3D11Collect(m_device->GetTracyContext());
            }
            m_rendering = false;
        }

        // 等待下一帧
        {
            tracy_zone_scoped_with_name("OnWait");
            ScopeTimer t(d.wait_time);
            m_swapchain->waitFrameLatency();
            m_p_frame_rate_controller->update();
        }

        m_framestate_index = i;
        m_frame_query_index = next_frame_query_index;
        FrameMark;
    }

    FrameStatistics ApplicationModel_Win32::getFrameStatistics()
    {
        return m_framestate[m_framestate_index];
    }
    FrameRenderStatistics ApplicationModel_Win32::getFrameRenderStatistics()
    {
        FrameQuery& frame_query = m_frame_query_list[m_frame_query_index];
        FrameRenderStatistics statistics{};
        statistics.render_time = frame_query.getTime();
        return statistics;
    }

    void ApplicationModel_Win32::requestExit()
    {
        m_exit_flag.store(true, std::memory_order_relaxed);
        SDL_Event event{};
        event.type = SDL_EVENT_QUIT;
        if(!SDL_PushEvent(&event)) {
            spdlog::error("[sdl] SDL_PushEvent failed: {}", SDL_GetError());
        }
    }
    bool ApplicationModel_Win32::run()
    {
        return runSingleThread();
    }

    ApplicationModel_Win32::ApplicationModel_Win32(IApplicationEventListener* p_listener)
        : m_listener(p_listener)
    {
        assert(m_listener);
        if(!m_sdl_runtime.initialize()) {
            throw std::runtime_error("SdlRuntime::initialize");
        }
        spdlog::info("[core] System: {}", Platform::WindowsVersion::GetName());
        spdlog::info("[core] Kernel: {}", Platform::WindowsVersion::GetKernelVersionString());
        spdlog::info("[core] CPU: {}", Platform::ProcessorInfo::name());
        if(m_steady_frame_rate_controller.available()) {
            spdlog::info("[core] High Resolution Waitable Timer available, enable SteadyFrameRateController");
            m_p_frame_rate_controller = &m_steady_frame_rate_controller;
        } else {
            m_p_frame_rate_controller = &m_frame_rate_controller;
        }
        get_system_memory_status();
        if(!IWindow::create(m_window.put()))
            throw std::runtime_error("IWindow::create");
        auto const& gpu = core::ConfigurationLoader::getInstance().getGraphicsSystem().getPreferredDeviceName();
        if(!Graphics::Direct3D11::Device::create(gpu, m_device.put()))
            throw std::runtime_error("Graphics::Direct3D11::Device::create");
        if(!Graphics::SwapChain_D3D11::create(*m_window, *m_device, m_swapchain.put()))
            throw std::runtime_error("Graphics::SwapChain_D3D11::create");
        if(!Graphics::Renderer_D3D11::create(*m_device, m_renderer.put()))
            throw std::runtime_error("Graphics::Renderer_D3D11::create");
        m_frame_query_list.reserve(2);
        for(int i = 0; i < 2; i += 1) {
            m_frame_query_list.emplace_back(m_device.get());
        }
    }
    ApplicationModel_Win32::~ApplicationModel_Win32()
    {
        InputSystem::getInstance().shutdown();
        m_frame_query_list.clear();
        m_renderer.reset();
        m_swapchain.reset();
        m_device.reset();
        m_window.reset();
    }

    bool IApplicationModel::create(IApplicationEventListener* p_app, IApplicationModel** pp_model)
    {
        try {
            *pp_model = new ApplicationModel_Win32(p_app);
            return true;
        } catch(...) {
            *pp_model = nullptr;
            return false;
        }
    }
}
