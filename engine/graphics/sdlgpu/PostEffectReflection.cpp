#include "PostEffectReflection.hpp"
#include <spirv_cross_c.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>

namespace core::Graphics::SDLGPU
{
    namespace
    {
        class Reflection
        {
        public:
            explicit Reflection(std::vector<uint32_t>& words) : m_words(words)
            {
                spvc_context raw{};
                if(spvc_context_create(&raw) != SPVC_SUCCESS)
                    throw std::runtime_error("Create SPIRV-Cross reflection context");
                m_context.reset(raw);
                spvc_parsed_ir ir{};
                check(spvc_context_parse_spirv(raw, words.data(), words.size(), &ir));
                check(spvc_context_create_compiler(raw, SPVC_BACKEND_NONE, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler));
                check(spvc_compiler_create_shader_resources(compiler, &m_resources));
                spvc_set active_variables{};
                check(spvc_compiler_get_active_interface_variables(compiler, &active_variables));
                check(spvc_compiler_create_shader_resources_for_active_variables(compiler, &m_active_resources, active_variables));
            }
            void check(spvc_result result) const
            {
                if(result != SPVC_SUCCESS)
                    throw std::runtime_error(std::string("Reflect post-effect: ") + spvc_context_get_last_error_string(m_context.get()));
            }
            std::span<const spvc_reflected_resource> resources(spvc_resource_type type) const
            {
                const spvc_reflected_resource* data{};
                size_t count{};
                check(spvc_resources_get_resource_list_for_type(m_resources, type, &data, &count));
                return { data, count };
            }
            void checkBuiltins() const
            {
                const spvc_reflected_builtin_resource* data{};
                size_t count{};
                check(spvc_resources_get_builtin_resource_list_for_type(m_resources, SPVC_BUILTIN_RESOURCE_TYPE_STAGE_OUTPUT, &data, &count));
                if(count)
                    throw std::runtime_error("Post-effects cannot write depth, coverage, or other built-in outputs");
                check(spvc_resources_get_builtin_resource_list_for_type(m_resources, SPVC_BUILTIN_RESOURCE_TYPE_STAGE_INPUT, &data, &count));
                for(size_t i = 0; i < count; ++i) {
                    if(data[i].builtin != SpvBuiltInFragCoord)
                        throw std::runtime_error("Post-effects support only SV_Position as a built-in input");
                }
            }
            bool activeBuffer(SpvId id) const
            {
                const spvc_reflected_resource* data{};
                size_t count{};
                check(spvc_resources_get_resource_list_for_type(m_active_resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &data, &count));
                for(size_t i = 0; i < count; ++i) {
                    if(data[i].id == id)
                        return true;
                }
                return false;
            }
            uint32_t slot(spvc_reflected_resource const& resource) const
            {
                if(spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationDescriptorSet) != 0)
                    throw std::runtime_error(std::string(resource.name) + ": legacy shaders must use register space 0");
                auto type = spvc_compiler_get_type_handle(compiler, resource.type_id);
                if(spvc_type_get_num_array_dimensions(type))
                    throw std::runtime_error(std::string(resource.name) + ": resource arrays are not supported");
                return spvc_compiler_get_decoration(compiler, resource.id, SpvDecorationBinding);
            }
            void decorate(SpvId id, SpvDecoration decoration, uint32_t value)
            {
                uint32_t offset{};
                if(!spvc_compiler_get_binary_offset_for_decoration(compiler, id, decoration, &offset) || offset >= m_words.size())
                    throw std::runtime_error("Post-effect is missing a required SPIR-V decoration");
                m_words[offset] = value;
            }
            spvc_compiler compiler{};

