#pragma once
#include <string>

namespace core::Graphics::SDLGPU::shaders
{
    inline std::string meshVertex(bool no_z, bool compressed)
    {
        return std::string(R"hlsl(
cbuffer Camera : register(b0, space1) { float4x4 view_projection; };
cbuffer Transform : register(b1, space1) { float4x4 world; };
struct Input { )hlsl") +
            (no_z ? "float2" : "float3") + R"hlsl( position : TEXCOORD0; float4 color : TEXCOORD1; float2 uv : TEXCOORD2; };
struct Output { float4 position : SV_Position; float3 world : TEXCOORD0; float4 color : TEXCOORD1; float2 uv : TEXCOORD2; };
Output main(Input input) {
    Output output;
    float4 position = mul(world, )hlsl" +
            (no_z ? "float4(input.position, 0, 1)" : "float4(input.position, 1)") + R"hlsl();
    output.position = mul(view_projection, position);
    output.world = position.xyz;
    output.color = )hlsl" +
            (compressed ? "input.color.bgra" : "input.color") + R"hlsl(;
    output.uv = input.uv;
    return output;
}
)hlsl";
    }
}
