#pragma once
#include "Core/Graphics/Device.hpp"
#include "core/Vector4.hpp"
#include <DirectXMath.h>
#include <vector>
#include <string_view>

namespace core::Graphics::SDLGPU
{
    struct ModelVertex
    {
        float position[3]{};
        float normal[3]{};
        float color[4]{ 1, 1, 1, 1 };
        float uv[2]{};
    };
    struct ModelPrimitive
    {
        std::vector<ModelVertex> vertices;
        std::vector<uint8_t> indices;
        bool index32{};
        uint32_t index_count{};
        int topology{ 4 };
        DirectX::XMFLOAT4X4 local{};
        Vector4F base_color{ 1, 1, 1, 1 };
        int image{ -1 }, sampler{ -1 };
        uint32_t alpha_mode{};
        float alpha_cutoff{ 0.5f };
        bool double_sided{};
    };
    struct ModelImage
    {
        Vector2U size;
        std::vector<uint8_t> rgba;
    };
    struct ModelData
    {
        std::vector<ModelPrimitive> primitives;
        std::vector<ModelImage> images;
        std::vector<Graphics::SamplerState> samplers;
    };
    ModelData loadModelData(std::string_view path);
}
