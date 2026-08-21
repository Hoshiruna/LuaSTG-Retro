#include "utf8.hpp"

#include <simdutf.h>

#include <cassert>

namespace utf8
{
    static_assert(CHAR_BIT == 8);
    static_assert(sizeof(wchar_t) == sizeof(char16_t) || sizeof(wchar_t) == sizeof(char32_t));

    std::string to_string(std::wstring_view const& str)
    {
        std::string buffer;
        if(str.empty()) {
            return buffer;
        }
        size_t length{};
        if constexpr(sizeof(wchar_t) == sizeof(char16_t)) {
            if(!simdutf::validate_utf16(reinterpret_cast<char16_t const*>(str.data()), str.size())) {
                assert(false);
                return buffer;
            }
            length = simdutf::utf8_length_from_utf16(
                reinterpret_cast<char16_t const*>(str.data()), str.size());
        } else {
            if(!simdutf::validate_utf32(reinterpret_cast<char32_t const*>(str.data()), str.size())) {
                assert(false);
                return buffer;
            }
            length = simdutf::utf8_length_from_utf32(
                reinterpret_cast<char32_t const*>(str.data()), str.size());
        }
        buffer.resize(length);

        size_t result{};
        if constexpr(sizeof(wchar_t) == sizeof(char16_t)) {
            result = simdutf::convert_utf16_to_utf8(
                reinterpret_cast<char16_t const*>(str.data()), str.size(), buffer.data());
        } else {
            result = simdutf::convert_utf32_to_utf8(
                reinterpret_cast<char32_t const*>(str.data()), str.size(), buffer.data());
        }
        if(result != length) {
            assert(false);
            buffer.clear();
        }
        return buffer;
    }
    std::wstring to_wstring(std::string_view const& str)
    {
        std::wstring buffer;
        if(str.empty()) {
            return buffer;
        }
        if(!simdutf::validate_utf8(str.data(), str.size())) {
            assert(false);
            return buffer;
        }
        size_t length{};
        if constexpr(sizeof(wchar_t) == sizeof(char16_t)) {
            length = simdutf::utf16_length_from_utf8(str.data(), str.size());
        } else {
            length = simdutf::utf32_length_from_utf8(str.data(), str.size());
        }
        buffer.resize(length);

        size_t result{};
        if constexpr(sizeof(wchar_t) == sizeof(char16_t)) {
            result = simdutf::convert_utf8_to_utf16(
                str.data(), str.size(), reinterpret_cast<char16_t*>(buffer.data()));
        } else {
            result = simdutf::convert_utf8_to_utf32(
                str.data(), str.size(), reinterpret_cast<char32_t*>(buffer.data()));
        }
        if(result != length) {
            assert(false);
            buffer.clear();
        }
        return buffer;
    }
}
