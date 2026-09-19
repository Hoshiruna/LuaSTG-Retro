#ifndef POSTEFFECT_FIXTURE_COMMON
#define POSTEFFECT_FIXTURE_COMMON
#include "res/posteffect/weights.hlsli"
struct PS_Input { float4 position : SV_Position; float2 uv : TEXCOORD0; float4 color : COLOR0; };
float4 grayscale(float4 color, float3 weights) { return float4(dot(color.rgb, weights).xxx, color.a); }
#endif
