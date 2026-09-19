#ifndef FIXTURE_COLOR_INCLUDED
#define FIXTURE_COLOR_INCLUDED
#include "../shared.hlsli"
float4 fixture_color(float2 uv) { return float4(uv, fixture_blue, 1); }
#endif
