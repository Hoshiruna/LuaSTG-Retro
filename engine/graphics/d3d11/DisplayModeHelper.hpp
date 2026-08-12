#pragma once
#include "d3d11/pch.h"

namespace d3d11
{
    bool getSwapChainNearestOutputFromDisplay(IDXGISwapChain* swap_chain, HMONITOR display, IDXGIOutput** out_output);

    bool findBestDisplayMode(IDXGISwapChain1* swap_chain, HMONITOR display, UINT target_width, UINT target_height, DXGI_MODE_DESC1& mode);
}
