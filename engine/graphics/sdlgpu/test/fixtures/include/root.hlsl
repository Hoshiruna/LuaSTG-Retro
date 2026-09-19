#define COLOR_INCLUDE "nested/color.hlsli"
#include COLOR_INCLUDE
#include COLOR_INCLUDE
#if 0
#include "missing-inactive.hlsli"
#endif
float4 main(float2 uv : TEXCOORD0) : SV_Target { return fixture_color(uv); }
