#include "includes/color.hlsli"
Texture2D screen_texture : register(t4);
SamplerState screen_sampler : register(s4);
cbuffer engine_data : register(b1) { float4 screen_texture_size; float4 viewport; };
cbuffer user_data : register(b7) { float4 tint; };
float4 main(float2 uv : TEXCOORD0) : SV_Target {
    float2 pixel = uv * screen_texture_size.xy;
    if(any(pixel < viewport.xy) || any(pixel > viewport.zw)) discard;
    return fixture_color(screen_texture.Sample(screen_sampler, uv)) * tint;
}
