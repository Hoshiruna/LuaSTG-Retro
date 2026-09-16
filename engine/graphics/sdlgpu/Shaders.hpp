#pragma once

namespace core::Graphics::SDLGPU::shaders
{
    inline constexpr char quad[] = R"hlsl(
cbuffer QuadData : register(b0, space1)
{
    float4 rectangle;
    float4 tint;
};

struct Output
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

Output main(uint vertex_id : SV_VertexID)
{
    const float2 corners[6] = {
        float2(0, 0), float2(1, 0), float2(0, 1),
        float2(0, 1), float2(1, 0), float2(1, 1)
    };
    float2 uv = corners[vertex_id];
    float2 pixel = rectangle.xy + uv * rectangle.zw;
    Output result;
    result.position = float4(pixel.x / 160.0 - 1.0, 1.0 - pixel.y / 120.0, 0, 1);
    result.uv = uv;
    result.color = tint;
    return result;
}
)hlsl";

    inline constexpr char textured[] = R"hlsl(
Texture2D image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0, float4 color : COLOR0) : SV_Target0
{
    return image.Sample(image_sampler, uv) * color;
}
)hlsl";

    inline constexpr char grayscale[] = R"hlsl(
Texture2D image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);

float4 main(float4 position : SV_Position, float2 uv : TEXCOORD0, float4 color : COLOR0) : SV_Target0
{
    float4 sampled = image.Sample(image_sampler, uv);
    float gray = dot(sampled.rgb, float3(0.2126, 0.7152, 0.0722));
    return float4(gray, gray, gray, sampled.a);
}
)hlsl";
}
