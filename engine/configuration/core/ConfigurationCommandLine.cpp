#include "core/Configuration.hpp"
#include "core/CommandLineArguments.hpp"
#include <charconv>
#include <format>
#include <optional>
#include <type_traits>

using namespace std::string_view_literals;

namespace core
{
    static std::optional<bool> to_boolean(std::string_view const& s)
    {
        if(s == "true"sv) {
            return { true };
        } else if(s == "false"sv) {
            return { false };
        } else {
            return std::nullopt;
        }
    }

    template<typename T>
    static std::optional<T> to_unsigned_integer(std::string_view const& s)
    {
        static_assert(std::is_unsigned<T>::value);
        T value{};
        auto const result = std::from_chars(s.data(), s.data() + s.size(), value);
        if(result.ec == std::errc{}) {
            return { value };
        } else {
            return std::nullopt;
        }
    }

    bool ConfigurationLoader::loadFromCommandLineArguments()
    {
        auto const args{ CommandLineArguments::copy() };
        auto write_arg_error = [this](std::string_view const& raw_arg) -> void {
            messages.emplace_back(std::format("invalid command line argument '{}'", raw_arg));
        };
        auto write_message = [this](std::string_view const& raw_arg, std::string_view const& message) -> void {
            messages.emplace_back(std::format("invalid command line argument '{}': {}", raw_arg, message));
        };
        for(size_t i = 0; i < args.size(); i += 1) {
            std::string_view const raw_arg(args[i]);
            std::string_view arg(args[i]);

            constexpr auto double_dash = "--"sv;
            constexpr auto separator = "."sv;
            constexpr auto assignment = "="sv;

            if(!arg.starts_with(double_dash)) {
                continue;
            }
            arg = arg.substr(double_dash.size());

#define access_parent_field(VAR, EXP)           \
    constexpr auto key_##VAR = "" #VAR ""sv;    \
    if(arg.starts_with(key_##VAR)) {            \
        arg = arg.substr(key_##VAR.size());     \
        if(arg.starts_with(separator)) {        \
            arg = arg.substr(separator.size()); \
            EXP;                                \
        }                                       \
    }

#define access_field(VAR, EXP)                   \
    constexpr auto key_##VAR = "" #VAR ""sv;     \
    if(arg.starts_with(key_##VAR)) {             \
        arg = arg.substr(key_##VAR.size());      \
        if(arg.starts_with(assignment)) {        \
            arg = arg.substr(assignment.size()); \
            EXP;                                 \
        }                                        \
    }

            access_parent_field(graphics_system,
                {
                    access_field(width, if(auto const value = to_unsigned_integer<uint32_t>(arg); value) {
							if (value.value() == 0) {
								write_message(raw_arg, "width must greater than 0"sv);
								return false;
							}
							graphics_system.setWidth(value.value()); } else {
							write_arg_error(raw_arg);
							return false; });
                    access_field(height, if(auto const value = to_unsigned_integer<uint32_t>(arg); value) {
							if (value.value() == 0) {
								write_message(raw_arg, "height must greater than 0"sv);
								return false;
							}
							graphics_system.setHeight(value.value()); } else {
							write_arg_error(raw_arg);
							return false; });
                    access_field(fullscreen, if(auto const value = to_boolean(arg); value) { graphics_system.setFullscreen(value.value()); } else {
							write_arg_error(raw_arg);
							return false; });
                    access_field(vsync, if(auto const value = to_boolean(arg); value) { graphics_system.setVsync(value.value()); } else {
							write_arg_error(raw_arg);
							return false; });
                });

#undef access_parent_field
#undef access_field
        }
        return true;
    }

}
