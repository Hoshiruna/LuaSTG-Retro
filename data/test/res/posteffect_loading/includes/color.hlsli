#ifndef LOADING_FIXTURE_COLOR
#define LOADING_FIXTURE_COLOR
#include "nested/channels.hlsli"
float4 fixture_color(float4 color) { return float4(fixture_channels(color.rgb), color.a); }
#endif
