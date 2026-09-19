#include "PostEffectSampling.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

namespace core::Graphics::SDLGPU
{
    void setEffectSampling(std::span<EffectBuffer> buffers, std::span<const EffectSamplingParameters> parameters)
    {
        if(parameters.empty())
            return;
        for(auto& buffer : buffers) {
            if(buffer.legacy_slot != effect_sampler_register)
                continue;
            if(buffer.bytes.size() != parameters.size_bytes())
                throw std::runtime_error("Unexpected post-effect sampler buffer layout");
            std::memcpy(buffer.bytes.data(), parameters.data(), parameters.size_bytes());
            return;
        }
        throw std::runtime_error("Missing post-effect sampler buffer");
    }

    EffectSamplingParameters effectSamplingParameters(Graphics::SamplerState const& d)
    {
        const auto f = d.filer;
        const bool min_linear = f != Filter::Point && f != Filter::PointMagLinear && f != Filter::PointMipLinear && f != Filter::LinearMinPoint;
        const bool mag_linear = f != Filter::Point && f != Filter::PointMinLinear && f != Filter::PointMipLinear && f != Filter::LinearMagPoint;
        const bool mip_linear = f == Filter::Linear || f == Filter::Anisotropic || f == Filter::LinearMinPoint || f == Filter::LinearMagPoint || f == Filter::PointMipLinear;
        const bool white = d.border_color == BorderColor::White || d.border_color == BorderColor::TransparentWhite;
        const bool opaque = d.border_color == BorderColor::White || d.border_color == BorderColor::OpaqueBlack;
        return { { uint32_t(d.address_u), uint32_t(d.address_v), 0, 0 },
            { uint32_t(min_linear), uint32_t(mag_linear), uint32_t(mip_linear), f == Filter::Anisotropic ? std::clamp(d.max_anisotropy, 1u, 16u) : 1u },
            { float(white), float(white), float(white), float(opaque) },
            { d.mip_lod_bias, d.min_lod, d.max_lod, 0 } };
    }

