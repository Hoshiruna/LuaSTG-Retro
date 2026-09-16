#include "core/HttpClient.hpp"
#include <curl/curl.h>
#include <array>
#include <chrono>
#include <exception>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>

namespace core::http
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        using Easy = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
        using Multi = std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)>;
        using Url = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>;
        using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;

        void check(const CURLcode code)
        {
            if(code != CURLE_OK) {
                throw std::runtime_error(curl_easy_strerror(code));
            }
        }

        void check(const CURLMcode code)
        {
            if(code != CURLM_OK) {
                throw std::runtime_error(curl_multi_strerror(code));
            }
        }

        void initialize()
        {
            struct Runtime
            {
                Runtime() { check(curl_global_init(CURL_GLOBAL_DEFAULT)); }
                ~Runtime() { curl_global_cleanup(); }
            };
            static const Runtime runtime;
        }

        enum class Phase
        {
            resolve,
            connect,
            send,
            receive,
        };

        struct Transfer
        {
            const Request& request;
            Response response;
            Phase phase{ Phase::resolve };
            Clock::time_point activity{ Clock::now() };
            curl_off_t uploaded{};
            curl_off_t upload_size{};
            std::exception_ptr callback_error;
            bool interim_headers{};

            explicit Transfer(const Request& input)
                : request(input)
            {
            }

            void enter(const Phase next)
            {
                phase = next;
                activity = Clock::now();
            }

            void checkTimeout() const
            {
                int32_t timeout{};
                const char* name{};
                switch(phase) {
                    case Phase::resolve:
                        timeout = request.timeouts.resolve;
                        name = "resolve";
                        break;
                    case Phase::connect:
                        timeout = request.timeouts.connect;
                        name = "connect";
                        break;
                    case Phase::send:
                        timeout = request.timeouts.send;
                        name = "send";
                        break;
                    case Phase::receive:
                        timeout = request.timeouts.receive;
                        name = "receive";
                        break;
                }
                if(timeout > 0 && Clock::now() - activity >= std::chrono::milliseconds(timeout)) {
                    throw std::runtime_error(std::string("HTTP ") + name + " timeout");
                }
            }

            static int resolving(void*, void*, void* const userdata) noexcept
            {
                static_cast<Transfer*>(userdata)->enter(Phase::resolve);
                return 0;
            }

            static int connecting(void* const userdata, curl_socket_t, curlsocktype) noexcept
            {
                auto& self = *static_cast<Transfer*>(userdata);
                if(self.phase == Phase::resolve) {
                    self.enter(Phase::connect);
                }
                return CURL_SOCKOPT_OK;
            }

            static int ready(void* const userdata, char*, char*, int, int) noexcept
            {
                auto& self = *static_cast<Transfer*>(userdata);
                self.uploaded = 0;
                self.upload_size = 0;
                self.enter(Phase::send);
                return CURL_PREREQFUNC_OK;
            }

            static int progress(void* const userdata, curl_off_t, curl_off_t, const curl_off_t upload_size, const curl_off_t uploaded) noexcept
            {
                auto& self = *static_cast<Transfer*>(userdata);
                self.upload_size = upload_size;
                if(self.phase == Phase::send && uploaded != self.uploaded) {
                    self.uploaded = uploaded;
                    self.activity = Clock::now();
                }
                return 0;
            }

            static size_t body(char* const data, const size_t size, const size_t count, void* const userdata) noexcept
            {
                auto& self = *static_cast<Transfer*>(userdata);
                try {
                    self.response.body.append(data, size * count);
                    self.enter(Phase::receive);
                    return size * count;
                } catch(...) {
                    self.callback_error = std::current_exception();
                    return CURL_WRITEFUNC_ERROR;
                }
            }

            static size_t header(char* const data, const size_t size, const size_t count, void* const userdata) noexcept
            {
                auto& self = *static_cast<Transfer*>(userdata);
                try {
                    const std::string_view line(data, size * count);
                    if(line.starts_with("HTTP/")) {
                        const auto separator = line.find(' ');
                        self.interim_headers = separator != std::string_view::npos && separator + 1 < line.size() && line[separator + 1] == '1';
                        self.response.headers.clear();
                        self.response.body.clear();
                    }
                    if(!self.interim_headers) {
                        self.enter(Phase::receive);
                        const auto colon = line.find(':');
                        if(colon != std::string_view::npos) {
                            auto value = line.substr(colon + 1);
                            const auto first = value.find_first_not_of(" \t");
                            const auto last = value.find_last_not_of(" \t\r\n");
                            value = first == std::string_view::npos || last == std::string_view::npos || last < first ? std::string_view{} : value.substr(first, last - first + 1);
                            self.response.headers.emplace(line.substr(0, colon), value);
                        }
                    }
                    return size * count;
                } catch(...) {
                    self.callback_error = std::current_exception();
                    return CURL_WRITEFUNC_ERROR;
                }
            }
        };

        struct Attachment
        {
            CURLM* multi;
            CURL* easy;
            ~Attachment() { curl_multi_remove_handle(multi, easy); }
        };
    }

    void validateUrl(const std::string_view url)
    {
        initialize();
        if(url.find('\0') != std::string_view::npos) {
            throw std::invalid_argument("invalid url");
        }
        const Url parsed(curl_url(), curl_url_cleanup);
        if(!parsed) {
            throw std::bad_alloc();
        }
        const std::string terminated(url);
        const auto result = curl_url_set(parsed.get(), CURLUPART_URL, terminated.c_str(), 0);
        if(result != CURLUE_OK) {
            throw std::invalid_argument(curl_url_strerror(result));
        }
        char* scheme{};
        const auto scheme_result = curl_url_get(parsed.get(), CURLUPART_SCHEME, &scheme, 0);
        const std::unique_ptr<char, decltype(&curl_free)> owned_scheme(scheme, curl_free);
        if(scheme_result != CURLUE_OK || (std::string_view(scheme) != "http" && std::string_view(scheme) != "https")) {
            throw std::invalid_argument("unsupported scheme");
        }
    }

    Response execute(const Request& request)
    {
        validateUrl(request.url);
        for(const auto timeout : { request.timeouts.resolve, request.timeouts.connect, request.timeouts.send, request.timeouts.receive }) {
            if(timeout < -1) {
                throw std::invalid_argument("HTTP timeout must be -1 or greater");
            }
        }
        Transfer transfer(request);
        std::array<char, CURL_ERROR_SIZE> error{};
        Headers headers(nullptr, curl_slist_free_all);
        const Easy easy(curl_easy_init(), curl_easy_cleanup);
        const Multi multi(curl_multi_init(), curl_multi_cleanup);
        if(!easy || !multi) {
            throw std::bad_alloc();
        }
        for(const auto& [name, value] : request.headers) {
            if(name.empty() || name.find_first_of(":\r\n") != std::string::npos || value.find_first_of("\r\n") != std::string::npos || name.find('\0') != std::string::npos || value.find('\0') != std::string::npos) {
                throw std::invalid_argument("invalid HTTP header");
            }
            const auto line = name + (value.empty() ? ";" : ": " + value);
            auto* const appended = curl_slist_append(headers.get(), line.c_str());
            if(appended == nullptr) {
                throw std::bad_alloc();
            }
            headers.release();
            headers.reset(appended);
        }
        check(curl_easy_setopt(easy.get(), CURLOPT_ERRORBUFFER, error.data()));
        check(curl_easy_setopt(easy.get(), CURLOPT_URL, request.url.c_str()));
        check(curl_easy_setopt(easy.get(), CURLOPT_PROTOCOLS_STR, "http,https"));
        check(curl_easy_setopt(easy.get(), CURLOPT_REDIR_PROTOCOLS_STR, "http,https"));
        check(curl_easy_setopt(easy.get(), CURLOPT_FOLLOWLOCATION, CURLFOLLOW_OBEYCODE));
        check(curl_easy_setopt(easy.get(), CURLOPT_MAXREDIRS, 10L));
        check(curl_easy_setopt(easy.get(), CURLOPT_NOSIGNAL, 1L));
        // The multi loop enforces separate phase deadlines, including DNS.
        check(curl_easy_setopt(easy.get(), CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::numeric_limits<int32_t>::max())));
        check(curl_easy_setopt(easy.get(), CURLOPT_HTTPHEADER, headers.get()));
        if(request.method == "HEAD") {
            check(curl_easy_setopt(easy.get(), CURLOPT_NOBODY, 1L));
        } else if(request.method == "POST" || request.method == "PUT" || request.method == "PATCH") {
            check(curl_easy_setopt(easy.get(), CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size())));
            check(curl_easy_setopt(easy.get(), CURLOPT_POSTFIELDS, request.body.c_str()));
        }
        if(request.method != "POST" && request.method != "GET" && request.method != "HEAD") {
            check(curl_easy_setopt(easy.get(), CURLOPT_CUSTOMREQUEST, request.method.c_str()));
        }
        check(curl_easy_setopt(easy.get(), CURLOPT_RESOLVER_START_FUNCTION, &Transfer::resolving));
        check(curl_easy_setopt(easy.get(), CURLOPT_RESOLVER_START_DATA, &transfer));
        check(curl_easy_setopt(easy.get(), CURLOPT_SOCKOPTFUNCTION, &Transfer::connecting));
        check(curl_easy_setopt(easy.get(), CURLOPT_SOCKOPTDATA, &transfer));
        check(curl_easy_setopt(easy.get(), CURLOPT_PREREQFUNCTION, &Transfer::ready));
        check(curl_easy_setopt(easy.get(), CURLOPT_PREREQDATA, &transfer));
        check(curl_easy_setopt(easy.get(), CURLOPT_NOPROGRESS, 0L));
        check(curl_easy_setopt(easy.get(), CURLOPT_XFERINFOFUNCTION, &Transfer::progress));
        check(curl_easy_setopt(easy.get(), CURLOPT_XFERINFODATA, &transfer));
        check(curl_easy_setopt(easy.get(), CURLOPT_WRITEFUNCTION, &Transfer::body));
        check(curl_easy_setopt(easy.get(), CURLOPT_WRITEDATA, &transfer));
        check(curl_easy_setopt(easy.get(), CURLOPT_HEADERFUNCTION, &Transfer::header));
        check(curl_easy_setopt(easy.get(), CURLOPT_HEADERDATA, &transfer));
        check(curl_multi_add_handle(multi.get(), easy.get()));
        const Attachment attached{ multi.get(), easy.get() };
        int running{};
        do {
            check(curl_multi_perform(multi.get(), &running));
            if(transfer.callback_error) {
                std::rethrow_exception(transfer.callback_error);
            }
            if(running == 0) {
                break;
            }
            if(transfer.phase == Phase::send) {
                long sent_headers{};
                check(curl_easy_getinfo(easy.get(), CURLINFO_REQUEST_SIZE, &sent_headers));
                if(sent_headers > 0 && transfer.uploaded >= transfer.upload_size) {
                    transfer.enter(Phase::receive);
                }
            }
            transfer.checkTimeout();
            check(curl_multi_poll(multi.get(), nullptr, 0, 25, nullptr));
        } while(running != 0);
        int remaining{};
        auto* const message = curl_multi_info_read(multi.get(), &remaining);
        if(message == nullptr || message->msg != CURLMSG_DONE) {
            throw std::runtime_error("HTTP transfer completed without a result");
        }
        if(message->data.result != CURLE_OK) {
            throw std::runtime_error(error[0] != '\0' ? error.data() : curl_easy_strerror(message->data.result));
        }
        return std::move(transfer.response);
    }
}
