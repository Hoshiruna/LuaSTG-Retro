#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace core::http
{
    struct Timeouts
    {
        // Milliseconds; zero and -1 disable the corresponding deadline.
        int32_t resolve{};
        int32_t connect{};
        int32_t send{};
        int32_t receive{};
    };

    struct Request
    {
        std::string url;
        std::string method{ "GET" };
        std::unordered_map<std::string, std::string> headers;
        std::string body;
        Timeouts timeouts;
    };

    struct Response
    {
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    // Errors are reported as exceptions. HTTP error responses retain their body.
    void validateUrl(std::string_view url);
    Response execute(const Request& request);
}
