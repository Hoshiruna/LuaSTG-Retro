#pragma once

namespace core::Graphics::SDLGPU::shaders
{
    inline constexpr char model_vertex[] = R"hlsl(
cbuffer Camera : register(b0, space1) { float4x4 view_projection; };
cbuffer Transform : register(b1, space1) { float4x4 world; float4x4 normal_world; };
struct Input { float3 position : TEXCOORD0; float3 normal : TEXCOORD1; float4 color : TEXCOORD2; float2 uv : TEXCOORD3; };
struct Output { float4 position : SV_Position; float3 world : TEXCOORD0; float3 normal : TEXCOORD1; float4 color : TEXCOORD2; float2 uv : TEXCOORD3; };
Output main(Input input) {
    Output output;
    float4 position = mul(world, float4(input.position, 1));
    output.position = mul(view_projection, position);
    output.world = position.xyz;
    output.normal = mul(normal_world, float4(input.normal, 0)).xyz;
    output.color = input.color;
    output.uv = input.uv;
    return output;
}
)hlsl";
    inline constexpr char model_fragment[] = R"hlsl(
Texture2D<float4> image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);
cbuffer Parameters : register(b0, space3) {
    float4 eye; float4 fog_color; float4 fog_range;
    float4 base_color; float4 ambient; float4 light_direction; float4 light_color;
    uint4 modes; // fog, alpha mode, reserved, reserved
    float4 alpha;
};
struct Input { float4 position : SV_Position; float3 world : TEXCOORD0; float3 normal : TEXCOORD1; float4 color : TEXCOORD2; float2 uv : TEXCOORD3; };
static const uint coverage[17] = { 0, 1, 1025, 1281, 1285, 1317, 34085, 42277, 42405, 42469, 46565, 62949, 62965, 62967, 65015, 65527, 65535 };
float4 main(Input input) : SV_Target {
    float4 sampled = image.Sample(image_sampler, input.uv);
    float4 color = base_color * input.color * float4(pow(sampled.rgb, 2.2), sampled.a);
    if(modes.y == 1 && color.a < alpha.x) discard;
    if(modes.y == 2) {
        uint level = uint(floor(saturate(color.a) * 16 + 0.5));
        uint2 pixel = uint2(floor(input.position.xy)) % 4;
        if((coverage[level] & (1u << (pixel.y * 4 + pixel.x))) == 0) discard;
    }
    float length_squared = dot(input.normal, input.normal);
    float3 normal = length_squared > 0 ? input.normal * rsqrt(length_squared) : float3(0, 0, 0);
    float lighting = max(0, dot(normal, -light_direction.xyz));
    color.rgb *= ambient.rgb * ambient.a + light_color.rgb * light_color.a * lighting;
    if(modes.x != 0) {
        float distance_to_eye = distance(eye.xyz, input.world);
        float k;
        if(modes.x == 2) k = saturate(1 - exp(-distance_to_eye * fog_range.x));
        else if(modes.x == 3) k = saturate(1 - exp(-pow(distance_to_eye * fog_range.x, 2)));
        else k = saturate((distance_to_eye - fog_range.x) / (fog_range.y - fog_range.x));
        color.rgb = lerp(color.rgb, fog_color.rgb, k);
    }
    return pow(color, 1.0 / 2.2);
}
)hlsl";
}
