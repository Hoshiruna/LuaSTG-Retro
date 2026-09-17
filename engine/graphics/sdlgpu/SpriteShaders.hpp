#pragma once

namespace core::Graphics::SDLGPU::shaders
{
    inline constexpr char sprite_vertex[] = R"hlsl(
cbuffer Camera : register(b0, space1) { float4x4 view_projection; };
struct Input { float3 position : TEXCOORD0; float4 color : TEXCOORD1; float2 uv : TEXCOORD2; };
struct Output { float4 position : SV_Position; float3 world : TEXCOORD0; float4 color : TEXCOORD1; float2 uv : TEXCOORD2; };
Output main(Input input) {
    Output output;
    output.position = mul(view_projection, float4(input.position, 1));
    output.world = input.position;
    output.color = input.color.bgra;
    output.uv = input.uv;
    return output;
}
)hlsl";

    inline constexpr char sprite_fragment[] = R"hlsl(
Texture2D<float4> image : register(t0, space2);
SamplerState image_sampler : register(s0, space2);
cbuffer Parameters : register(b0, space3) {
    float4 eye;
    float4 fog_color;
    float4 fog_range;
    uint4 modes; // vertex color, fog, premultiplied alpha, max anisotropy
    uint4 addressing; // u, v, min linear, mag linear
    float4 border;
    float4 lod; // bias, min, max, mip linear
};
struct Input { float4 position : SV_Position; float3 world : TEXCOORD0; float4 color : TEXCOORD1; float2 uv : TEXCOORD2; };

int address(int p, int size, uint mode) {
    if(mode == 0) return (p % size + size) % size;
    if(mode == 1) {
        p = (p % (2 * size) + 2 * size) % (2 * size);
        return p < size ? p : 2 * size - 1 - p;
    }
    if(mode == 4) p = p < 0 ? -p - 1 : p;
    return clamp(p, 0, size - 1);
}
float4 texel(int2 p, int2 size, uint level) {
    if((addressing.x == 3 && (p.x < 0 || p.x >= size.x)) ||
       (addressing.y == 3 && (p.y < 0 || p.y >= size.y))) return border;
    return image.Load(int3(address(p.x, size.x, addressing.x), address(p.y, size.y, addressing.y), level));
}
float4 sample_level(float2 uv, uint level, bool linear_filter) {
    uint width, height, levels;
    image.GetDimensions(level, width, height, levels);
    int2 size = int2(width, height);
    float2 p = uv * size;
    if(!linear_filter) return texel(int2(floor(p)), size, level);
    p -= 0.5;
    int2 base = int2(floor(p));
    float2 f = frac(p);
    return lerp(lerp(texel(base, size, level), texel(base + int2(1, 0), size, level), f.x),
                lerp(texel(base + int2(0, 1), size, level), texel(base + int2(1, 1), size, level), f.x), f.y);
}
float4 sample_mips(float2 uv, float raw, uint levels, bool linear_filter) {
    float level = clamp(clamp(raw, lod.y, lod.z), 0, float(levels - 1));
    if(lod.w == 0) return sample_level(uv, uint(floor(level + 0.5)), linear_filter);
    uint low = uint(floor(level));
    return lerp(sample_level(uv, low, linear_filter), sample_level(uv, min(low + 1, levels - 1), linear_filter), frac(level));
}
float4 sample_image(float2 uv) {
    if(addressing.x == 4) uv.x = abs(uv.x);
    if(addressing.y == 4) uv.y = abs(uv.y);
    if(addressing.x != 3 && addressing.y != 3) return image.Sample(image_sampler, uv);
    uint width, height, levels;
    image.GetDimensions(0, width, height, levels);
    float2 dx = ddx(uv) * float2(width, height);
    float2 dy = ddy(uv) * float2(width, height);
    float raw = 0.5 * log2(max(max(dot(dx, dx), dot(dy, dy)), 1e-12)) + lod.x;
    bool linear_filter = (raw > 0 ? addressing.z : addressing.w) != 0;
    if(modes.w > 1 && raw > 0) {
        // Border colors require explicit taps; sample along the longer footprint.
        float major = max(length(dx), length(dy));
        float minor = max(min(length(dx), length(dy)), major / modes.w);
        uint taps = uint(clamp(ceil(major / max(minor, 1e-6)), 1, float(modes.w)));
        float2 direction = (dot(dx, dx) > dot(dy, dy) ? ddx(uv) : ddy(uv));
        float4 sum = 0;
        for(uint i = 0; i < taps; ++i) {
            sum += sample_mips(uv + direction * ((i + 0.5) / taps - 0.5), log2(max(minor, 1e-6)) + lod.x, levels, true);
        }
        return sum / taps;
    }
    return sample_mips(uv, raw, levels, linear_filter);
}
float4 main(Input input) : SV_Target0 {
    float4 color = modes.x == 1 ? float4(1, 1, 1, 1) : sample_image(input.uv);
    if(modes.x == 3) {
        color *= input.color;
        color.rgb *= modes.z == 0 ? color.a : input.color.a;
    } else if(modes.x == 2) {
        if(modes.z != 0) {
            if(color.a < 1.0 / 255.0) discard;
            color.rgb /= color.a;
        }
        color.rgb = min(color.rgb + input.color.rgb, 1);
        color.a *= input.color.a;
        color.rgb *= color.a;
    } else {
        if(modes.x == 1) color = input.color;
        if(modes.z == 0) color.rgb *= color.a;
    }
    if(modes.y != 0) {
        float distance_to_eye = distance(eye.xyz, input.world);
        float k;
        if(modes.y == 2) k = saturate(1 - exp(-distance_to_eye * fog_range.x));
        else if(modes.y == 3) k = saturate(1 - exp(-pow(distance_to_eye * fog_range.x, 2)));
        else k = saturate((distance_to_eye - fog_range.x) / (fog_range.y - fog_range.x));
        float alpha = color.a * (1 - k + k * fog_color.a);
        color = float4((1 - k) * (1 - k + k * fog_color.a) * color.rgb + k * alpha * fog_color.rgb, alpha);
    }
    return color;
}
)hlsl";
}
