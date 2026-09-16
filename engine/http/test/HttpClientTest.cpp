#include "core/HttpClient.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <cstdlib>
#include <stdexcept>

namespace
{
    class HttpClientTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            const char* const url = std::getenv("LUASTG_HTTP_TEST_URL");
            if(url == nullptr) {
                GTEST_SKIP() << "Run through test/run_integration.py to start the loopback server";
            }
            base = url;
        }

        core::http::Request request(const std::string& path)
        {
            core::http::Request value;
            value.url = base + path;
            value.timeouts = { 1000, 1000, 2000, 2000 };
            return value;
        }

        std::string base;
    };

    TEST_F(HttpClientTest, PreservesMethodsPortQueryHeadersAndBinaryBodies)
    {
        for(const auto* method : { "GET", "HEAD", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "TRACE", "CONNECT" }) {
            auto input = request("/echo?q=a%20b&value=%25");
            input.method = method;
            input.headers["X-Test"] = "request value";
            if(input.method == "POST" || input.method == "PUT" || input.method == "PATCH") {
                input.body.assign("hello\0world", 11);
            }
            const auto output = core::http::execute(input);
            EXPECT_EQ(output.headers.at("X-Method"), method);
            EXPECT_EQ(output.headers.at("X-Path"), "/echo?q=a%20b&value=%25");
            EXPECT_EQ(output.headers.at("X-Test"), "request value");
            EXPECT_EQ(output.body, input.body);
            EXPECT_EQ(core::http::execute(input).body, input.body);
        }
    }

    TEST_F(HttpClientTest, FollowsRedirectsAndKeepsHttpErrorResponses)
    {
        const auto redirected = core::http::execute(request("/redirect"));
        EXPECT_EQ(redirected.headers.at("X-Path"), "/echo?redirected=1");
        EXPECT_FALSE(redirected.headers.contains("Location"));
        const auto missing = core::http::execute(request("/missing"));
        EXPECT_EQ(missing.body, "missing");
    }

    TEST_F(HttpClientTest, TimesOutWaitingForHeadersAndBody)
    {
        for(const auto* path : { "/slow-headers", "/slow-body" }) {
            auto input = request(path);
            input.timeouts.receive = 50;
            const auto start = std::chrono::steady_clock::now();
            try {
                (void)core::http::execute(input);
                FAIL() << "Expected receive timeout for " << path;
            } catch(const std::runtime_error& error) {
                EXPECT_NE(std::string(error.what()).find("receive timeout"), std::string::npos);
            }
            EXPECT_LT(std::chrono::steady_clock::now() - start, std::chrono::seconds(2));
        }
    }

    TEST_F(HttpClientTest, TimesOutBlockedUpload)
    {
        auto input = request("/no-read");
        input.method = "POST";
        input.body.assign(16 * 1024 * 1024, 'x');
        input.headers["Expect"] = "";
        input.timeouts.send = 50;
        try {
            (void)core::http::execute(input);
            FAIL() << "Expected send timeout";
        } catch(const std::runtime_error& error) {
            EXPECT_NE(std::string(error.what()).find("send timeout"), std::string::npos);
        }
    }

    TEST_F(HttpClientTest, RejectsUnsupportedUrlsAndInvalidTimeouts)
    {
        EXPECT_THROW(core::http::validateUrl("file:///tmp/example"), std::invalid_argument);
        EXPECT_THROW(core::http::validateUrl("not a url"), std::invalid_argument);
        auto input = request("/echo");
        input.timeouts.connect = -2;
        EXPECT_THROW((void)core::http::execute(input), std::invalid_argument);
    }
}
