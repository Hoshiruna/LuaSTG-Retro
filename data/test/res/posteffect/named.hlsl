#include "res/posteffect/common.hlsli"
Texture2D extra_texture : register(t0);
SamplerState extra_sampler : register(s0);
Texture2D screen_texture : register(t4);
SamplerState screen_sampler : register(s4);
cbuffer user_data : register(b0) { float4 channel_factor; float amount; };
cbuffer engine_data : register(b1) { float4 screen_texture_size; float4 viewport; };
float4 main(PS_Input input) : SV_Target {
    float2 xy = input.uv * screen_texture_size.xy;
    if(any(xy < viewport.xy) || any(xy > viewport.zw)) discard;
    return grayscale(screen_texture.Sample(screen_sampler, input.uv), channel_factor.rgb)
        + extra_texture.Sample(extra_sampler, input.uv) * amount;
}