        private:
            std::unique_ptr<spvc_context_s, decltype(&spvc_context_destroy)> m_context{ nullptr, spvc_context_destroy };
            spvc_resources m_resources{};
            spvc_resources m_active_resources{};
            std::vector<uint32_t>& m_words;
        };
    }

    EffectLayout normalizePostEffect(std::vector<uint32_t>& words)
    {
        Reflection reflection(words);
        auto compiler = reflection.compiler;
        reflection.checkBuiltins();
        for(auto type : { SPVC_RESOURCE_TYPE_STORAGE_BUFFER, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, SPVC_RESOURCE_TYPE_SUBPASS_INPUT, SPVC_RESOURCE_TYPE_ATOMIC_COUNTER, SPVC_RESOURCE_TYPE_PUSH_CONSTANT }) {
            for(auto const& resource : reflection.resources(type))
                throw std::runtime_error(std::string(resource.name) + ": unsupported post-effect resource type");
        }

        EffectLayout layout;
        std::map<uint32_t, spvc_reflected_resource> images, samplers, buffers;
        auto collect = [&](spvc_resource_type type, auto& output) {
            for(auto const& resource : reflection.resources(type)) {
                if(!output.emplace(reflection.slot(resource), resource).second)
                    throw std::runtime_error(std::string(resource.name) + ": duplicate resource register");
            }
        };
        collect(SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, images);
        collect(SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, samplers);
        collect(SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, buffers);
        if(images.size() > 16 || buffers.size() > 4)
            throw std::runtime_error("Post-effect exceeds SDL GPU limits (16 textures, 4 uniform buffers)");
        if(images.size() != samplers.size())
            throw std::runtime_error("Every post-effect texture needs a sampler at the same legacy register");
        for(auto const& [legacy, resource] : images) {
            auto type = spvc_compiler_get_type_handle(compiler, resource.type_id);
            if(spvc_type_get_image_dimension(type) != SpvDim2D || spvc_type_get_image_arrayed(type) || spvc_type_get_image_multisampled(type) || spvc_type_get_image_is_depth(type))
                throw std::runtime_error(std::string(resource.name) + ": only ordinary Texture2D resources are supported");
            auto sample_type = spvc_compiler_get_type_handle(compiler, spvc_type_get_image_sampled_type(type));
            if(spvc_type_get_basetype(sample_type) != SPVC_BASETYPE_FP32)
                throw std::runtime_error(std::string(resource.name) + ": post-effects require floating-point Texture2D samples");
            if(!samplers.contains(legacy))
                throw std::runtime_error(std::string(resource.name) + ": texture and sampler registers do not match");
            const auto slot = static_cast<uint32_t>(layout.textures.size());
            layout.textures.push_back({ resource.name, legacy, slot, samplers.at(legacy).name });
            for(auto id : { resource.id, samplers.at(legacy).id }) {
                reflection.decorate(id, SpvDecorationDescriptorSet, 2);
                reflection.decorate(id, SpvDecorationBinding, slot);
            }
        }
        std::set<std::string> names;
        std::vector<std::pair<uint32_t, spvc_reflected_resource>> ordered_buffers(buffers.begin(), buffers.end());
        // Shadercross counts active buffers. Keep those slots dense while retaining
        // inactive declarations for existing Lua setters and layout reflection.
        std::stable_sort(ordered_buffers.begin(), ordered_buffers.end(), [&](auto const& a, auto const& b) {
            return reflection.activeBuffer(a.second.id) > reflection.activeBuffer(b.second.id);
        });
        for(auto const& [legacy, resource] : ordered_buffers) {
            EffectBuffer buffer;
            buffer.active = reflection.activeBuffer(resource.id);
            buffer.name = spvc_compiler_get_name(compiler, resource.id);
            if(buffer.name.empty())
                buffer.name = resource.name;
            buffer.legacy_slot = legacy;
            buffer.slot = static_cast<uint32_t>(layout.buffers.size());
            auto type = spvc_compiler_get_type_handle(compiler, resource.base_type_id);
            size_t size{};
            reflection.check(spvc_compiler_get_declared_struct_size(compiler, type, &size));
            if(size == 0 || size > 16384)
                throw std::runtime_error(buffer.name + ": constant buffer must contain 1 to 16384 bytes");
            buffer.bytes.resize((size + 15) & ~size_t(15));
            for(unsigned i = 0; i < spvc_type_get_num_member_types(type); ++i) {
                EffectVariable variable;
                variable.name = spvc_compiler_get_member_name(compiler, resource.base_type_id, i);
                if(!names.insert(variable.name).second)
                    throw std::runtime_error(variable.name + ": duplicate constant name across buffers");
                reflection.check(spvc_compiler_type_struct_member_offset(compiler, type, i, &variable.offset));
                size_t member_size{};
                reflection.check(spvc_compiler_get_declared_struct_member_size(compiler, type, i, &member_size));
                variable.size = static_cast<uint32_t>(member_size);
                auto member = spvc_compiler_get_type_handle(compiler, spvc_type_get_member_type(type, i));
                if(spvc_type_get_basetype(member) == SPVC_BASETYPE_FP32 && spvc_type_get_columns(member) == 1 && !spvc_type_get_num_array_dimensions(member))
                    variable.components = spvc_type_get_vector_size(member);
                if(variable.offset + member_size > buffer.bytes.size())
                    throw std::runtime_error(variable.name + ": invalid constant offset");
                buffer.variables.push_back(std::move(variable));
            }
            reflection.decorate(resource.id, SpvDecorationDescriptorSet, 3);
            reflection.decorate(resource.id, SpvDecorationBinding, buffer.slot);
            layout.buffers.push_back(std::move(buffer));
        }
        for(auto const& input : reflection.resources(SPVC_RESOURCE_TYPE_STAGE_INPUT)) {
            std::string semantic = spvc_compiler_get_decoration_string(compiler, input.id, SpvDecorationHlslSemanticGOOGLE);
            std::transform(semantic.begin(), semantic.end(), semantic.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            auto type = spvc_compiler_get_type_handle(compiler, input.type_id);
            const bool uv = semantic == "TEXCOORD0" || semantic == "TEXCOORD";
            const bool color = semantic == "COLOR0" || semantic == "COLOR";
            if((!uv && !color) || spvc_type_get_basetype(type) != SPVC_BASETYPE_FP32 || spvc_type_get_vector_size(type) != (uv ? 2u : 4u) || spvc_type_get_columns(type) != 1)
                throw std::runtime_error(std::string(input.name) + ": expected float2 TEXCOORD0 or float4 COLOR0 input");
            reflection.decorate(input.id, SpvDecorationLocation, uv ? 0 : 1);
        }
        const auto outputs = reflection.resources(SPVC_RESOURCE_TYPE_STAGE_OUTPUT);
        if(outputs.size() != 1)
            throw std::runtime_error("Post-effects require one float4 color output");
        for(auto const& output : outputs) {
            auto type = spvc_compiler_get_type_handle(compiler, output.type_id);
            if(spvc_compiler_get_decoration(compiler, output.id, SpvDecorationLocation) != 0 || spvc_type_get_basetype(type) != SPVC_BASETYPE_FP32 || spvc_type_get_vector_size(type) != 4)
                throw std::runtime_error("Post-effects require float4 SV_Target0");
        }
        // Re-parse the edited module to check the actual buffer layout sent to SDL.
        Reflection normalized(words);
        for(auto const& resource : normalized.resources(SPVC_RESOURCE_TYPE_UNIFORM_BUFFER)) {
            const auto slot = spvc_compiler_get_decoration(normalized.compiler, resource.id, SpvDecorationBinding);
            if(slot >= layout.buffers.size())
                throw std::runtime_error("Invalid normalized constant binding");
            auto type = spvc_compiler_get_type_handle(normalized.compiler, resource.base_type_id);
            for(unsigned i = 0; i < layout.buffers[slot].variables.size(); ++i) {
                unsigned offset{};
                normalized.check(spvc_compiler_type_struct_member_offset(normalized.compiler, type, i, &offset));
                if(offset != layout.buffers[slot].variables[i].offset || layout.buffers[slot].variables[i].name != spvc_compiler_get_member_name(normalized.compiler, resource.base_type_id, i))
                    throw std::runtime_error("Constant layout changed during normalization");
            }
        }
        return layout;
    }
}
