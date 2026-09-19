#include "Mesh.hpp"
#include "ModelData.hpp"
#include "core/FileSystem.hpp"
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>
#include <cstring>

namespace
{
    using namespace core;
    using namespace core::Graphics;
    using namespace core::Graphics::SDLGPU;

    class GeometryTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            if(!spdlog::default_logger())
                spdlog::set_default_logger(spdlog::null_logger_mt("geometry-test"));
            FileSystemManager::addSearchPath(LUASTG_GEOMETRY_FIXTURES);
        }
        void TearDown() override { FileSystemManager::removeSearchPath(LUASTG_GEOMETRY_FIXTURES); }
    };

    TEST_F(GeometryTest, MeshLayoutsPreservePositionUvAndColor)
    {
        for(bool no_z : { false, true })
            for(bool compressed : { false, true }) {
                MeshOptions options{};
                options.vertex_count = 1;
                options.vertex_position_no_z = no_z;
                options.vertex_color_compression = compressed;
                MeshData data(options);
                data.position(0, { 3, 4, 5 });
                data.uv(0, { 0.25f, 0.75f });
                data.color(0, { 1, 0.5f, 0.25f, 1 });
                EXPECT_EQ(data.stride, (no_z ? 8u : 12u) + 8u + (compressed ? 4u : 16u));
                float x{}, v{};
                std::memcpy(&x, data.vertices.data(), sizeof(x));
                std::memcpy(&v, data.vertices.data() + data.uv_offset + 4, sizeof(v));
                EXPECT_FLOAT_EQ(x, 3);
                EXPECT_FLOAT_EQ(v, 0.75f);
                if(compressed) {
                    EXPECT_EQ(data.vertices[data.color_offset], 63u);
                    EXPECT_EQ(data.vertices[data.color_offset + 1], 127u);
                    EXPECT_EQ(data.vertices[data.color_offset + 2], 255u);
                } else {
                    float green{};
                    std::memcpy(&green, data.vertices.data() + data.color_offset + 4, sizeof(green));
                    EXPECT_FLOAT_EQ(green, 0.5f);
                }
            }
    }

    TEST_F(GeometryTest, ReadOnlyAndIndexLimitsLeaveDataUnchanged)
    {
        MeshOptions options{};
        options.vertex_count = 65538;
        options.index_count = 1;
        MeshData compressed(options);
        compressed.index(0, 65535);
        auto before = compressed.indices;
        compressed.index(0, 65536);
        EXPECT_EQ(compressed.indices, before);
        compressed.read_only = true;
        compressed.index(0, 0);
        EXPECT_EQ(compressed.indices, before);
        auto vertices = compressed.vertices;
        compressed.position(0, { 1, 2, 3 });
        EXPECT_EQ(compressed.vertices, vertices);
        options.vertex_index_compression = false;
        MeshData wide(options);
        wide.index(0, 65536);
        uint32_t value{};
        std::memcpy(&value, wide.indices.data(), sizeof(value));
        EXPECT_EQ(value, 65536u);
        EXPECT_NO_THROW(wide.validate());
    }

    TEST_F(GeometryTest, GltfAndGlbDecodeTheSameGeometryImagesAndNodeTransform)
    {
        const auto gltf = loadModelData("textured.gltf");
        const auto glb = loadModelData("textured.glb");
        ASSERT_EQ(gltf.primitives.size(), 1u);
        ASSERT_EQ(glb.primitives.size(), 1u);
        auto const& primitive = gltf.primitives[0];
        ASSERT_EQ(primitive.vertices.size(), 4u);
        EXPECT_EQ(primitive.indices, glb.primitives[0].indices);
        EXPECT_EQ(gltf.images[0].rgba, glb.images[0].rgba);
        EXPECT_FLOAT_EQ(primitive.vertices[0].position[0], -0.75f);
        EXPECT_FLOAT_EQ(primitive.vertices[0].uv[1], 1);
        EXPECT_NEAR(primitive.local._41, 0, 1e-6f);
        EXPECT_FLOAT_EQ(primitive.local._33, -1);
        EXPECT_EQ(gltf.samplers[0].filer, Filter::Anisotropic);
        EXPECT_EQ(gltf.samplers[0].address_u, TextureAddressMode::Wrap);
        EXPECT_FLOAT_EQ(gltf.samplers[0].max_lod, 0);
    }

    TEST_F(GeometryTest, ModelMaterialsIndicesAndUnindexedPrimitives)
    {
        EXPECT_EQ(loadModelData("mask.gltf").primitives[0].alpha_mode, 1u);
        EXPECT_EQ(loadModelData("blend.gltf").primitives[0].alpha_mode, 2u);
        EXPECT_FALSE(loadModelData("culled.gltf").primitives[0].double_sided);
        EXPECT_EQ(loadModelData("strip.gltf").primitives[0].topology, 5);
        EXPECT_TRUE(loadModelData("index32.gltf").primitives[0].index32);
        const auto bytes = loadModelData("index8.gltf");
        EXPECT_FALSE(bytes.primitives[0].index32);
        EXPECT_EQ(bytes.primitives[0].indices.size(), 12u);
        const auto unindexed = loadModelData("nonindexed.gltf");
        EXPECT_TRUE(unindexed.primitives[0].indices.empty());
        EXPECT_EQ(unindexed.primitives[0].vertices.size(), 3u);
        const auto colored = loadModelData("vertex.gltf");
        EXPECT_FLOAT_EQ(colored.primitives[0].vertices[0].color[1], 0.25f);
        EXPECT_FLOAT_EQ(colored.primitives[0].vertices[0].color[3], 1);
    }

    TEST_F(GeometryTest, UnsupportedAndMalformedModelsProduceDiagnostics)
    {
        for(auto const& [name, diagnostic] : { std::pair{ "invalid-loop.gltf", "Unsupported glTF primitive mode 2" }, std::pair{ "invalid-fan.gltf", "Unsupported glTF primitive mode 6" } }) {
            try {
                loadModelData(name);
                FAIL() << name << " must fail";
            } catch(std::exception const& error) {
                EXPECT_NE(std::string(error.what()).find(diagnostic), std::string::npos);
            }
        }
        for(auto name : { "invalid-range.gltf", "invalid-component.gltf", "invalid-sparse.gltf", "missing.gltf" }) {
            try {
                loadModelData(name);
                FAIL() << name << " must fail";
            } catch(std::exception const& error) {
                EXPECT_GT(std::strlen(error.what()), 0u);
            }
        }
    }

    TEST_F(GeometryTest, NormalizedStridedAttributesDecodeWithoutPadding)
    {
        const auto model = loadModelData("normalized.gltf");
        auto const& vertices = model.primitives.at(0).vertices;
        ASSERT_EQ(vertices.size(), 4u);
        EXPECT_FLOAT_EQ(vertices[0].color[1], 128.0f / 255);
        EXPECT_FLOAT_EQ(vertices[0].color[3], 1);
        EXPECT_FLOAT_EQ(vertices[0].uv[0], 0);
        EXPECT_FLOAT_EQ(vertices[0].uv[1], 1);
        EXPECT_FLOAT_EQ(vertices[2].uv[0], 1);
        EXPECT_FLOAT_EQ(vertices[2].uv[1], 0);
        EXPECT_EQ(model.samplers[0].address_u, TextureAddressMode::Mirror);
        EXPECT_EQ(model.samplers[0].address_v, TextureAddressMode::Clamp);
        EXPECT_EQ(loadModelData("line-strip.gltf").primitives[0].topology, 3);
    }

    TEST_F(GeometryTest, NestedScaleQuaternionAndTranslationUseLeftHandedCoordinates)
    {
        const auto model = loadModelData("transformed.gltf");
        auto const& matrix = model.primitives.at(0).local;
        EXPECT_NEAR(matrix._11, -2, 1e-6f);
        EXPECT_NEAR(matrix._22, -3, 1e-6f);
        EXPECT_NEAR(matrix._33, -4, 1e-6f);
        EXPECT_NEAR(matrix._41, 2, 1e-6f);
        EXPECT_NEAR(matrix._42, 3, 1e-6f);
        EXPECT_NEAR(matrix._43, -4, 1e-6f);
    }
}
