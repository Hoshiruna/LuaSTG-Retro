#include "AppFrame.h"
#include "core/InputSystem.hpp"

namespace luastg
{
    core::Vector2F AppFrame::GetMousePosition(const bool no_flip, bool* const inside, const bool raw) noexcept
    {
        const auto& input = core::InputSystem::getInstance();
        const auto mouse = raw ? input.getRawMouseState() : input.getMouseState();
        const auto window_size = GetAppModel()->getWindow()->getSize();
        const auto canvas_size = GetAppModel()->getSwapChain()->getCanvasSize();
        core::CanvasScalingMode scaling_mode{};
        switch(GetAppModel()->getSwapChain()->getScalingMode()) {
            case core::Graphics::SwapChainScalingMode::AspectRatio:
                scaling_mode = core::CanvasScalingMode::aspect_ratio;
                break;
            case core::Graphics::SwapChainScalingMode::IntegerAspectRatio:
                scaling_mode = core::CanvasScalingMode::integer_aspect_ratio;
                break;
            default:
                scaling_mode = core::CanvasScalingMode::stretch;
                break;
        }
        const auto mapped = core::mapMouseToCanvas(
            { mouse.x, mouse.y }, window_size, canvas_size, scaling_mode, !no_flip);
        if(inside != nullptr) {
            *inside = mapped.inside;
        }
        return mapped.position;
    }

}
