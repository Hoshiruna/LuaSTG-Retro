#include "res/posteffect/common.hlsli"
Texture2D source_texture : register(t4);
SamplerState source_sampler : register(s4);
Texture2D mask_texture : register(t9);
SamplerState mask_sampler : register(s9);
cbuffer parameters : register(b7) { float gain; float3 tint; float2 shift; float4 color; };
float4 main(PS_Input input) : SV_Target {
    return grayscale(source_texture.Sample(source_sampler, input.uv + shift), tint) * gain
        * mask_texture.Sample(mask_sampler, input.uv) * color * input.color;
}
