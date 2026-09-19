#include "res/posteffect/common.hlsli"
Texture2D extra_texture : register(t0);
SamplerState extra_sampler : register(s0);
Texture2D screen_texture : register(t4);
SamplerState screen_sampler : register(s4);
cbuffer user_data : register(b0) { float4 channel; float4 uv_transform; float4 extra_mix; };
cbuffer engine_data : register(b1) { float4 screen_texture_size; float4 viewport; };
float4 main(PS_Input input) : SV_Target {
    if(any(screen_texture_size.xy != float2(64, 48))) return float4(1, 0, 1, 1);
    float2 xy = input.uv * screen_texture_size.xy;
    if(any(xy < viewport.xy) || any(xy > viewport.zw)) discard;
    return screen_texture.Sample(screen_sampler, input.uv * uv_transform.xy + uv_transform.zw) * channel
        + extra_texture.Sample(extra_sampler, input.uv) * extra_mix;
}