    namespace
    {
        struct Token
        {
            size_t begin, end;
            std::string_view text;
        };
        std::vector<Token> tokens(std::string_view source)
        {
            std::vector<Token> result;
            for(size_t i = 0; i < source.size();) {
                if(std::isspace(static_cast<unsigned char>(source[i]))) {
                    ++i;
                    continue;
                }
                if(source.substr(i, 2) == "//") {
                    i = source.find('\n', i);
                    if(i == std::string_view::npos)
                        break;
                    continue;
                }
                if(source.substr(i, 2) == "/*") {
                    i = source.find("*/", i + 2);
                    if(i == std::string_view::npos)
                        break;
                    i += 2;
                    continue;
                }
                const auto begin = i++;
                if(source[begin] == '"') {
                    while(i < source.size()) {
                        if(source[i++] == '"')
                            break;
                        if(source[i - 1] == '\\' && i < source.size())
                            ++i;
                    }
                } else if(std::isalpha(static_cast<unsigned char>(source[begin])) || source[begin] == '_') {
                    while(i < source.size() && (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_')) ++i;
                }
                result.push_back({ begin, i, source.substr(begin, i - begin) });
            }
            return result;
        }
        void replace(std::string& text, std::string_view from, std::string const& to)
        {
            size_t offset{};
            while((offset = text.find(from, offset)) != std::string::npos) {
                text.replace(offset, from.size(), to);
                offset += to.size();
            }
        }
        constexpr char sampling[] = R"hlsl(
int @P@address(int p, int size, uint mode) {
    if(mode == 0) return (p % size + size) % size;
    if(mode == 1) { p = (p % (2 * size) + 2 * size) % (2 * size); return p < size ? p : 2 * size - 1 - p; }
    if(mode == 4) p = p < 0 ? -p - 1 : p;
    return clamp(p, 0, size - 1);
}
float4 @P@texel(int2 p, int2 size, uint level) {
    uint2 mode = _luastg_sampling[@I@].addressing.xy;
    if((mode.x == 3 && (p.x < 0 || p.x >= size.x)) || (mode.y == 3 && (p.y < 0 || p.y >= size.y))) return _luastg_sampling[@I@].border;
    return @T@.Load(int3(@P@address(p.x, size.x, mode.x), @P@address(p.y, size.y, mode.y), level));
}
float4 @P@level(float2 uv, uint level, bool linear_filter) {
    uint w, h, levels; @T@.GetDimensions(level, w, h, levels);
    int2 size = int2(w, h); float2 p = uv * size;
    if(!linear_filter) return @P@texel(int2(floor(p)), size, level);
    p -= 0.5; int2 base = int2(floor(p)); float2 f = frac(p);
    return lerp(lerp(@P@texel(base, size, level), @P@texel(base + int2(1, 0), size, level), f.x),
                lerp(@P@texel(base + int2(0, 1), size, level), @P@texel(base + int2(1, 1), size, level), f.x), f.y);
}
float4 @P@mips(float2 uv, float raw, bool linear_filter) {
    uint w, h, levels; @T@.GetDimensions(0, w, h, levels);
    float level = clamp(clamp(raw, _luastg_sampling[@I@].lod.y, _luastg_sampling[@I@].lod.z), 0, float(levels - 1));
    if(_luastg_sampling[@I@].filtering.z == 0) return @P@level(uv, uint(floor(level + 0.5)), linear_filter);
    uint low = uint(floor(level));
    return lerp(@P@level(uv, low, linear_filter), @P@level(uv, min(low + 1, levels - 1), linear_filter), frac(level));
}
float2 @P@uv(float2 uv) {
    if(_luastg_sampling[@I@].addressing.x == 4) uv.x = abs(uv.x);
    if(_luastg_sampling[@I@].addressing.y == 4) uv.y = abs(uv.y);
    return uv;
}
bool @P@border() { return any(_luastg_sampling[@I@].addressing.xy == 3); }
float4 @P@footprint(float2 uv, float2 dx, float2 dy, float bias) {
    uint w, h, levels; @T@.GetDimensions(0, w, h, levels);
    float2 px = dx * float2(w, h), py = dy * float2(w, h);
    float raw = 0.5 * log2(max(max(dot(px, px), dot(py, py)), 1e-12)) + _luastg_sampling[@I@].lod.x + bias;
    bool linear_filter = (raw > 0 ? _luastg_sampling[@I@].filtering.x : _luastg_sampling[@I@].filtering.y) != 0;
    uint anisotropy = _luastg_sampling[@I@].filtering.w;
    if(anisotropy > 1 && raw > 0) {
        float major = max(length(px), length(py));
        float minor = max(min(length(px), length(py)), major / anisotropy);
        uint taps = uint(clamp(ceil(major / max(minor, 1e-6)), 1, float(anisotropy)));
        float2 direction = dot(px, px) > dot(py, py) ? dx : dy;
        float4 sum = 0;
        for(uint i = 0; i < taps; ++i) sum += @P@mips(uv + direction * ((i + 0.5) / taps - 0.5), log2(max(minor, 1e-6)) + _luastg_sampling[@I@].lod.x + bias, true);
        return sum / taps;
    }
    return @P@mips(uv, raw, linear_filter);
}
float4 @P@Sample(SamplerState s, float2 uv) {
    uv = @P@uv(uv);
    if(!@P@border()) return @T@.Sample(s, uv);
    return @P@footprint(uv, ddx(uv), ddy(uv), 0);
}
float4 @P@SampleLevel(SamplerState s, float2 uv, float level) {
    uv = @P@uv(uv);
    if(!@P@border()) return @T@.SampleLevel(s, uv, level);
    return @P@mips(uv, level, (level > 0 ? _luastg_sampling[@I@].filtering.x : _luastg_sampling[@I@].filtering.y) != 0);
}
float4 @P@SampleBias(SamplerState s, float2 uv, float bias) {
    uv = @P@uv(uv);
    if(!@P@border()) return @T@.SampleBias(s, uv, bias);
    return @P@footprint(uv, ddx(uv), ddy(uv), bias);
}
float4 @P@SampleGrad(SamplerState s, float2 uv, float2 dx, float2 dy) {
    if(_luastg_sampling[@I@].addressing.x == 4 && uv.x < 0) { dx.x = -dx.x; dy.x = -dy.x; }
    if(_luastg_sampling[@I@].addressing.y == 4 && uv.y < 0) { dx.y = -dx.y; dy.y = -dy.y; }
    uv = @P@uv(uv);
    if(!@P@border()) return @T@.SampleGrad(s, uv, dx, dy);
    return @P@footprint(uv, dx, dy, 0);
}
)hlsl";
    }

