#include "ModelData.hpp"
#include "core/FileSystem.hpp"
#include "core/SmartReference.hpp"
#include "core/Logger.hpp"
#include <tiny_gltf.h>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        template<typename T>
        T const& at(std::vector<T> const& items, int index, const char* kind)
        {
            if(index < 0 || size_t(index) >= items.size())
                throw std::runtime_error(std::string("Invalid glTF ") + kind + " index");
            return items[size_t(index)];
        }
        struct Accessor
        {
            tinygltf::Accessor const& info;
            const uint8_t* bytes{};
            size_t stride{}, components{}, component_size{};
            Accessor(tinygltf::Model const& model, int index) : info(at(model.accessors, index, "accessor"))
            {
                if(info.sparse.isSparse)
                    throw std::runtime_error("Sparse glTF accessors are unsupported");
                auto const& view = at(model.bufferViews, info.bufferView, "buffer view");
                auto const& buffer = at(model.buffers, view.buffer, "buffer");
                const int size = tinygltf::GetComponentSizeInBytes(info.componentType);
                const int count = tinygltf::GetNumComponentsInType(info.type);
                if(size <= 0 || count <= 0 || !info.count)
                    throw std::runtime_error("Invalid glTF accessor format or count");
                component_size = size_t(size);
                components = size_t(count);
                const size_t element = components * component_size;
                stride = view.byteStride ? view.byteStride : element;
                if(stride < element || stride % component_size || view.byteOffset % component_size || info.byteOffset % component_size || view.byteOffset > buffer.data.size() || view.byteLength > buffer.data.size() - view.byteOffset || info.byteOffset > view.byteLength)
                    throw std::runtime_error("Invalid glTF accessor range or stride");
                const size_t available = view.byteLength - info.byteOffset;
                if(info.count && (available < element || info.count - 1 > (available - element) / stride))
                    throw std::runtime_error("glTF accessor extends beyond its buffer view");
                bytes = buffer.data.data() + view.byteOffset + info.byteOffset;
            }
            template<typename T>
            T read(size_t i, size_t c) const
            {
                T value{};
                std::memcpy(&value, bytes + i * stride + c * component_size, sizeof(T));
                return value;
            }
            float value(size_t i, size_t c) const
            {
                switch(info.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_FLOAT: return read<float>(i, c);
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return float(read<uint8_t>(i, c)) / (info.normalized ? 255.0f : 1.0f);
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return float(read<uint16_t>(i, c)) / (info.normalized ? 65535.0f : 1.0f);
                    default: throw std::runtime_error("Unsupported glTF vertex component type");
                }
            }
            uint32_t index(size_t i) const
            {
                if(components != 1 || info.normalized)
                    throw std::runtime_error("Invalid glTF index accessor");
                switch(info.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return read<uint8_t>(i, 0);
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return read<uint16_t>(i, 0);
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: return read<uint32_t>(i, 0);
                    default: throw std::runtime_error("Unsupported glTF index component type");
                }
            }
        };
        DirectX::XMMATRIX nodeMatrix(tinygltf::Node const& node)
        {
            using namespace DirectX;
            if(!node.matrix.empty()) {
                if(node.matrix.size() != 16)
                    throw std::runtime_error("Invalid glTF node matrix");
                XMFLOAT4X4 matrix{};
                for(size_t i = 0; i < 16; ++i) matrix.m[i / 4][i % 4] = float(node.matrix[i]);
                return XMLoadFloat4x4(&matrix);
            }
            auto scale = XMMatrixIdentity(), rotation = scale, translation = scale;
            if(!node.scale.empty()) {
                if(node.scale.size() != 3)
                    throw std::runtime_error("Invalid glTF node scale");
                scale = XMMatrixScaling(float(node.scale[0]), float(node.scale[1]), float(node.scale[2]));
            }
            if(!node.rotation.empty()) {
                if(node.rotation.size() != 4)
                    throw std::runtime_error("Invalid glTF node quaternion");
                rotation = XMMatrixRotationQuaternion(XMVectorSet(float(node.rotation[0]), float(node.rotation[1]), float(node.rotation[2]), float(node.rotation[3])));
            }
            if(!node.translation.empty()) {
                if(node.translation.size() != 3)
                    throw std::runtime_error("Invalid glTF node translation");
                translation = XMMatrixTranslation(float(node.translation[0]), float(node.translation[1]), float(node.translation[2]));
            }
            return scale * rotation * translation;
        }
    }
    ModelData loadModelData(std::string_view path)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::FsCallbacks fs{};
        fs.FileExists = [](const std::string& name, void*) { return FileSystemManager::hasFile(name); };
        fs.ExpandFilePath = [](const std::string& name, void*) { return name; };
        fs.ReadWholeFile = [](std::vector<unsigned char>* out, std::string* error, const std::string& name, void*) {
            SmartReference<IData> bytes;
            if(!FileSystemManager::readFile(name, bytes.put())) {
                if(error)
                    *error += "Cannot read model resource: " + name;
                return false;
            }
            out->resize(bytes->size());
            std::memcpy(out->data(), bytes->data(), bytes->size());
            return true;
        };
        fs.WriteWholeFile = [](std::string*, const std::string&, const std::vector<unsigned char>&, void*) { return false; };
        fs.GetFileSizeInBytes = [](size_t* size, std::string*, const std::string& name, void*) {
            *size = FileSystemManager::getFileSize(name);
            return FileSystemManager::hasFile(name);
        };
        loader.SetFsCallbacks(fs);
        tinygltf::Model model;
        std::string error, warning, filename(path);
        const bool loaded = filename.ends_with(".gltf") ? loader.LoadASCIIFromFile(&model, &error, &warning, filename) : loader.LoadBinaryFromFile(&model, &error, &warning, filename);
        if(!warning.empty())
            Logger::warn("[sdlgpu] Model {}: {}", filename, warning);
        if(!loaded)
            throw std::runtime_error(filename + ": " + error);
        ModelData result;
        for(auto const& image : model.images) {
            if(image.width <= 0 || image.height <= 0 || image.component < 1 || image.component > 4 || image.bits != 8 ||
                uint64_t(image.width) * image.height * image.component != image.image.size())
                throw std::runtime_error("Invalid glTF image: " + image.name);
            ModelImage decoded{ { uint32_t(image.width), uint32_t(image.height) }, {} };
            decoded.rgba.resize(size_t(image.width) * image.height * 4);
            for(size_t i = 0; i < decoded.rgba.size() / 4; ++i) {
                auto const* source = image.image.data() + i * image.component;
                auto* target = decoded.rgba.data() + i * 4;
                target[0] = source[0];
                target[1] = image.component < 3 ? source[0] : source[1];
                target[2] = image.component < 3 ? source[0] : source[2];
                target[3] = image.component == 2 ? source[1] : (image.component == 4 ? source[3] : 255);
            }
            result.images.push_back(std::move(decoded));
        }
        for(auto const& source : model.samplers) {
            Graphics::SamplerState sampler;
            // The retained renderer forces anisotropic filtering for explicit glTF samplers.
            sampler.filer = Filter::Anisotropic;
            sampler.max_anisotropy = 1;
            auto address = [](int value) { return value == 33071 ? TextureAddressMode::Clamp : (value == 33648 ? TextureAddressMode::Mirror : TextureAddressMode::Wrap); };
            sampler.address_u = address(source.wrapS);
            sampler.address_v = address(source.wrapT);
            sampler.address_w = TextureAddressMode::Wrap;
            if((source.minFilter == 9728 || source.minFilter == 9729) && (source.magFilter == 9728 || source.magFilter == 9729))
                sampler.max_lod = 0;
            result.samplers.push_back(sampler);
        }
        std::vector<bool> visiting(model.nodes.size());
        std::function<void(int, DirectX::XMMATRIX const&)> visit;
        visit = [&](int index, DirectX::XMMATRIX const& parent) {
            auto const& node = at(model.nodes, index, "node");
            if(visiting[size_t(index)])
                throw std::runtime_error("Cyclic glTF node hierarchy");
            visiting[size_t(index)] = true;
            const auto transform = nodeMatrix(node) * parent;
            if(node.mesh >= 0)
                for(auto const& source : at(model.meshes, node.mesh, "mesh").primitives) {
                    ModelPrimitive primitive;
                    primitive.topology = source.mode < 0 ? 4 : source.mode;
                    if(primitive.topology != 0 && primitive.topology != 1 && primitive.topology != 3 && primitive.topology != 4 && primitive.topology != 5)
                        throw std::runtime_error("Unsupported glTF primitive mode " + std::to_string(primitive.topology));
                    auto position = source.attributes.find("POSITION");
                    if(position == source.attributes.end())
                        throw std::runtime_error("glTF primitive has no POSITION");
                    const Accessor positions(model, position->second);
                    if(positions.components != 3 || positions.info.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || positions.info.normalized || positions.info.count > UINT32_MAX / sizeof(ModelVertex))
                        throw std::runtime_error("Invalid glTF POSITION accessor");
                    primitive.vertices.resize(positions.info.count);
                    for(size_t i = 0; i < primitive.vertices.size(); ++i)
                        for(size_t c = 0; c < 3; ++c) primitive.vertices[i].position[c] = positions.value(i, c);
                    auto attribute = [&](const char* name, size_t offset, size_t minimum, size_t maximum) {
                        auto found = source.attributes.find(name);
                        if(found == source.attributes.end())
                            return;
                        const Accessor accessor(model, found->second);
                        if(accessor.info.count != primitive.vertices.size() || accessor.components < minimum || accessor.components > maximum)
                            throw std::runtime_error(std::string("Invalid glTF ") + name);
                        const bool floating = accessor.info.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && !accessor.info.normalized;
                        const bool normalized = accessor.info.normalized && (accessor.info.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || accessor.info.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);
                        if(!floating && (std::string_view(name) == "NORMAL" || !normalized))
                            throw std::runtime_error(std::string("Invalid glTF component type for ") + name);
                        for(size_t i = 0; i < primitive.vertices.size(); ++i)
                            for(size_t c = 0; c < accessor.components; ++c) {
                                const float value = accessor.value(i, c);
                                std::memcpy(reinterpret_cast<uint8_t*>(&primitive.vertices[i]) + offset + c * sizeof(float), &value, sizeof(value));
                            }
                    };
                    attribute("NORMAL", offsetof(ModelVertex, normal), 3, 3);
                    attribute("TEXCOORD_0", offsetof(ModelVertex, uv), 2, 2);
                    attribute("COLOR_0", offsetof(ModelVertex, color), 3, 4);
                    if(source.indices >= 0) {
                        const Accessor indices(model, source.indices);
                        primitive.index32 = indices.info.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                        const size_t size = primitive.index32 ? 4 : 2;
                        if(indices.info.count > UINT32_MAX / size)
                            throw std::runtime_error("glTF index buffer too large");
                        primitive.index_count = uint32_t(indices.info.count);
                        primitive.indices.resize(indices.info.count * size);
                        for(size_t i = 0; i < indices.info.count; ++i) {
                            const auto value = indices.index(i);
                            if(value >= primitive.vertices.size())
                                throw std::runtime_error("glTF index references a missing vertex");
                            std::memcpy(primitive.indices.data() + i * size, &value, size);
                        }
                    }
                    DirectX::XMStoreFloat4x4(&primitive.local, transform * DirectX::XMMatrixScaling(1, 1, -1));
                    if(source.material >= 0) {
                        auto const& material = at(model.materials, source.material, "material");
                        auto const& factor = material.pbrMetallicRoughness.baseColorFactor;
                        if(factor.size() != 4)
                            throw std::runtime_error("Invalid glTF base color");
                        primitive.base_color = { float(factor[0]), float(factor[1]), float(factor[2]), float(factor[3]) };
                        primitive.alpha_mode = material.alphaMode == "MASK" ? 1u : (material.alphaMode == "BLEND" ? 2u : 0u);
                        primitive.alpha_cutoff = float(material.alphaCutoff);
                        primitive.double_sided = material.doubleSided;
                        const auto texture = material.pbrMetallicRoughness.baseColorTexture.index;
                        if(texture >= 0) {
                            auto const& ref = at(model.textures, texture, "texture");
                            at(result.images, ref.source, "image");
                            primitive.image = ref.source;
                            if(ref.sampler >= 0)
                                at(result.samplers, ref.sampler, "sampler");
                            primitive.sampler = ref.sampler;
                        }
                    }
                    result.primitives.push_back(std::move(primitive));
                }
            for(int child : node.children) visit(child, transform);
            visiting[size_t(index)] = false;
        };
        auto const& scene = at(model.scenes, model.defaultScene < 0 ? 0 : model.defaultScene, "scene");
        for(int node : scene.nodes) visit(node, DirectX::XMMatrixIdentity());
        return result;
    }
}
