#include "ShaderCompiler.hpp"
#include "ShaderSource.hpp"
#include "PostEffectReflection.hpp"
#include "PostEffectSampling.hpp"
#include "MeshShaders.hpp"
#include "ModelShaders.hpp"
#include "SpriteShaders.hpp"
#include "core/FileSystem.hpp"
#include <SDL3_shadercross/SDL_shadercross.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace
{
    using namespace core::Graphics::SDLGPU;

    constexpr char sparse[] = R"hlsl(
Texture2D first : register(t4);
SamplerState first_sampler : register(s4);
Texture2D second : register(t9);
SamplerState second_sampler : register(s9);
cbuffer scalars : register(b0) { float gain; float3 tint; float2 shift; float4 color; };
cbuffer extra : register(b7) { float4 other; };
float4 main(float2 uv : TEXCOORD0, float4 vertex : COLOR0) : SV_Target {
    return first.Sample(first_sampler, uv + shift) * float4(tint * gain, 1) * color
        + second.SampleLevel(second_sampler, uv, 0) * other * vertex;
}
)hlsl";

    class ShaderTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            if(!spdlog::default_logger())
                spdlog::set_default_logger(spdlog::null_logger_mt("shader-test"));
            compiler = std::make_unique<ShaderCompiler>("direct3d12");
            core::FileSystemManager::addSearchPath(LUASTG_SHADER_FIXTURES);
        }
        void TearDown() override { core::FileSystemManager::removeSearchPath(LUASTG_SHADER_FIXTURES); }
        std::unique_ptr<ShaderCompiler> compiler;
    };

    TEST_F(ShaderTest, SparseRegistersKeepNamesOffsetsAndSamplingLayout)
    {
        auto source = expandShaderIncludes(sparse, "sparse.hlsl");
        auto binary = compiler->compileLegacy(source, "sparse.hlsl");
        auto layout = normalizePostEffect(binary);
        ASSERT_EQ(layout.textures.size(), 2u);
        EXPECT_EQ(layout.textures[0].name, "first");
        EXPECT_EQ(layout.textures[0].legacy_slot, 4u);
        EXPECT_EQ(layout.textures[0].slot, 0u);
        EXPECT_EQ(layout.textures[1].legacy_slot, 9u);
        EXPECT_EQ(layout.textures[1].slot, 1u);
        ASSERT_EQ(layout.buffers.size(), 2u);
        EXPECT_EQ(layout.buffers[1].legacy_slot, 7u);
        EXPECT_EQ(layout.buffers[1].slot, 1u);
        const std::vector<EffectVariable> expected{
            { "gain", 0, 4, 1 }, { "tint", 4, 12, 3 }, { "shift", 16, 8, 2 }, { "color", 32, 16, 4 }
        };
        EXPECT_EQ(layout.buffers[0].variables, expected);
        auto sampled = compiler->compileLegacy(addPostEffectSampling(source, layout), "sampled.hlsl");
        auto translated = normalizePostEffect(sampled);
        EXPECT_EQ(translated.textures, layout.textures);
        ASSERT_EQ(translated.buffers.size(), 3u);
        EXPECT_EQ(translated.buffers[0].variables, expected);
        EXPECT_EQ(translated.buffers[1].variables, layout.buffers[1].variables);
        EXPECT_EQ(translated.buffers[2].legacy_slot, effect_sampler_register);
        EXPECT_EQ(translated.buffers[2].bytes.size(), 2 * sizeof(EffectSamplingParameters));
        std::vector<EffectSamplingParameters> parameters(2);
        EXPECT_NO_THROW(setEffectSampling(translated.buffers, parameters));
        SDL_ShaderCross_SPIRV_Info info{};
        info.bytecode = reinterpret_cast<const Uint8*>(sampled.data());
        info.bytecode_size = sampled.size() * sizeof(uint32_t);
        info.entrypoint = "main";
        info.shader_stage = SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT;
        size_t dxil_size{};
        const std::unique_ptr<void, decltype(&SDL_free)> dxil(SDL_ShaderCross_CompileDXILFromSPIRV(&info, &dxil_size), SDL_free);
        ASSERT_NE(dxil.get(), nullptr) << SDL_GetError();
        EXPECT_GT(dxil_size, 0u);
    }

    TEST_F(ShaderTest, UnusedEngineBufferDoesNotLeaveAGapBeforeSamplerUniforms)
    {
        constexpr char source[] = R"hlsl(
Texture2D screen_texture : register(t4);
SamplerState screen_sampler : register(s4);
cbuffer user_data : register(b0) { float4 channel; };
cbuffer engine_data : register(b1) { float4 screen_texture_size; float4 viewport; };
float4 main(float2 uv : TEXCOORD0) : SV_Target {
    return screen_texture.Sample(screen_sampler, uv) * channel;
}
)hlsl";
        auto binary = compiler->compileLegacy(source, "unused-engine-buffer.hlsl");
        auto layout = normalizePostEffect(binary);
        binary = compiler->compileLegacy(addPostEffectSampling(source, layout), "unused-engine-buffer.hlsl");
        layout = normalizePostEffect(binary);
        ASSERT_EQ(layout.buffers.size(), 3u);
        EXPECT_EQ(layout.buffers[0].legacy_slot, 0u);
        EXPECT_EQ(layout.buffers[1].legacy_slot, effect_sampler_register);
        EXPECT_EQ(layout.buffers[2].legacy_slot, 1u);
        EXPECT_TRUE(layout.buffers[0].active);
        EXPECT_TRUE(layout.buffers[1].active);
        EXPECT_FALSE(layout.buffers[2].active);
        ASSERT_EQ(layout.buffers[2].variables.size(), 2u);
        EXPECT_EQ(layout.buffers[2].variables[0].name, "screen_texture_size");
        EXPECT_EQ(layout.buffers[2].variables[1].name, "viewport");
        const std::unique_ptr<SDL_ShaderCross_GraphicsShaderMetadata, decltype(&SDL_free)> metadata(
            SDL_ShaderCross_ReflectGraphicsSPIRV(reinterpret_cast<const Uint8*>(binary.data()), binary.size() * sizeof(uint32_t), 0), SDL_free);
        ASSERT_NE(metadata.get(), nullptr) << SDL_GetError();
        EXPECT_EQ(metadata->resource_info.num_uniform_buffers, 2u);
        for(auto const& buffer : layout.buffers) {
            if(buffer.active)
                EXPECT_LT(buffer.slot, metadata->resource_info.num_uniform_buffers);
        }
    }

    TEST_F(ShaderTest, PreprocessorUsesVfsRelativeIncludesMacrosAndGuards)
    {
        auto source = readShaderSource("include/root.hlsl");
        auto expanded = expandShaderIncludes(source, "include/root.hlsl");
        auto binary = compiler->compileLegacy(expanded, "include/root.hlsl");
        EXPECT_FALSE(binary.empty());
        EXPECT_NE(expanded.find("nested/color.hlsli"), std::string::npos);
        EXPECT_EQ(expanded.find("missing-inactive.hlsli"), std::string::npos);
    }

    TEST_F(ShaderTest, MissingIncludeAndUnguardedCycleHaveDiagnostics)
    {
        for(const auto* path : { "include/missing.hlsl", "include/cycle.hlsl" }) {
            auto const source = readShaderSource(path);
            try {
                expandShaderIncludes(source, path);
                FAIL() << path << " should fail preprocessing";
            } catch(std::exception const& error) {
                EXPECT_NE(std::string(error.what()).find(path), std::string::npos);
            }
        }
    }

    TEST_F(ShaderTest, RejectsUnpairedSamplersAndUnsupportedResources)
    {
        for(const auto* source : {
                "Texture2D t:register(t4); SamplerState s:register(s0); float4 main(float2 uv:TEXCOORD0):SV_Target{return t.Sample(s,uv);}",
                "TextureCube t:register(t0); SamplerState s:register(s0); float4 main(float2 uv:TEXCOORD0):SV_Target{return t.Sample(s,float3(uv,1));}" }) {
            auto binary = compiler->compileLegacy(source, "unsupported.hlsl");
            EXPECT_THROW(normalizePostEffect(binary), std::runtime_error);
        }
    }

    TEST_F(ShaderTest, PreservesSyntaxErrorSourceName)
    {
        try {
            compiler->compileLegacy(expandShaderIncludes("float4 main(:SV_Target {", "broken.hlsl"), "broken.hlsl");
            FAIL() << "Invalid HLSL should fail compilation";
        } catch(std::exception const& error) {
            EXPECT_NE(std::string(error.what()).find("broken.hlsl"), std::string::npos);
        }
    }

    TEST_F(ShaderTest, GeometryShadersCompileReflectAndTranslateToDxil)
    {
        auto verify = [](const char* source, SDL_ShaderCross_ShaderStage stage, uint32_t uniforms, uint32_t samplers) {
            SDL_ShaderCross_HLSL_Info hlsl{};
            hlsl.source = source;
            hlsl.entrypoint = "main";
            hlsl.shader_stage = stage;
            size_t size{};
            const std::unique_ptr<void, decltype(&SDL_free)> binary(SDL_ShaderCross_CompileSPIRVFromHLSL(&hlsl, &size), SDL_free);
            ASSERT_NE(binary.get(), nullptr) << SDL_GetError();
            const std::unique_ptr<SDL_ShaderCross_GraphicsShaderMetadata, decltype(&SDL_free)> metadata(
                SDL_ShaderCross_ReflectGraphicsSPIRV(static_cast<const Uint8*>(binary.get()), size, 0), SDL_free);
            ASSERT_NE(metadata.get(), nullptr) << SDL_GetError();
            EXPECT_EQ(metadata->resource_info.num_uniform_buffers, uniforms);
            EXPECT_EQ(metadata->resource_info.num_samplers, samplers);
            SDL_ShaderCross_SPIRV_Info info{};
            info.bytecode = static_cast<const Uint8*>(binary.get());
            info.bytecode_size = size;
            info.entrypoint = "main";
            info.shader_stage = stage;
            size_t dxil_size{};
            const std::unique_ptr<void, decltype(&SDL_free)> dxil(SDL_ShaderCross_CompileDXILFromSPIRV(&info, &dxil_size), SDL_free);
            ASSERT_NE(dxil.get(), nullptr) << SDL_GetError();
            EXPECT_GT(dxil_size, 0u);
        };
        for(bool no_z : { false, true })
            for(bool compressed : { false, true }) {
                const auto source = shaders::meshVertex(no_z, compressed);
                verify(source.c_str(), SDL_SHADERCROSS_SHADERSTAGE_VERTEX, 2, 0);
            }
        verify(shaders::sprite_fragment, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT, 1, 1);
        verify(shaders::model_vertex, SDL_SHADERCROSS_SHADERSTAGE_VERTEX, 2, 0);
        verify(shaders::model_fragment, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT, 1, 1);
    }
}