    std::string addPostEffectSampling(std::string_view source, EffectLayout const& layout)
    {
        if(layout.textures.empty())
            return std::string(source);
        if(layout.buffers.size() >= 4)
            throw std::runtime_error("Post-effects with textures support three user cbuffers; SDL's fourth slot holds sampler parameters");
        if(source.find("_luastg_") != std::string_view::npos)
            throw std::runtime_error("The _luastg_ shader prefix is reserved for engine sampling helpers");
        std::string prefix = "struct _luastg_Sampling { uint4 addressing; uint4 filtering; float4 border; float4 lod; };\n"
                             "cbuffer _luastg_effect_samplers : register(b65535) { _luastg_Sampling _luastg_sampling[" +
            std::to_string(layout.textures.size()) + "]; };\n";
        std::string suffix;
        for(auto const& texture : layout.textures) {
            const auto p = "_luastg_t" + std::to_string(texture.slot) + "_";
            prefix += "float4 " + p + "Sample(SamplerState s, float2 uv);\n";
            prefix += "float4 " + p + "SampleLevel(SamplerState s, float2 uv, float level);\n";
            prefix += "float4 " + p + "SampleBias(SamplerState s, float2 uv, float bias);\n";
            prefix += "float4 " + p + "SampleGrad(SamplerState s, float2 uv, float2 dx, float2 dy);\n";
            std::string helper(sampling);
            replace(helper, "@P@", p);
            replace(helper, "@I@", std::to_string(texture.slot));
            replace(helper, "@T@", texture.name);
            suffix += helper;
        }
        auto list = tokens(source);
        std::string body;
        size_t copied{};
        for(size_t i = 0; i + 3 < list.size(); ++i) {
            if(list[i + 1].text != "." || list[i + 3].text != "(")
                continue;
            bool matched{};
            for(auto const& texture : layout.textures) {
                if(list[i].text != texture.name)
                    continue;
                matched = true;
                auto method = list[i + 2].text;
                if(method == "Load" || method == "GetDimensions")
                    continue;
                if(method != "Sample" && method != "SampleLevel" && method != "SampleGrad" && method != "SampleBias")
                    throw std::runtime_error(texture.name + ": unsupported sampling method " + std::string(method));
                if(i + 5 >= list.size() || list[i + 4].text != texture.sampler_name || list[i + 5].text != ",")
                    throw std::runtime_error(texture.name + ": use its matching sampler " + texture.sampler_name);
                body.append(source.substr(copied, list[i].begin - copied));
                body += "_luastg_t" + std::to_string(texture.slot) + "_" + std::string(method);
                copied = list[i + 2].end;
            }
            if(!matched && list[i + 2].text.starts_with("Sample"))
                throw std::runtime_error("Post-effect sampling must name a global texture directly: " + std::string(list[i].text));
        }
        body.append(source.substr(copied));
        return prefix + body + "\n#line 1 \"<engine-sampling>\"\n" + suffix;
    }
}
